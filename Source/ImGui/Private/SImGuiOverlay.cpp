#include "SImGuiOverlay.h"

#include <Engine/Texture2D.h>
#include <Framework/Application/SlateApplication.h>

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

	DrawLists.SetNumUninitialized(Source->CmdListsCount);
	ConstructItems<FImGuiDrawList>(DrawLists.GetData(), Source->CmdLists.Data, Source->CmdListsCount);

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
	}

	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> SlateCursor) override
	{
		ImGui::FScopedContextSwitcher ContextSwitcher(Owner->GetContext());

		ImGuiIO& IO = ImGui::GetIO();

		const bool bHasGamepad = (IO.BackendFlags & ImGuiBackendFlags_HasGamepad);
		if (bHasGamepad != SlateApp.IsGamepadAttached())
		{
			IO.BackendFlags ^= ImGuiBackendFlags_HasGamepad;
		}

		if (IO.WantSetMousePos)
		{
			SlateApp.SetCursorPos(IO.MousePos);
		}

#if 0
		// #TODO(Ves): Sometimes inconsistent, something else is changing the cursor later in the frame?
		if (IO.WantCaptureMouse && !(IO.ConfigFlags & ImGuiConfigFlags_NoMouseCursorChange))
		{
			const ImGuiMouseCursor CursorType = ImGui::GetMouseCursor();

			if (IO.MouseDrawCursor || CursorType == ImGuiMouseCursor_None)
			{
				SlateCursor->SetType(EMouseCursor::None);
			}
			else if (CursorType == ImGuiMouseCursor_Arrow)
			{
				SlateCursor->SetType(EMouseCursor::Default);
			}
			else if (CursorType == ImGuiMouseCursor_TextInput)
			{
				SlateCursor->SetType(EMouseCursor::TextEditBeam);
			}
			else if (CursorType == ImGuiMouseCursor_ResizeAll)
			{
				SlateCursor->SetType(EMouseCursor::CardinalCross);
			}
			else if (CursorType == ImGuiMouseCursor_ResizeNS)
			{
				SlateCursor->SetType(EMouseCursor::ResizeUpDown);
			}
			else if (CursorType == ImGuiMouseCursor_ResizeEW)
			{
				SlateCursor->SetType(EMouseCursor::ResizeLeftRight);
			}
			else if (CursorType == ImGuiMouseCursor_ResizeNESW)
			{
				SlateCursor->SetType(EMouseCursor::ResizeSouthWest);
			}
			else if (CursorType == ImGuiMouseCursor_ResizeNWSE)
			{
				SlateCursor->SetType(EMouseCursor::ResizeSouthEast);
			}
			else if (CursorType == ImGuiMouseCursor_Hand)
			{
				SlateCursor->SetType(EMouseCursor::Hand);
			}
			else if (CursorType == ImGuiMouseCursor_NotAllowed)
			{
				SlateCursor->SetType(EMouseCursor::SlashedCircle);
			}
		}
#endif

		if (IO.WantCaptureKeyboard && !Owner->HasKeyboardFocus())
		{
			// No HandleKeyCharEvent so punt focus to the widget for it to receive OnKeyChar events
			SlateApp.SetKeyboardFocus(Owner->AsShared());
		}
	}

	virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& Event) override
	{
		ImGui::FScopedContextSwitcher ContextSwitcher(Owner->GetContext());

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
		ImGui::FScopedContextSwitcher ContextSwitcher(Owner->GetContext());

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
		ImGui::FScopedContextSwitcher ContextSwitcher(Owner->GetContext());

		ImGuiIO& IO = ImGui::GetIO();

		const float Value = Event.GetAnalogValue();
		IO.AddKeyAnalogEvent(ImGui::ConvertKey(Event.GetKey()), FMath::Abs(Value) > 0.1f, Value);

		return IO.WantCaptureKeyboard;
	}

	virtual bool HandleMouseMoveEvent(FSlateApplication& SlateApp, const FPointerEvent& Event) override
	{
		ImGui::FScopedContextSwitcher ContextSwitcher(Owner->GetContext());

		ImGuiIO& IO = ImGui::GetIO();

		if (SlateApp.HasAnyMouseCaptor())
		{
			IO.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
			return false;
		}

		const FVector2f Position = Event.GetScreenSpacePosition();
		IO.AddMousePosEvent(Position.X, Position.Y);

		return IO.WantCaptureMouse;
	}

	virtual bool HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& Event) override
	{
		ImGui::FScopedContextSwitcher ContextSwitcher(Owner->GetContext());

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
		ImGui::FScopedContextSwitcher ContextSwitcher(Owner->GetContext());

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

		return IO.WantCaptureMouse;
	}

	virtual bool HandleMouseButtonDoubleClickEvent(FSlateApplication& SlateApp, const FPointerEvent& Event) override
	{
		// Treat as mouse down, ImGui handles double click internally
		return HandleMouseButtonDownEvent(SlateApp, Event);
	}

	virtual bool HandleMouseWheelOrGestureEvent(FSlateApplication& SlateApp, const FPointerEvent& Event, const FPointerEvent* GestureEvent) override
	{
		ImGui::FScopedContextSwitcher ContextSwitcher(Owner->GetContext());

		ImGuiIO& IO = ImGui::GetIO();

		IO.AddMouseWheelEvent(0.0f, Event.GetWheelDelta());

		return IO.WantCaptureMouse;
	}

