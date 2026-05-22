#include "SImGuiOverlay.h"

#ifndef IMGUI_DISABLE

#include <Framework/Application/SlateApplication.h>
#include <Framework/Application/SlateUser.h>

#include "ImGuiContext.h"
#include "Widgets/SViewport.h"

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

class FImGuiInputProcessor : public IInputProcessor
{
public:
	explicit FImGuiInputProcessor(SImGuiOverlay* InOwner)
	{
		Owner = InOwner;

		FSlateApplication::Get().OnApplicationActivationStateChanged().AddRaw(this, &FImGuiInputProcessor::OnApplicationActivationChanged);
		FSlateApplication::Get().OnFocusChanging().AddRaw(this, &FImGuiInputProcessor::OnFocusChanging);

		LastFocusedWindow = FSlateApplication::Get().GetActiveTopLevelRegularWindow();
	}

	virtual ~FImGuiInputProcessor() override
	{
		if (FSlateApplication::IsInitialized())
		{
			FSlateApplication::Get().OnApplicationActivationStateChanged().RemoveAll(this);
			FSlateApplication::Get().OnFocusChanging().RemoveAll(this);
		}
	}

	void OnApplicationActivationChanged(bool bIsActive) const
	{
		ImGui::FScopedContext ScopedContext(Owner->GetContext());

		ImGuiIO& IO = ImGui::GetIO();

		IO.AddFocusEvent(bIsActive);
	}

