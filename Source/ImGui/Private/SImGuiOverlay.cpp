#include "SImGuiOverlay.h"

#ifndef IMGUI_DISABLE

#include <Framework/Application/SlateApplication.h>
#include <Framework/Application/SlateUser.h>
#include <Misc/DefinePrivateMemberPtr.h>
#include <Widgets/SViewport.h>

THIRD_PARTY_INCLUDES_START
#include <imgui_internal.h>
THIRD_PARTY_INCLUDES_END

#include "ImGuiContext.h"

FImGuiDrawList::FImGuiDrawList(ImDrawList* Source)
{
	VtxBuffer.swap(Source->VtxBuffer);
	IdxBuffer.swap(Source->IdxBuffer);
	CmdBuffer.swap(Source->CmdBuffer);
	Flags = Source->Flags;
}

FImGuiDrawData::FImGuiDrawData(const ImDrawData* Source)
{
	bValid = Source->Valid;

	TotalIdxCount = Source->TotalIdxCount;
	TotalVtxCount = Source->TotalVtxCount;

	ImGui::CopyArray(Source->CmdLists, DrawLists);

	DisplayPos = Source->DisplayPos;
	DisplaySize = Source->DisplaySize;
	FrameBufferScale = Source->FramebufferScale;
}

// FSlateApplication::TransformPointerEvent() is protected. Reuse its fullscreen mapping so ImGui
// pointer positions stay aligned with Slate geometry without duplicating the coordinate formula.

UE_DEFINE_PRIVATE_MEMBER_PTR(FPointerEvent(const FPointerEvent&, const TSharedPtr<SWindow>&) const,
                             GTransformPointerEvent, FSlateApplication, TransformPointerEvent);

enum class EPointerInteractionState : uint8
{
	/// Allows the owning pointer to update Dear ImGui until its contact or all tracked mouse buttons are released.
	Held,

	/// Cancels the ImGui interaction but retains pointer ownership and the
	/// consumption decision until physical release to prevent a resumed drag.
	CanceledUntilRelease
};

struct FPointerInteraction
{
	ImGuiMouseSource Source = ImGuiMouseSource_Mouse;
	uint32 UserIndex = 0;
	FInputDeviceId DeviceId = INPUTDEVICEID_NONE;
	uint32 PointerIndex = 0;
	bool bConsumePointerInput = false;
	EPointerInteractionState State = EPointerInteractionState::Held;
	TSet<FKey> PressedMouseButtons;
};

struct FPointerLocation
{
	ImGuiViewport* Viewport = nullptr;
	TSharedPtr<SImGuiOverlay> Overlay;
	FVector2f ScreenPosition = FVector2f::ZeroVector;
	FVector2f ImGuiPosition = FVector2f::ZeroVector;
	bool bOverOverlay = false;
};

class FImGuiInputProcessor : public IInputProcessor
{
private:
	SImGuiOverlay* Owner = nullptr;
	ImGuiID NewFrameContextHookId = 0;
	TOptional<FPointerInteraction> PointerInteraction;
	bool bCancelPointerInteractionRequested = false;

public:
	explicit FImGuiInputProcessor(SImGuiOverlay* InOwner)
	{
		Owner = InOwner;

		FSlateApplication::Get().OnApplicationActivationStateChanged().AddRaw(this, &FImGuiInputProcessor::OnApplicationActivationChanged);
		FSlateApplication::Get().OnFocusChanging().AddRaw(this, &FImGuiInputProcessor::OnFocusChanging);

		ImGui::FScopedContext ScopedContext(Owner->GetContext());

		ImGuiContextHook ContextHook;
		ContextHook.Type = ImGuiContextHookType_NewFramePre;
		ContextHook.Callback = &FImGuiInputProcessor::OnNewFrame;
		ContextHook.UserData = this;
		NewFrameContextHookId = ImGui::AddContextHook(ImGui::GetCurrentContext(), &ContextHook);
	}