private:
	SImGuiOverlay* Owner = nullptr;
};

void SImGuiOverlay::Construct(const FArguments& Args)
{
	SetVisibility(EVisibility::HitTestInvisible);

	Context = Args._Context;
	if (!Context.IsValid())
	{
		Context = FImGuiContext::Create();

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

	FSlateBrush TextureBrush;
	for (const FImGuiDrawList& DrawList : DrawData.DrawLists)
	{
		TArray<FSlateVertex> Vertices;
		Vertices.SetNumUninitialized(DrawList.VtxBuffer.Size);
		for (int32 BufferIdx = 0; BufferIdx < Vertices.Num(); ++BufferIdx)
		{
			const ImDrawVert& Vtx = DrawList.VtxBuffer.Data[BufferIdx];
			Vertices[BufferIdx] = FSlateVertex::Make<ESlateVertexRounding::Disabled>(Transform, Vtx.pos, Vtx.uv, FVector2f::UnitVector, ImGui::ConvertColor(Vtx.col));
		}

		TArray<SlateIndex> Indices;
		Indices.SetNumUninitialized(DrawList.IdxBuffer.Size);
		for (int32 BufferIdx = 0; BufferIdx < Indices.Num(); ++BufferIdx)
		{
			Indices[BufferIdx] = DrawList.IdxBuffer.Data[BufferIdx];
		}

		for (const ImDrawCmd& DrawCmd : DrawList.CmdBuffer)
		{
			TArray VerticesSlice(Vertices.GetData() + DrawCmd.VtxOffset, Vertices.Num() - DrawCmd.VtxOffset);
			TArray IndicesSlice(Indices.GetData() + DrawCmd.IdxOffset, DrawCmd.ElemCount);

			UTexture2D* Texture = DrawCmd.GetTexID();
			if (TextureBrush.GetResourceObject() != Texture)
			{
				TextureBrush.SetResourceObject(Texture);
				if (IsValid(Texture))
				{
					TextureBrush.ImageSize.X = Texture->GetSizeX();
					TextureBrush.ImageSize.Y = Texture->GetSizeY();
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

			FSlateRect ClipRect(DrawCmd.ClipRect.x, DrawCmd.ClipRect.y, DrawCmd.ClipRect.z, DrawCmd.ClipRect.w);
			ClipRect = TransformRect(Transform, ClipRect);

			OutDrawElements.PushClip(FSlateClippingZone(ClipRect));
			FSlateDrawElement::MakeCustomVerts(OutDrawElements, LayerId, TextureBrush.GetRenderingResource(), VerticesSlice, IndicesSlice, nullptr, 0, 0);
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
	ImGui::FScopedContextSwitcher ContextSwitcher(Context);

	ImGuiIO& IO = ImGui::GetIO();

	IO.AddInputCharacter(CharCast<ANSICHAR>(Event.GetCharacter()));

	return IO.WantCaptureKeyboard ? FReply::Handled() : FReply::Unhandled();
}

TSharedPtr<FImGuiContext> SImGuiOverlay::GetContext() const
{
	return Context;
}

void SImGuiOverlay::SetDrawData(const ImDrawData* InDrawData)
{
	DrawData = FImGuiDrawData(InDrawData);
}