	void OnFocusChanging(const FFocusEvent& Event, const FWeakWidgetPath& OldWidgetPath, const TSharedPtr<SWidget>& OldWidget, const FWidgetPath& NewWidgetPath, const TSharedPtr<SWidget>& NewWidget)
	{
		if (NewWidgetPath.IsValid())
		{
			LastFocusedWindow = NewWidgetPath.GetDeepestWindow();
		}
		else
		{
			LastFocusedWindow.Reset();
		}
	}

	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> SlateCursor) override
	{
		ImGui::FScopedContext ScopedContext(Owner->GetContext());

		ImGuiIO& IO = ImGui::GetIO();

		const bool bHasGamepad = (IO.BackendFlags & ImGuiBackendFlags_HasGamepad);
		if (bHasGamepad != SlateApp.IsGamepadAttached())
		{
			IO.BackendFlags ^= ImGuiBackendFlags_HasGamepad;
		}

		if (IO.WantSetMousePos)
		{
			FVector2f Position = IO.MousePos;
			if (!(IO.ConfigFlags & ImGuiConfigFlags_ViewportsEnable))
			{
				// Mouse position for single viewport mode is in client space
				Position += Owner->GetTickSpaceGeometry().AbsolutePosition;
			}

			SlateCursor->SetPosition(Position.X, Position.Y);
		}

		if (IO.WantTextInput && !Owner->HasKeyboardFocus())
		{
			// No HandleKeyCharEvent so punt focus to the widget for it to receive OnKeyChar events
			SlateApp.SetKeyboardFocus(Owner->AsShared());
		}
	}

	virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& Event) override
	{
		ImGui::FScopedContext ScopedContext(Owner->GetContext());

		if (!ShouldHandleEvent(SlateApp, Event))
		{
			return false;
		}

		ImGuiIO& IO = ImGui::GetIO();

		IO.AddKeyEvent(ImGui::ConvertKey(Event.GetKey()), true);

		const FModifierKeysState& ModifierKeys = Event.GetModifierKeys();
		IO.AddKeyEvent(ImGuiMod_Ctrl, ModifierKeys.IsControlDown());
		IO.AddKeyEvent(ImGuiMod_Shift, ModifierKeys.IsShiftDown());
		IO.AddKeyEvent(ImGuiMod_Alt, ModifierKeys.IsAltDown());
		IO.AddKeyEvent(ImGuiMod_Super, ModifierKeys.IsCommandDown());

		return IO.WantCaptureKeyboard;
	}

	virtual bool HandleKeyUpEvent(FSlateApplication& SlateApp, const FKeyEvent& Event) override
	{
		ImGui::FScopedContext ScopedContext(Owner->GetContext());

		if (!ShouldHandleEvent(SlateApp, Event))
		{
			return false;
		}

		ImGuiIO& IO = ImGui::GetIO();

		IO.AddKeyEvent(ImGui::ConvertKey(Event.GetKey()), false);

		const FModifierKeysState& ModifierKeys = Event.GetModifierKeys();
		IO.AddKeyEvent(ImGuiMod_Ctrl, ModifierKeys.IsControlDown());
		IO.AddKeyEvent(ImGuiMod_Shift, ModifierKeys.IsShiftDown());
		IO.AddKeyEvent(ImGuiMod_Alt, ModifierKeys.IsAltDown());
		IO.AddKeyEvent(ImGuiMod_Super, ModifierKeys.IsCommandDown());

		return IO.WantCaptureKeyboard;
	}

	virtual bool HandleAnalogInputEvent(FSlateApplication& SlateApp, const FAnalogInputEvent& Event) override
	{
		ImGui::FScopedContext ScopedContext(Owner->GetContext());

		if (!ShouldHandleEvent(SlateApp, Event))
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

		if (!ShouldHandleEvent(SlateApp, Event))
		{
			return false;
		}

		ImGuiIO& IO = ImGui::GetIO();

		const TSharedPtr<FSlateUser> SlateUser = SlateApp.GetUser(Event.GetUserIndex());
		if (SlateUser.IsValid())
		{
			const FImGuiViewportData* TargetViewport = nullptr;

			if (!SlateUser->HasCapture(Event.GetPointerIndex()))
			{
				const FWeakWidgetPath LastWidgetsUnderPointer = SlateUser->GetLastWidgetsUnderPointer(Event.GetPointerIndex());
				TargetViewport = FindViewportForWindow(LastWidgetsUnderPointer.Window.Pin());
			}

			if (!TargetViewport && !ImGui::IsMouseDown(0))
			{
				IO.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
				return false;
			}
		}

		FVector2f Position = Event.GetScreenSpacePosition();
		if (!(IO.ConfigFlags & ImGuiConfigFlags_ViewportsEnable))
		{
			// Mouse position for single viewport mode is in client space
			Position -= Owner->GetTickSpaceGeometry().AbsolutePosition;
		}

		IO.AddMousePosEvent(Position.X, Position.Y);

		return IO.WantCaptureMouse;
	}

	virtual bool HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& Event) override
	{
		ImGui::FScopedContext ScopedContext(Owner->GetContext());

		if (!ShouldHandleEvent(SlateApp, Event))
		{
			return false;
		}

		ImGuiIO& IO = ImGui::GetIO();

		const FKey Button = Event.GetEffectingButton();
		if (Button == EKeys::LeftMouseButton)
		{
			IO.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
		}
		else if (Button == EKeys::RightMouseButton)
		{
			IO.AddMouseButtonEvent(ImGuiMouseButton_Right, true);
		}
		else if (Button == EKeys::MiddleMouseButton)
		{
			IO.AddMouseButtonEvent(ImGuiMouseButton_Middle, true);
		}

		return IO.WantCaptureMouse;
	}

	virtual bool HandleMouseButtonUpEvent(FSlateApplication& SlateApp, const FPointerEvent& Event) override
	{
		ImGui::FScopedContext ScopedContext(Owner->GetContext());

		if (!ShouldHandleEvent(SlateApp, Event))
		{
			return false;
		}

		ImGuiIO& IO = ImGui::GetIO();

		const FKey Button = Event.GetEffectingButton();
		if (Button == EKeys::LeftMouseButton)
		{
			IO.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
		}
		else if (Button == EKeys::RightMouseButton)
		{
			IO.AddMouseButtonEvent(ImGuiMouseButton_Right, false);
		}
		else if (Button == EKeys::MiddleMouseButton)
		{
			IO.AddMouseButtonEvent(ImGuiMouseButton_Middle, false);
		}

		return false;
	}

	virtual bool HandleMouseButtonDoubleClickEvent(FSlateApplication& SlateApp, const FPointerEvent& Event) override
	{
		// Treat as mouse down, ImGui handles double click internally
		return HandleMouseButtonDownEvent(SlateApp, Event);
	}

	virtual bool HandleMouseWheelOrGestureEvent(FSlateApplication& SlateApp, const FPointerEvent& Event, const FPointerEvent* GestureEvent) override
	{
		ImGui::FScopedContext ScopedContext(Owner->GetContext());

		if (!ShouldHandleEvent(SlateApp, Event))
		{
			return false;
		}

		ImGuiIO& IO = ImGui::GetIO();

		IO.AddMouseWheelEvent(0.0f, Event.GetWheelDelta());

		return IO.WantCaptureMouse;
	}

	bool ShouldHandleEvent(FSlateApplication& SlateApp, const FInputEvent& Event) const
	{
#if WITH_EDITORONLY_DATA
		if (GIntraFrameDebuggingGameThread)
		{
			// Discard input events when the game thread is paused for debugging
			return false;
		}
#endif

		if (Event.IsKeyEvent())
		{
			const FImGuiViewportData* FocusedViewport = FindViewportForWindow(LastFocusedWindow.Pin());

#if PLATFORM_DESKTOP
			if (FocusedViewport == nullptr)
			{
				return false;
			}

			const TSharedPtr<FSlateUser> SlateUser = SlateApp.GetUser(Event);
			const TSharedPtr<SViewport> Viewport = FocusedViewport->Viewport.Pin();

			if (SlateUser.IsValid() && Viewport.IsValid())
			{
				// Ignore input if the game viewport is not in the focus path.
				return SlateUser->IsWidgetInFocusPath(Viewport);
			}
#else
			return FocusedViewport != nullptr;
#endif
		}

		return true;
	}

	static FImGuiViewportData* FindViewportForWindow(const TSharedPtr<SWindow>& Window)
	{
		if (!Window.IsValid())
		{
			return nullptr;
		}

		for (ImGuiViewport* Viewport : ImGui::GetPlatformIO().Viewports)
		{
			FImGuiViewportData* ViewportData = FImGuiViewportData::GetOrCreate(Viewport);
			if (ViewportData->Window == Window)
			{
				return ViewportData;
			}
		}

		return nullptr;
	}

private:
	SImGuiOverlay* Owner = nullptr;
	TWeakPtr<SWindow> LastFocusedWindow;
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