	virtual ~FImGuiInputProcessor() override
	{
		if (FSlateApplication::IsInitialized())
		{
			FSlateApplication::Get().OnApplicationActivationStateChanged().RemoveAll(this);
			FSlateApplication::Get().OnFocusChanging().RemoveAll(this);
		}

		ImGui::FScopedContext ScopedContext(Owner->GetContext());

		ImGui::RemoveContextHook(ImGui::GetCurrentContext(), NewFrameContextHookId);

		ClearImGuiPointerInput();
	}

	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, const TSharedRef<ICursor> SlateCursor) override
	{
		ImGui::FScopedContext ScopedContext(Owner->GetContext());

		// Focus or ImGuiIO's input acceptance can change without another pointer event. Cancel
		// idle holds too; a stationary finger must not leave a disabled slider active indefinitely.

		CancelPointerInteractionIfRequired();

		ImGuiIO& IO = ImGui::GetIO();

		const bool bHasGamepad = (IO.BackendFlags & ImGuiBackendFlags_HasGamepad);
		if (bHasGamepad != SlateApp.IsGamepadAttached())
		{
			IO.BackendFlags ^= ImGuiBackendFlags_HasGamepad;
		}

		if (IO.WantSetMousePos)
		{
			// ImGuiIO::WantSetMousePos targets the physical mouse. During touch, ImGuiIO::MousePos belongs
			// to the contact; honoring navigation cursor requests would make the hardware cursor jump to it.

			if (!PointerInteraction.IsSet() || PointerInteraction->Source != ImGuiMouseSource_TouchScreen)
			{
				FVector2f Position = IO.MousePos;
				if (!(IO.ConfigFlags & ImGuiConfigFlags_ViewportsEnable))
				{
					// Single-viewport ImGuiIO::MousePos is overlay-relative; the Slate cursor expects screen coordinates.
					Position += Owner->GetTickSpaceGeometry().AbsolutePosition;
				}

				SlateCursor->SetPosition(Position.X, Position.Y);
			}
		}

		if (IO.WantTextInput)
		{
			const ImGuiViewport* FocusedViewport = FindImGuiViewportForUserFocus(SlateApp, SlateApp.GetUserIndexForKeyboard());
			if (FocusedViewport != nullptr)
			{
				// Select the viewport from this validated path, not Owner or the viewport under the hardware mouse.
				// Typing in a detached ImGui window must not move focus back to the main overlay, and a stale
				// ImGuiIO::WantTextInput must not take focus from an editor panel that replaced the ImGui focus path.

				const FImGuiViewportData* ViewportData = static_cast<const FImGuiViewportData*>(FocusedViewport->PlatformUserData);
				const TSharedPtr<SImGuiOverlay> Overlay = ViewportData->Overlay.Pin();

				if (Overlay.IsValid() && !Overlay->HasKeyboardFocus())
				{
					// IInputProcessor has no character callback. Focus the current viewport's overlay for
					// SImGuiOverlay::OnKeyChar(), rather than pulling a detached window's text input into Owner.
					// FImGuiInputProcessor::FindImGuiViewportForUserFocus() also preserves editor text-box focus.

					SlateApp.SetKeyboardFocus(Overlay);
				}
			}
		}
	}

	virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& Event) override
	{
		ImGui::FScopedContext ScopedContext(Owner->GetContext());

#if WITH_EDITORONLY_DATA
		if (!SlateApp.IsNormalExecution())
		{
			// Avoid accumulating input or performing keyboard cleanup while the game thread is paused in the debugger.
			return false;
		}
#endif

		if (!IsUserFocusRelevant(SlateApp, Event.GetUserIndex()))
		{
			return false;
		}

		ImGuiIO& IO = ImGui::GetIO();
		const FModifierKeysState& ModifierKeys = Event.GetModifierKeys();

		IO.AddKeyEvent(ImGui::ConvertKey(Event.GetKey()), true);
		IO.AddKeyEvent(ImGuiMod_Ctrl, ModifierKeys.IsControlDown());
		IO.AddKeyEvent(ImGuiMod_Shift, ModifierKeys.IsShiftDown());
		IO.AddKeyEvent(ImGuiMod_Alt, ModifierKeys.IsAltDown());
		IO.AddKeyEvent(ImGuiMod_Super, ModifierKeys.IsCommandDown());

		return IO.WantCaptureKeyboard;
	}

	virtual bool HandleKeyUpEvent(FSlateApplication& SlateApp, const FKeyEvent& Event) override
	{
		ImGui::FScopedContext ScopedContext(Owner->GetContext());

#if WITH_EDITORONLY_DATA
		if (!SlateApp.IsNormalExecution())
		{
			// Avoid accumulating input or performing keyboard cleanup while the game thread is paused in the debugger.
			return false;
		}
#endif

		ImGuiIO& IO = ImGui::GetIO();
		const ImGuiKey Key = ImGui::ConvertKey(Event.GetKey());
		const FModifierKeysState& ModifierKeys = Event.GetModifierKeys();

		if (IsUserFocusRelevant(SlateApp, Event.GetUserIndex()))
		{
			IO.AddKeyEvent(Key, false);
			IO.AddKeyEvent(ImGuiMod_Ctrl, ModifierKeys.IsControlDown());
			IO.AddKeyEvent(ImGuiMod_Shift, ModifierKeys.IsShiftDown());
			IO.AddKeyEvent(ImGuiMod_Alt, ModifierKeys.IsAltDown());
			IO.AddKeyEvent(ImGuiMod_Super, ModifierKeys.IsCommandDown());

			return IO.WantCaptureKeyboard;
		}

		if (ImGui::IsKeyboardKey(Key) && Event.GetUserIndex() == SlateApp.GetUserIndexForKeyboard())
		{
			// A key pressed in ImGui can be released after a click moves focus to the editor. Forward that
			// cleanup even outside the focus path; ImGuiIO::AddKeyEvent() checks queued state too, so a quick
			// exit and return before ImGui::NewFrame() cannot leave the key held when focus events cancel out.

			IO.AddKeyEvent(Key, false);

			// Outside this context, only release modifier state. Sending true from the editor's snapshot
			// would introduce a new ImGui hold without a corresponding press in its focus path.

			if (!ModifierKeys.IsControlDown())
			{
				IO.AddKeyEvent(ImGuiMod_Ctrl, false);
			}

			if (!ModifierKeys.IsShiftDown())
			{
				IO.AddKeyEvent(ImGuiMod_Shift, false);
			}

			if (!ModifierKeys.IsAltDown())
			{
				IO.AddKeyEvent(ImGuiMod_Alt, false);
			}

			if (!ModifierKeys.IsCommandDown())
			{
				IO.AddKeyEvent(ImGuiMod_Super, false);
			}
		}

		// Cleanup must remain available to the editor even while ImGuiIO::WantCaptureKeyboard is stale.
		return false;
	}

	virtual bool HandleAnalogInputEvent(FSlateApplication& SlateApp, const FAnalogInputEvent& Event) override
	{
		ImGui::FScopedContext ScopedContext(Owner->GetContext());

#if WITH_EDITORONLY_DATA
		if (!SlateApp.IsNormalExecution())
		{
			// Avoid accumulating input or performing keyboard cleanup while the game thread is paused in the debugger.
			return false;
		}
#endif

		if (!IsUserFocusRelevant(SlateApp, Event.GetUserIndex()))
		{
			return false;
		}

		ImGuiIO& IO = ImGui::GetIO();

		const float Value = Event.GetAnalogValue();
		IO.AddKeyAnalogEvent(ImGui::ConvertKey(Event.GetKey()), FMath::Abs(Value) > 0.1f, Value);

		return IO.WantCaptureKeyboard;
	}

	virtual bool HandleMouseMoveEvent(FSlateApplication& SlateApp, const FPointerEvent& Event) override
	{
		ImGui::FScopedContext ScopedContext(Owner->GetContext());

		// ImGuiIO's input acceptance or focus can change before FImGuiInputProcessor::Tick() runs.
		// Cancel before dispatching this event; otherwise the next move can keep changing a slider
		// after input is disabled. Cancellation also applies when this event itself is rejected.

		CancelPointerInteractionIfRequired();

#if WITH_EDITORONLY_DATA
		if (!SlateApp.IsNormalExecution())
		{
			// Avoid accumulating input or performing keyboard cleanup while the game thread is paused in the debugger.
			return false;
		}
#endif

		// FSlateApplication::ProcessTouchMovedEvent() also reaches this mouse-named callback. Preserve
		// the native touch classification instead of changing the input seen by other Slate handlers.

		return HandlePointerMoved(SlateApp, Event);
	}

	virtual bool HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& Event) override
	{
		ImGui::FScopedContext ScopedContext(Owner->GetContext());

		CancelPointerInteractionIfRequired();

#if WITH_EDITORONLY_DATA
		if (!SlateApp.IsNormalExecution())
		{
			// Avoid accumulating input or performing keyboard cleanup while the game thread is paused in the debugger.
			return false;
		}
#endif

		return Event.IsTouchEvent()
			       ? HandleTouchStarted(SlateApp, Event)
			       : HandleMousePressed(SlateApp, Event);
	}

	virtual bool HandleMouseButtonUpEvent(FSlateApplication& SlateApp, const FPointerEvent& Event) override
	{
		ImGui::FScopedContext ScopedContext(Owner->GetContext());

		CancelPointerInteractionIfRequired();

#if WITH_EDITORONLY_DATA
		if (!SlateApp.IsNormalExecution())
		{
			// Avoid accumulating input or performing keyboard cleanup while the game thread is paused in the debugger.
			return false;
		}
#endif

		return Event.IsTouchEvent()
			       ? HandleTouchEnded(SlateApp, Event)
			       : HandleMouseReleased(SlateApp, Event);
	}

	virtual bool HandleMouseButtonDoubleClickEvent(FSlateApplication& SlateApp, const FPointerEvent& Event) override
	{
		// ImGuiIO::AddMouseButtonEvent() already feeds Dear ImGui's click timing. Treat Slate's double-click
		// notification as the second press rather than introducing a separate activation path.

		return HandleMouseButtonDownEvent(SlateApp, Event);
	}

	virtual bool HandleMouseWheelOrGestureEvent(FSlateApplication& SlateApp, const FPointerEvent& Event, const FPointerEvent* GestureEvent) override
	{
		ImGui::FScopedContext ScopedContext(Owner->GetContext());

		CancelPointerInteractionIfRequired();

#if WITH_EDITORONLY_DATA
		if (!SlateApp.IsNormalExecution())
		{
			// Avoid accumulating input or performing keyboard cleanup while the game thread is paused in the debugger.
			return false;
		}
#endif

		// Touch gestures are outside this backend's single-contact model. Leave them available to Slate
		// instead of interpreting a touch swipe as a hardware mouse wheel at Dear ImGui's shared pointer.

		if (Event.IsTouchEvent() || (GestureEvent != nullptr && GestureEvent->IsTouchEvent()))
		{
			return false;
		}

		if (!DoesImGuiAcceptPointerInput())
		{
			return false;
		}

		return HandleMouseWheel(SlateApp, Event);
	}

private:
	void OnApplicationActivationChanged(const bool bIsActive)
	{
		ImGui::FScopedContext ScopedContext(Owner->GetContext());

		// Reactivating the editor does not necessarily reactivate this ImGui context. Its keyboard user
		// can still be focused on another panel, so do not report focus solely from application activation.

		const FSlateApplication& SlateApp = FSlateApplication::Get();
		const bool bFocusRelevant = bIsActive && IsUserFocusRelevant(SlateApp, SlateApp.GetUserIndexForKeyboard());

		ImGui::GetIO().AddFocusEvent(bFocusRelevant);

		bCancelPointerInteractionRequested = !bFocusRelevant;

		if (!bIsActive)
		{
			// Physical releases can occur while the application is inactive and never reach this processor.
			// End ownership now so returning from Alt-Tab cannot leave a slider held or reject every new touch.

			PointerInteraction.Reset();
			ClearImGuiPointerInput();
		}
	}

	void OnFocusChanging(const FFocusEvent& Event, const FWeakWidgetPath& OldWidgetPath, const TSharedPtr<SWidget>& OldWidget, const FWidgetPath& NewWidgetPath, const TSharedPtr<SWidget>& NewWidget)
	{
		if (Event.GetUser() != FSlateApplication::Get().GetUserIndexForKeyboard())
		{
			// ImGuiIO's focus is shared. Another Slate user's focus transition must not release the keyboard user's holds.
			return;
		}

		ImGui::FScopedContext ScopedContext(Owner->GetContext());

		ImGuiIO& IO = ImGui::GetIO();

		// FSlateApplication::SetUserFocus() broadcasts before installing the new path. Evaluate NewWidgetPath,
		// not the user's previous path, so clicking another panel in the same editor window counts as focus loss.

		if (FindImGuiViewportForFocusPath(NewWidgetPath) != nullptr)
		{
			// Cancel only a pending focus-loss request. An interaction already marked CanceledUntilRelease
			// stays canceled, so restoring focus cannot resume a drag while the original button is still held.

			bCancelPointerInteractionRequested = false;

			// ImGuiIO::AddFocusEvent() lets a focus loss immediately followed by a restore, as UConsole::FakeGotoState()
			// does, cancel out in ImGui::NewFrame() instead of releasing keys held by the user.

			IO.AddFocusEvent(true);
		}
		else
		{
			// Defer interaction cancellation so a temporary focus reset by UConsole::FakeGotoState() can be restored.
			// Canceling a processed hold immediately would interrupt a drag even though the user never leaves the UI.

			bCancelPointerInteractionRequested = true;

			// ImGuiIO::AddFocusEvent(false) releases processed keys even when their key-up is now routed
			// to the editor. Clear pending input first so holding a key, clicking an editor panel and
			// releasing it cannot leave that key pressed in ImGui or replay an old press when focus returns.

			IO.ClearEventsQueue();
			IO.AddFocusEvent(false);
		}
	}

	/// Defines this context's keyboard-focus boundary so unrelated editor panels are excluded and detached ImGui windows remain eligible.
	static ImGuiViewport* FindImGuiViewportForFocusPath(const FWidgetPath& FocusPath)
	{
		if (!FocusPath.IsValid())
		{
			return nullptr;
		}

		ImGuiViewport* FocusedViewport = FindImGuiViewportForWindow(FocusPath.GetDeepestWindow());
		if (FocusedViewport == nullptr)
		{
			return nullptr;
		}

#if PLATFORM_DESKTOP
		// An embedded game viewport shares its SWindow with editor panels. Match the SViewport subtree,
		// or clicking an editor search box would reject key-up while still reporting ImGui as focused.

		const FImGuiViewportData* ViewportData = static_cast<const FImGuiViewportData*>(FocusedViewport->PlatformUserData);
		const TSharedPtr<SViewport> Viewport = ViewportData->Viewport.Pin();

		if (Viewport.IsValid() && !FocusPath.ContainsWidget(Viewport.Get()))
		{
			return nullptr;
		}
#endif

		// Standalone ImGui windows have no SViewport. Their window identifies the context, as it also
		// does on non-desktop platforms where keyboard routing does not require SViewport membership.

		return FocusedViewport;
	}

	/// Resolves this context's focused viewport for the specified Slate user without accepting stale or interrupted focus paths.
	static ImGuiViewport* FindImGuiViewportForUserFocus(const FSlateApplication& SlateApp, const int32 UserIndex)
	{
		const TSharedPtr<const FSlateUser> SlateUser = SlateApp.GetUser(UserIndex);
		if (!SlateUser.IsValid())
		{
			return nullptr;
		}

		// Use the event user's current path rather than one cached window shared by every Slate user. FWeakWidgetPath::ToWidgetPath()
		// rejects interrupted paths so a destroyed focus target cannot leave the surviving part of its path eligible for keyboard input.

		const FWidgetPath FocusPath = SlateUser->GetWeakFocusPath().ToWidgetPath(FWeakWidgetPath::EInterruptedPathHandling::ReturnInvalid);
		return FindImGuiViewportForFocusPath(FocusPath);
	}

	/// Checks whether the specified Slate user's focus belongs to this context without requiring a live overlay.
	static bool IsUserFocusRelevant(const FSlateApplication& SlateApp, const int32 UserIndex)
	{
		return FindImGuiViewportForUserFocus(SlateApp, UserIndex) != nullptr;
	}

	/// Gives the hit-tested overlay user focus for SImGuiOverlay::OnKeyChar() when ImGui consumes a
	/// pointer press. Canceled holds and outside presses that only dismiss a popup do not transfer focus.
	void FocusOverlayForPointerPress(FSlateApplication& SlateApp, const FPointerEvent& Event, const FPointerLocation& Location) const
	{
		// Additional buttons still belong to a canceled mouse hold, but must not restore its focus.
		// Pressing right while a canceled left-button drag remains held must not take focus from the editor.

		if (!PointerInteraction.IsSet() || PointerInteraction->State != EPointerInteractionState::Held ||
		    !PointerInteraction->bConsumePointerInput || !Location.bOverOverlay)
		{
			return;
		}

		// FImGuiInputProcessor::ResolvePointerLocation() tests this overlay's bounds and topmost window.
		// Using Owner here would reject detached-window presses or focus the main overlay instead. A
		// popup-closing press outside any overlay must not transfer focus merely because ImGui consumes it.

		const TSharedPtr<SImGuiOverlay>& Overlay = Location.Overlay;
		if (!Overlay.IsValid() || Overlay->HasUserFocus(Event.GetUserIndex()).IsSet())
		{
			return;
		}

		// Consuming a press bypasses FSlateApplication::RoutePointerDownEvent() and its automatic focus
		// update. Focus the same overlay used for hit testing so SImGuiOverlay::OnKeyChar() receives typed
		// characters in the pressed window, without moving focus to another viewport of the same ImGui context.

		SlateApp.SetUserFocus(Event.GetUserIndex(), Overlay, EFocusCause::Mouse);
	}

	bool HandlePointerMoved(FSlateApplication& SlateApp, const FPointerEvent& Event)
	{
		if (PointerInteraction.IsSet())
		{
			// ImGuiIO::MousePos is shared by mouse and touch. A mouse move must not pull a touch-held slider to the hardware cursor's
			// position, and a second finger must not move the first finger's slider. Unrelated pointer input stays available to Slate.

			if (!DoesEventMatchCurrentPointerInteraction(Event))
			{
				return false;
			}

			// Cancellation clears Dear ImGui's hold, not the physical button or contact. Suppress
			// movement until release so re-enabling input cannot resume the canceled slider drag.

			if (PointerInteraction->State != EPointerInteractionState::Held)
			{
				return PointerInteraction->bConsumePointerInput;
			}

			const FPointerLocation PointerLocation = ResolvePointerLocation(SlateApp, Event);
			QueuePointerPosition(PointerInteraction->Source, PointerLocation);

			// ImGuiIO::WantCaptureMouse can change during a drag. Keep the press-time consumption decision outside the overlay too,
			// so dragging an ImGui window across its boundary does not suddenly start controlling the application underneath it.

			return PointerInteraction->bConsumePointerInput;
		}

		if (Event.IsTouchEvent())
		{
			// Touch has no hover phase. A contact that never started an interaction cannot update Dear ImGui's pointer.
			return false;
		}

		return HandleMouseHover(SlateApp, Event);
	}

	bool HandleMouseHover(FSlateApplication& SlateApp, const FPointerEvent& Event)
	{
		if (!DoesImGuiAcceptPointerInput())
		{
			return false;
		}

		const FPointerLocation PointerLocation = ResolvePointerLocation(SlateApp, Event);
		if (!PointerLocation.bOverOverlay)
		{
			// Invalidate ImGuiIO::MousePos when hover leaves the overlay. Otherwise the last widget
			// can keep its hover highlight or tooltip while the user points at another editor panel.

			QueuePointerLeave(ImGuiMouseSource_Mouse);
			return false;
		}

		QueuePointerPosition(ImGuiMouseSource_Mouse, PointerLocation);

		// Hover has no press-time owner, so ImGuiIO::WantCaptureMouse determines consumption for each movement.
		return ImGui::GetIO().WantCaptureMouse;
	}

	bool HandleMousePressed(FSlateApplication& SlateApp, const FPointerEvent& Event)
	{
		const FKey Key = Event.GetEffectingButton();
		const ImGuiMouseButton MouseButton = ImGui::ConvertMouseButton(Key);

		if (MouseButton <= -1)
		{
			// Unsupported buttons must not establish ownership that the release handler cannot finish.
			return false;
		}

		if (PointerInteraction.IsSet())
		{
			// Extra buttons may join the owning mouse, but another mouse or touch cannot replace it mid-drag.
			if (!DoesEventMatchCurrentPointerInteraction(Event))
			{
				return false;
			}

			// Additional buttons prolong ownership even after cancellation. Otherwise releasing the original
			// button could let a touch take over while another mouse button remains physically held.

			PointerInteraction->PressedMouseButtons.Emplace(Key);

			if (PointerInteraction->State == EPointerInteractionState::Held)
			{
				const FPointerLocation PointerLocation = ResolvePointerLocation(SlateApp, Event);

				QueuePointerPress(ImGuiMouseSource_Mouse, PointerLocation, MouseButton);
				FocusOverlayForPointerPress(SlateApp, Event, PointerLocation);
			}

			// All buttons inherit the first press's consumption decision. Rechecking ImGuiIO::WantCaptureMouse
			// here could split one physical interaction between ImGui and the application as capture intent changes.

			return PointerInteraction->bConsumePointerInput;
		}
		else
		{
			if (!DoesImGuiAcceptPointerInput())
			{
				return false;
			}

			FPointerInteraction Interaction;
			Interaction.Source = ImGuiMouseSource_Mouse;
			Interaction.UserIndex = Event.GetUserIndex();
			Interaction.DeviceId = Event.GetInputDeviceId();
			Interaction.PointerIndex = Event.GetPointerIndex();

			const FPointerLocation PointerLocation = ResolvePointerLocation(SlateApp, Event);
			const bool bConsumePointerInput = ShouldConsumePointerInputForMousePress(PointerLocation);

			// ImGuiIO::WantCaptureMouse can change during a drag. Keep bConsumePointerInput fixed until release;
			// a drag started in the application must not become an ImGui drag when it crosses the overlay.

			Interaction.bConsumePointerInput = bConsumePointerInput;
			Interaction.PressedMouseButtons.Emplace(Key);
			PointerInteraction = MoveTemp(Interaction);

			// Unconsumed events still reach Dear ImGui to maintain click ownership and dismiss popups.
			// Consumption determines Slate routing, not whether Dear ImGui observes an application-owned press.

			QueuePointerPress(ImGuiMouseSource_Mouse, PointerLocation, MouseButton);
			FocusOverlayForPointerPress(SlateApp, Event, PointerLocation);

			return PointerInteraction->bConsumePointerInput;
		}
	}

	bool HandleTouchStarted(FSlateApplication& SlateApp, const FPointerEvent& Event)
	{
		if (PointerInteraction.IsSet())
		{
			// A second contact must not interrupt the selected interaction. A repeated start for the
			// same contact keeps its consumption decision without queuing a second left-button press.

			if (!DoesEventMatchCurrentPointerInteraction(Event))
			{
				return false;
			}

			return PointerInteraction->bConsumePointerInput;
		}
		else
		{
			if (!DoesImGuiAcceptPointerInput())
			{
				return false;
			}

			FPointerInteraction Interaction;
			Interaction.Source = ImGuiMouseSource_TouchScreen;
			Interaction.UserIndex = Event.GetUserIndex();
			Interaction.DeviceId = Event.GetInputDeviceId();
			Interaction.PointerIndex = Event.GetPointerIndex();

			const FPointerLocation PointerLocation = ResolvePointerLocation(SlateApp, Event);
			const bool bConsumePointerInput = ShouldConsumePointerInputForTouchPress(PointerLocation);

			// ImGuiIO::WantCaptureMouse can change during a drag. Keep bConsumePointerInput fixed until release;
			// a drag started in the application must not become an ImGui drag when it crosses the overlay.

			Interaction.bConsumePointerInput = bConsumePointerInput;
			PointerInteraction = MoveTemp(Interaction);

			// Dear ImGui treats the selected contact as its left button. FImGuiInputProcessor::QueuePointerPress()
			// tags the queued events as ImGuiMouseSource_TouchScreen without turning Slate's touch into mouse input.

			QueuePointerPress(ImGuiMouseSource_TouchScreen, PointerLocation, ImGuiMouseButton_Left);
			FocusOverlayForPointerPress(SlateApp, Event, PointerLocation);

			return PointerInteraction->bConsumePointerInput;
		}
	}

	bool HandleMouseReleased(FSlateApplication& SlateApp, const FPointerEvent& Event)
	{
		// A release from another source must not lift the selected touch or finish another mouse's drag.
		if (!DoesEventMatchCurrentPointerInteraction(Event))
		{
			return false;
		}

		const FKey Key = Event.GetEffectingButton();
		const ImGuiMouseButton MouseButton = ImGui::ConvertMouseButton(Key);

		if (MouseButton <= -1 || !PointerInteraction->PressedMouseButtons.Contains(Key))
		{
			// Do not synthesize an ImGui release for a button whose physical press this interaction never tracked.
			return false;
		}

		// Cancellation is not a successful button release. Finish physical bookkeeping without queuing
		// another ImGui release that could activate the widget the user was holding before cancellation.

		if (PointerInteraction->State == EPointerInteractionState::Held)
		{
			const FPointerLocation PointerLocation = ResolvePointerLocation(SlateApp, Event);
			QueuePointerRelease(ImGuiMouseSource_Mouse, PointerLocation, MouseButton);
		}

		// Multiple mouse buttons share one pointer. Releasing left while right is still held must not
		// free ownership for a touch contact that would interrupt the remaining mouse interaction.

		PointerInteraction->PressedMouseButtons.Remove(Key);

		if (PointerInteraction->PressedMouseButtons.IsEmpty())
		{
			PointerInteraction.Reset();
		}

		// FSlateApplication::ProcessMouseButtonUpEvent() releases native mouse capture after widget routing.
		// Consuming mouse-up here skips that cleanup and can leave the hardware mouse captured after the
		// user releases the button, so even an ImGui-owned mouse release remains available to Slate.

		return false;
	}

	bool HandleTouchEnded(FSlateApplication& SlateApp, const FPointerEvent& Event)
	{
		// Lifting a second finger must not release the left button held by the selected contact.
		if (!DoesEventMatchCurrentPointerInteraction(Event))
		{
			return false;
		}

		// Do not turn a canceled contact into a normal ImGui release, as in FImGuiInputProcessor::HandleMouseReleased().

		if (PointerInteraction->State == EPointerInteractionState::Held)
		{
			const FPointerLocation PointerLocation = ResolvePointerLocation(SlateApp, Event);

			// Touch has no hover after release. Queue the endpoint and release before invalidating
			// position; ImGui::UpdateInputEvents() can spread a quick tap across frames, and reversing
			// this order can make a valid button tap look like a release outside the button.

			QueuePointerRelease(ImGuiMouseSource_TouchScreen, PointerLocation, ImGuiMouseButton_Left);
			QueuePointerLeave(ImGuiMouseSource_TouchScreen);
		}

		// FSlateApplication::ProcessMouseButtonUpEvent() cleans up consumed touch releases, unlike native
		// mouse capture. Save this contact's consumption decision before clearing physical ownership.

		const bool bPreviousConsumePointerInput = PointerInteraction->bConsumePointerInput;

		PointerInteraction.Reset();

		return bPreviousConsumePointerInput;
	}

	bool HandleMouseWheel(FSlateApplication& SlateApp, const FPointerEvent& Event)
	{
		// FImGuiInputProcessor::QueuePointerPosition() would move ImGuiIO::MousePos from a touch-held
		// slider to the hardware mouse. Ignore wheel from another pointer in Dear ImGui, but
		// return false so Slate can still scroll its own widgets while that touch remains active.

		if (PointerInteraction.IsSet() && !DoesEventMatchCurrentPointerInteraction(Event))
		{
			return false;
		}

		const FPointerLocation PointerLocation = ResolvePointerLocation(SlateApp, Event);

		// Wheel input is confined to an overlay even during an ImGui-owned mouse hold. Unlike drag movement,
		// wheel outside the overlay remains available to Slate, such as when pointing at another editor panel.

		if (!PointerLocation.bOverOverlay)
		{
			return false;
		}

		// A wheel event can arrive without a preceding mouse move. Queue its position so scrolling uses
		// the current location. Buttons held by the owning mouse do not disable wheel input; this path
		// checks pointer identity but does not gate scrolling on EPointerInteractionState.

		QueuePointerPosition(ImGuiMouseSource_Mouse, PointerLocation);

		ImGuiIO& IO = ImGui::GetIO();

		IO.AddMouseWheelEvent(0.0f, Event.GetWheelDelta());

		// Within the overlay, bConsumePointerInput keeps routing stable while buttons are held.
		// Without a physical interaction, ImGuiIO::WantCaptureMouse decides whether Slate also receives the wheel.

		return PointerInteraction.IsSet()
			       ? PointerInteraction->bConsumePointerInput
			       : IO.WantCaptureMouse;
	}

	/// Checks whether the event belongs to the interaction's physical owner.
	/// Cancellation preserves that identity until physical release completes the interaction.
	bool DoesEventMatchCurrentPointerInteraction(const FPointerEvent& Event) const
	{
		const ImGuiMouseSource MouseSource = Event.IsTouchEvent() ? ImGuiMouseSource_TouchScreen : ImGuiMouseSource_Mouse;

		// FPointerEvent::GetPointerIndex() alone is not unique across sources, users and devices.
		// Match all of them so another user's touch release cannot finish the selected pointer's drag.

		return PointerInteraction.IsSet() &&
		       PointerInteraction->Source == MouseSource &&
		       PointerInteraction->UserIndex == Event.GetUserIndex() &&
		       PointerInteraction->DeviceId == Event.GetInputDeviceId() &&
		       PointerInteraction->PointerIndex == Event.GetPointerIndex();
	}

	static ImGuiViewport* FindImGuiViewportForWindow(const TSharedPtr<SWindow>& Window)
	{
		if (!Window.IsValid())
		{
			return nullptr;
		}

		for (ImGuiViewport* Viewport : ImGui::GetPlatformIO().Viewports)
		{
			const FImGuiViewportData* ViewportData = FImGuiViewportData::GetOrCreate(Viewport);
			if (ViewportData->Window == Window)
			{
				return Viewport;
			}
		}

		return nullptr;
	}

	FPointerLocation ResolvePointerLocation(FSlateApplication& SlateApp, const FPointerEvent& Event) const
	{
		// FSlateApplication::LocateWindowUnderMouse() finds the topmost window, not just Owner's screen rectangle.
		// A separate Slate popup covering the overlay must receive input instead of the ImGui control hidden below it.

		const FWidgetPath WidgetsUnderPointer = SlateApp.LocateWindowUnderMouse(
			Event.GetScreenSpacePosition(), SlateApp.GetInteractiveTopLevelWindows(), false, Event.GetUserIndex());

		TSharedPtr<SWindow> WindowUnderPointer;
		if (WidgetsUnderPointer.IsValid())
		{
			WindowUnderPointer = WidgetsUnderPointer.GetDeepestWindow();
		}

		// CoordinateWindow chooses fullscreen mapping, not ownership of the hit. Single-viewport input uses Owner's mapping even after
		// a drag leaves its window; multi-viewport input uses the hit window and falls back to Owner only when no window is found.

		const bool bMultipleViewports = ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable;
		TSharedPtr<SWindow> CoordinateWindow = WindowUnderPointer;

		if (!bMultipleViewports || !WindowUnderPointer.IsValid())
		{
			CoordinateWindow = SlateApp.FindWidgetWindow(Owner->AsShared());
		}

		FPointerLocation PointerLocation;
		PointerLocation.Viewport = ImGui::GetMainViewport();

		// Use WindowUnderPointer, not the fallback CoordinateWindow, to identify ImGui membership. An unrelated Slate window can cover
		// Owner's rectangle; looking up Owner's window instead would mistake a click on that covering window for a hit on visible ImGui UI.

		ImGuiViewport* ViewportUnderPointer = FindImGuiViewportForWindow(WindowUnderPointer);

		if (bMultipleViewports && ViewportUnderPointer != nullptr)
		{
			PointerLocation.Viewport = ViewportUnderPointer;
		}

		PointerLocation.ScreenPosition = (SlateApp.*GTransformPointerEvent)(Event, CoordinateWindow).GetScreenSpacePosition();

		PointerLocation.ImGuiPosition = PointerLocation.ScreenPosition;

		if (!bMultipleViewports)
		{
			// ImGuiConfigFlags_ViewportsEnable uses absolute screen coordinates; single-viewport
			// input is overlay-relative. Keep ScreenPosition absolute for Slate bounds, or a tap in
			// an offset game viewport can hit a different control from the one under the user's finger.

			PointerLocation.ImGuiPosition -= Owner->GetTickSpaceGeometry().AbsolutePosition;
		}

		if (ViewportUnderPointer != nullptr)
		{
			const FImGuiViewportData* ViewportData = static_cast<const FImGuiViewportData*>(ViewportUnderPointer->PlatformUserData);

			// ImGui_CreateWindow() gives each detached viewport its own SImGuiOverlay with input handling disabled.
			// Retain the hit-tested overlay so FImGuiInputProcessor::FocusOverlayForPointerPress() uses the same
			// target as the bounds check, not Owner or a viewport selected only for coordinate conversion.

			const TSharedPtr<SImGuiOverlay> Overlay = ViewportData->Overlay.Pin();
			PointerLocation.Overlay = Overlay;

			// ImGuiIO::WantCaptureMouse can stay set for an open modal across the application. Check overlay bounds rather than the
			// hosting SWindow, so a dialog in an embedded game viewport does not make the rest of the editor window unclickable.

			PointerLocation.bOverOverlay =
				Overlay.IsValid() && Overlay->GetTickSpaceGeometry().GetLayoutBoundingRect().ContainsPoint(PointerLocation.ScreenPosition);
		}

		return PointerLocation;
	}

	static bool DoesImGuiAcceptPointerInput()
	{
		const ImGuiIO& IO = ImGui::GetIO();

		// ImGuiConfigFlags_NoMouse disables touch too because Dear ImGui uses one pointer state for both.
		// Cursor visibility is not permission: a touchscreen can operate the UI without any visible mouse.
		// Respect application-controlled flags rather than silently enabling input again.

		return IO.AppAcceptingEvents && !(IO.ConfigFlags & ImGuiConfigFlags_NoMouse);
	}

	/// Chooses mouse consumption from hover-derived ImGuiIO::WantCaptureMouse and popup-dismissal rules.
	/// Modal blocking stays within the overlay so unrelated Slate UI remains interactive.
	static bool ShouldConsumePointerInputForMousePress(const FPointerLocation& Location)
	{
		const ImGuiContext& Context = *ImGui::GetCurrentContext();

		// Before the first ImGui::NewFrame(), no completed ImGui frame supplies capture or hit-test state.
		// Do not consume a startup click on behalf of an interface that has not been established yet.

		if (Context.FrameCount <= 0)
		{
			return false;
		}

		if (!Location.bOverOverlay)
		{
			// A regular popup owns its closing press, so clicking outside a menu must not also activate the application underneath. A modal
			// only blocks its overlay; otherwise an ImGui dialog inside a game viewport would make unrelated editor panels unclickable.

			const bool bHasOpenPopup = !Context.OpenPopupStack.empty();
			const ImGuiWindow* ModalWindow = ImGui::GetTopMostPopupModal();

			return bHasOpenPopup && ModalWindow == nullptr;
		}

		// Mouse hover supplies ImGuiIO::WantCaptureMouse. FImGuiInputProcessor::HandleMousePressed()
		// stores this result once instead of reassigning the drag as capture changes in later frames.

		return Context.IO.WantCaptureMouse;
	}

	/// Chooses touch consumption from the initial contact's hit test instead of stale ImGuiIO::WantCaptureMouse.
	/// Preserves popup-dismissal ownership and modal blocking without updating Dear ImGui's frame input state.
	static bool ShouldConsumePointerInputForTouchPress(const FPointerLocation& Location)
	{
		ImGuiContext& Context = *ImGui::GetCurrentContext();

		// Leave startup presses to Slate, as in FImGuiInputProcessor::ShouldConsumePointerInputForMousePress().

		if (Context.FrameCount <= 0)
		{
			return false;
		}

		const bool bHasOpenPopup = !Context.OpenPopupStack.empty();
		ImGuiWindow* ModalWindow = ImGui::GetTopMostPopupModal();

		if (!Location.bOverOverlay)
		{
			// Preserve popup-close ownership outside the overlay, as in FImGuiInputProcessor::ShouldConsumePointerInputForMousePress().

			return bHasOpenPopup && ModalWindow == nullptr;
		}

		// Based on ImGui::UpdateHoveredWindowAndCaptureFlags(). Its full update changes frame input and
		// click ownership, so query only the first contact here. Using stale ImGuiIO::WantCaptureMouse
		// instead can pass a tap through an ImGui button or consume a tap on empty application space.

		// ImGui::FindHoveredWindowEx() filters by ImGuiContext::MouseViewport even in first-hit
		// mode. Temporarily select this contact's viewport so a tap in a detached ImGui window is
		// not tested against the main viewport, then restore the frame's existing viewport state.

		TGuardValue MouseViewportGuard(Context.MouseViewport, static_cast<ImGuiViewportP*>(Location.Viewport));

		ImGuiWindow* HoveredWindow = nullptr;
		ImGui::FindHoveredWindowEx(ImVec2(Location.ImGuiPosition.X, Location.ImGuiPosition.Y), true, &HoveredWindow, nullptr);

		// ImGui::FindHoveredWindowEx() does not enforce modal blocking. Exclude windows outside the modal's
		// begin stack so a tap on a dimmed slider is not treated as a hit on that underlying window.

		if (ModalWindow != nullptr && HoveredWindow != nullptr &&
		    !ImGui::IsWindowWithinBeginStackOf(HoveredWindow->RootWindow, ModalWindow))
		{
			HoveredWindow = nullptr;
		}

		// An open popup also owns empty space inside the overlay: a menu can close on that tap, and a
		// modal can block it without letting the same interaction start in the application behind it.

		return HoveredWindow != nullptr || bHasOpenPopup;
	}

	static void QueuePointerPosition(const ImGuiMouseSource Source, const FPointerLocation& Location)
	{
		ImGuiIO& IO = ImGui::GetIO();

		// ImGuiIO::AddMouseSourceEvent() latches the source for later position, button and wheel events. Set it for every
		// submission so switching from touch to mouse cannot leave real mouse input classified as touchscreen input in Dear ImGui.

		IO.AddMouseSourceEvent(Source);

		// ImGui::UpdateInputEvents() discards queued positions while ImGuiIO::WantSetMousePos is true.
		// Clear that request for touch so navigation cannot replace a tap location with its old mouse target.
		// Preserve the existing request for other sources; this operation never enables cursor warping.

		IO.WantSetMousePos &= Source != ImGuiMouseSource_TouchScreen;

		IO.AddMousePosEvent(Location.ImGuiPosition.X, Location.ImGuiPosition.Y);
	}

	static void QueuePointerPress(const ImGuiMouseSource Source, const FPointerLocation& Location, const ImGuiMouseButton Button)
	{
		// A touch can press without prior movement. Queue its location before ImGuiIO::AddMouseButtonEvent(),
		// so tapping button B after button A does not reuse A's position. ImGui::UpdateInputEvents()
		// keeps the position and press ordered even when it spreads the input across trickled frames.

		QueuePointerPosition(Source, Location);

		ImGui::GetIO().AddMouseButtonEvent(Button, true);
	}

	static void QueuePointerRelease(const ImGuiMouseSource Source, const FPointerLocation& Location,
	                                const ImGuiMouseButton Button)
	{
		// A release can arrive without a final move. Queue its endpoint before ImGuiIO::AddMouseButtonEvent(),
		// or pressing a button and releasing outside it can still activate it using the last inside position.
		// Drag completion also needs the final endpoint rather than the previous movement sample.

		QueuePointerPosition(Source, Location);

		ImGui::GetIO().AddMouseButtonEvent(Button, false);
	}

	static void QueuePointerLeave(const ImGuiMouseSource Source)
	{
		ImGuiIO& IO = ImGui::GetIO();

		// ImGuiIO::AddMousePosEvent() uses (-FLT_MAX, -FLT_MAX) for an absent pointer. Keep source tagging
		// consistent without synthesizing a press or release; the invalid position removes hover highlights
		// and tooltips after the pointer leaves, including touch contacts that have no hover after release.

		IO.AddMouseSourceEvent(Source);
		IO.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
	}

	void CancelPointerInteractionIfRequired()
	{
		// Clear pending pointer input even without a physical owner: a quick tap can already be
		// released while its press and release still wait in Dear ImGui's trickled event queue.

		if (bCancelPointerInteractionRequested || !DoesImGuiAcceptPointerInput())
		{
			bCancelPointerInteractionRequested = false;

			ClearImGuiPointerInput();

			if (PointerInteraction.IsSet())
			{
				// ImGuiIO::ClearInputMouse() does not release the physical contact. Retain its identity and
				// bConsumePointerInput until release: disabling input while dragging a slider and enabling it
				// with the finger still down must not resume the drag or leak its remainder to the application.

				PointerInteraction->State = EPointerInteractionState::CanceledUntilRelease;
			}
		}
	}

	static void ClearImGuiPointerInput()
	{
		ImGuiContext& Context = *ImGui::GetCurrentContext();

		DiscardQueuedPointerEvents(Context);
		CancelImGuiPointerActivity(Context);
		ClearImGuiPointerState(Context.IO);
	}

	static void DiscardQueuedPointerEvents(ImGuiContext& Context)
	{
		// ImGuiIO::ClearInputMouse() does not discard queued events. Mouse and touch both use
		// ImGuiInputSource_Mouse in ImGuiContext::InputEventsQueue; remove that source so canceled
		// taps cannot activate later, without discarding unrelated keyboard, gamepad or text input.

		ImVector<ImGuiInputEvent>& InputEventsQueue = Context.InputEventsQueue;

		for (int32 EventIndex = InputEventsQueue.Size - 1; EventIndex >= 0; EventIndex--)
		{
			if (InputEventsQueue[EventIndex].Source == ImGuiInputSource_Mouse)
			{
				InputEventsQueue.erase(InputEventsQueue.Data + EventIndex);
			}
		}
	}

	static void CancelImGuiPointerActivity(const ImGuiContext& Context)
	{
		// ImGui::AcceptDragDropPayload() can deliver as soon as the source button is no longer down. Clear
		// internal payloads before ImGuiIO::ClearInputMouse(), or disabling input during an item drag can
		// complete an unintended drop. Leave ImGuiDragDropFlags_SourceExtern payloads to their external source.

		if (Context.DragDropActive && !(Context.DragDropSourceFlags & ImGuiDragDropFlags_SourceExtern))
		{
			ImGui::ClearDragDrop();
		}

		// Clearing raw mouse input alone leaves ImGuiContext::MovingWindow from a canceled title-bar drag.
		// ImGui::StopMouseMovingWindow() performs viewport cleanup too, unlike only clearing that pointer.

		ImGui::StopMouseMovingWindow();

		// Buttons, sliders and resize handles can retain ImGuiContext::ActiveId after input is disabled. Cancel only
		// ImGuiInputSource_Mouse activity so pointer cancellation does not also cancel a keyboard- or gamepad-driven active widget.

		if (Context.ActiveIdSource == ImGuiInputSource_Mouse)
		{
			ImGui::ClearActiveID();
		}
	}

	static void ClearImGuiPointerState(ImGuiIO& IO)
	{
		// ImGuiIO::ClearInputMouse() resets held buttons, durations, position, wheel and mouse key aliases. Clear rather than queue button
		// releases, so cancellation cannot look like a successful release and activate a button the user was holding when input was disabled.

		IO.ClearInputMouse();

		// ImGuiIO::AddMouseButtonEvent() can latch a macOS Ctrl-left click as a right click. Clear ImGuiIO::MouseCtrlLeftAsRightClick
		// too, or a canceled Ctrl-click can make the next plain left-button event affect the wrong button.

		IO.MouseCtrlLeftAsRightClick = false;

		// ImGuiIO::ClearInputMouse() does not reset the source latched by ImGuiIO::AddMouseSourceEvent().
		// Restore future mouse-event tagging so another input producer cannot inherit the canceled touch
		// source. This does not immediately change the source of the already processed ImGuiIO::MouseSource.

		IO.AddMouseSourceEvent(ImGuiMouseSource_Mouse);

		// ImGuiIO::ClearInputMouse() leaves current-frame click and release flags. Cancellation can happen after
		// ImGui::NewFrame(); ImGui::IsMouseReleased() or ImGui::UpdateMouseMovingWindowEndFrame() can still
		// react to those flags unless cleared now, causing a canceled click to focus a window or close a popup.

		for (int32 ButtonIndex = 0; ButtonIndex < ImGuiMouseButton_COUNT; ButtonIndex++)
		{
			IO.MouseClicked[ButtonIndex] = false;
			IO.MouseReleased[ButtonIndex] = false;
			IO.MouseDoubleClicked[ButtonIndex] = false;
			IO.MouseClickedCount[ButtonIndex] = 0;

			// ImGuiIO::MouseDownOwned and ImGuiIO::MouseDownOwnedUnlessPopupClose no longer describe a live press.
			// They are inactive once MouseDown and MouseReleased are cleared; reset them for consistent canceled state.

			IO.MouseDownOwned[ButtonIndex] = false;
			IO.MouseDownOwnedUnlessPopupClose[ButtonIndex] = false;
		}
	}

	static void OnNewFrame(ImGuiContext* Context, ImGuiContextHook* Hook)
	{
		FImGuiInputProcessor* Processor = static_cast<FImGuiInputProcessor*>(Hook->UserData);

		// Input can be disabled after the event callbacks and FImGuiInputProcessor::Tick(). This last check removes queued pointer events
		// before ImGui::UpdateInputEvents(), so an already released tap cannot activate a button after the application disables input.

		Processor->CancelPointerInteractionIfRequired();

		// A quick tap can finish before ImGui::NewFrame(): physical ownership is gone, but its input is queued. Check
		// the queue, not just PointerInteraction, and clear ImGuiIO::WantSetMousePos so ImGui::UpdateInputEvents()
		// cannot discard the tap's position and process its press at an unrelated navigation cursor location.

		for (const ImGuiInputEvent& InputEvent : Context->InputEventsQueue)
		{
			if (InputEvent.Type == ImGuiInputEventType_MousePos &&
			    InputEvent.MousePos.MouseSource == ImGuiMouseSource_TouchScreen)
			{
				Context->IO.WantSetMousePos = false;
				return;
			}
		}
	}
};

void SImGuiOverlay::Construct(const FArguments& Args)
{
	SetVisibility(EVisibility::HitTestInvisible);
	ForceVolatile(true);

	Context = Args._Context.IsValid() ? Args._Context : FImGuiContext::Create();
	if (Args._HandleInput)
	{
		InputProcessor = MakeShared<FImGuiInputProcessor>(this);
		FSlateApplication::Get().RegisterInputPreProcessor(InputProcessor.ToSharedRef(), 0);
	}
}

SImGuiOverlay::~SImGuiOverlay()
{
	if (FSlateApplication::IsInitialized() && InputProcessor.IsValid())
	{
		FSlateApplication::Get().UnregisterInputPreProcessor(InputProcessor);
	}

	// FImGuiInputProcessor's destructor uses Owner's ImGui context to remove its frame hook and cancel input.
	// Destroy it while SImGuiOverlay is still intact, before its remaining members are torn down.

	InputProcessor.Reset();
}

int32 SImGuiOverlay::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	if (!DrawData.bValid)
	{
		return LayerId;
	}

	const FSlateRenderTransform Transform(AllottedGeometry.GetAccumulatedRenderTransform().GetTranslation() - FVector2d(DrawData.DisplayPos));

	TArray<FSlateVertex> Vertices;
	TArray<SlateIndex> Indices;
	FSlateBrush TextureBrush;

	for (const FImGuiDrawList& DrawList : DrawData.DrawLists)
	{
		Vertices.SetNumUninitialized(DrawList.VtxBuffer.Size);

		ImDrawVert* SrcVertex = DrawList.VtxBuffer.Data;
		FSlateVertex* DstVertex = Vertices.GetData();

		for (int32 BufferIdx = 0; BufferIdx < Vertices.Num(); ++BufferIdx, ++SrcVertex, ++DstVertex)
		{
			DstVertex->TexCoords[0] = SrcVertex->uv.x;
			DstVertex->TexCoords[1] = SrcVertex->uv.y;
			DstVertex->TexCoords[2] = 1;
			DstVertex->TexCoords[3] = 1;
			DstVertex->Position = TransformPoint(Transform, FVector2f(SrcVertex->pos));
			DstVertex->Color.Bits = SrcVertex->col;
		}

		ImGui::CopyArray(DrawList.IdxBuffer, Indices);

		for (const ImDrawCmd& DrawCmd : DrawList.CmdBuffer)
		{
#if WITH_ENGINE
			UTexture* Texture = DrawCmd.GetTexID();
			if (TextureBrush.GetResourceObject() != Texture)
			{
				TextureBrush.SetResourceObject(Texture);
				if (IsValid(Texture))
				{
					TextureBrush.ImageSize.X = Texture->GetSurfaceWidth();
					TextureBrush.ImageSize.Y = Texture->GetSurfaceHeight();
					TextureBrush.ImageType = ESlateBrushImageType::FullColor;
					TextureBrush.DrawAs = ESlateBrushDrawType::Image;
				}
				else
				{
					TextureBrush.ImageSize.X = 0;
					TextureBrush.ImageSize.Y = 0;
					TextureBrush.ImageType = ESlateBrushImageType::NoImage;
					TextureBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
				}
			}
#else
			FSlateBrush* Texture = DrawCmd.GetTexID();
			if (Texture)
			{
				TextureBrush = *Texture;
			}
			else
			{
				TextureBrush.ImageSize.X = 0;
				TextureBrush.ImageSize.Y = 0;
				TextureBrush.ImageType = ESlateBrushImageType::NoImage;
				TextureBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
			}
#endif

			FSlateRect ClipRect(DrawCmd.ClipRect.x, DrawCmd.ClipRect.y, DrawCmd.ClipRect.z, DrawCmd.ClipRect.w);
			ClipRect = TransformRect(Transform, ClipRect);

			OutDrawElements.PushClip(FSlateClippingZone(ClipRect));

			FSlateDrawElement::MakeCustomVerts(
				OutDrawElements, LayerId, TextureBrush.GetRenderingResource(),
				TArray(Vertices.GetData() + DrawCmd.VtxOffset, Vertices.Num() - DrawCmd.VtxOffset),
				TArray(Indices.GetData() + DrawCmd.IdxOffset, DrawCmd.ElemCount),
				nullptr, 0, 0
			);

			OutDrawElements.PopClip();
		}
	}

	return LayerId;
}

FVector2D SImGuiOverlay::ComputeDesiredSize(float LayoutScaleMultiplier) const
{
	return FVector2D::ZeroVector;
}

bool SImGuiOverlay::SupportsKeyboardFocus() const
{
	return true;
}

FReply SImGuiOverlay::OnKeyChar(const FGeometry& MyGeometry, const FCharacterEvent& Event)
{
	ImGui::FScopedContext ScopedContext(Context);

	ImGuiIO& IO = ImGui::GetIO();

	IO.AddInputCharacter(Event.GetCharacter());

	return IO.WantTextInput ? FReply::Handled() : FReply::Unhandled();
}

TSharedPtr<FImGuiContext> SImGuiOverlay::GetContext() const
{
	return Context;
}

void SImGuiOverlay::SetDrawData(const ImDrawData* InDrawData)
{
	DrawData = FImGuiDrawData(InDrawData);
}

#endif // #ifndef IMGUI_DISABLE
