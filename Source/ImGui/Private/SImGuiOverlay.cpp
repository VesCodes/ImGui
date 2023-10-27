#include "SImGuiOverlay.h"

#include <TextureResource.h>
#include <Engine/Engine.h>
#include <Engine/Texture2D.h>
#include <Framework/Application/SlateApplication.h>
#include <Misc/App.h>

static void ImGui_CreateWindow(ImGuiViewport* Viewport)
{
	FImGuiViewportData* ViewportData = FImGuiViewportData::GetOrCreate(Viewport);
	if (ViewportData)
	{
		const FImGuiViewportData* MainViewportData = FImGuiViewportData::GetOrCreate(ImGui::GetMainViewport());
		const TSharedPtr<SWindow> ParentWindow = MainViewportData ? MainViewportData->Window.Pin() : nullptr;

		const bool bTooltipWindow = (Viewport->Flags & ImGuiViewportFlags_TopMost);
		const bool bPopupWindow = (Viewport->Flags & ImGuiViewportFlags_NoTaskBarIcon);

		// #TODO(Ves): Still blits a black background in the window frame :(
		static FWindowStyle WindowStyle = FWindowStyle()
		                                  .SetActiveTitleBrush(FSlateNoResource())
		                                  .SetInactiveTitleBrush(FSlateNoResource())
		                                  .SetFlashTitleBrush(FSlateNoResource())
		                                  .SetOutlineBrush(FSlateNoResource())
		                                  .SetBorderBrush(FSlateNoResource())
		                                  .SetBackgroundBrush(FSlateNoResource())
		                                  .SetChildBackgroundBrush(FSlateNoResource());

		const TSharedRef<SWindow> Window =
			SAssignNew(ViewportData->Window, SWindow)
			.Type(bTooltipWindow ? EWindowType::ToolTip : EWindowType::Normal)
			.Style(&WindowStyle)
			.ScreenPosition(FVector2f(Viewport->Pos))
			.ClientSize(FVector2f(Viewport->Size))
			.SupportsTransparency(EWindowTransparency::PerWindow)
			.SizingRule(ESizingRule::UserSized)
			.IsPopupWindow(bTooltipWindow || bPopupWindow)
			.IsTopmostWindow(bTooltipWindow)
			.FocusWhenFirstShown(!(Viewport->Flags & ImGuiViewportFlags_NoFocusOnAppearing))
			.HasCloseButton(false)
			.SupportsMaximize(false)
			.SupportsMinimize(false)
			.CreateTitleBar(false)
			.LayoutBorder(0)
			.UserResizeBorder(0)
			.UseOSWindowBorder(false)
			[
				SAssignNew(ViewportData->Overlay, SImGuiOverlay).Context(ImGui::GetCurrentContext())
			];

		if (ParentWindow.IsValid())
		{
			FSlateApplication::Get().AddWindowAsNativeChild(Window, ParentWindow.ToSharedRef());
		}
		else
		{
			FSlateApplication::Get().AddWindow(Window);
		}
	}
}

static void ImGui_DestroyWindow(ImGuiViewport* Viewport)
{
	const FImGuiViewportData* ViewportData = FImGuiViewportData::GetOrCreate(Viewport);
	if (ViewportData)
	{
		if (!(Viewport->Flags & ImGuiViewportFlags_OwnedByApp))
		{
			if (const TSharedPtr<SWindow> Window = ViewportData->Window.Pin())
			{
				Window->RequestDestroyWindow();
			}
		}

		Viewport->PlatformUserData = nullptr;
		delete ViewportData;
	}
}

static void ImGui_ShowWindow(ImGuiViewport* Viewport)
{
	const FImGuiViewportData* ViewportData = FImGuiViewportData::GetOrCreate(Viewport);
	if (ViewportData)
	{
		if (const TSharedPtr<SWindow> Window = ViewportData->Window.Pin())
		{
			Window->ShowWindow();
		}
	}
}

static void ImGui_SetWindowPos(ImGuiViewport* Viewport, ImVec2 Pos)
{
	const FImGuiViewportData* ViewportData = FImGuiViewportData::GetOrCreate(Viewport);
	if (ViewportData)
	{
		if (const TSharedPtr<SWindow> Window = ViewportData->Window.Pin())
		{
			Window->MoveWindowTo(FVector2f(Pos));
		}
	}
}

static ImVec2 ImGui_GetWindowPos(ImGuiViewport* Viewport)
{
	const FImGuiViewportData* ViewportData = FImGuiViewportData::GetOrCreate(Viewport);
	if (ViewportData)
	{
		if (const TSharedPtr<SImGuiOverlay> Overlay = ViewportData->Overlay.Pin())
		{
			return Overlay->GetTickSpaceGeometry().GetAbsolutePosition();
		}
	}

	return FVector2f::ZeroVector;
}

static void ImGui_SetWindowSize(ImGuiViewport* Viewport, ImVec2 Size)
{
	const FImGuiViewportData* ViewportData = FImGuiViewportData::GetOrCreate(Viewport);
	if (ViewportData)
	{
		if (const TSharedPtr<SWindow> Window = ViewportData->Window.Pin())
		{
			Window->Resize(FVector2f(Size));
		}
	}
}

static ImVec2 ImGui_GetWindowSize(ImGuiViewport* Viewport)
{
	const FImGuiViewportData* ViewportData = FImGuiViewportData::GetOrCreate(Viewport);
	if (ViewportData)
	{
		if (const TSharedPtr<SImGuiOverlay> Overlay = ViewportData->Overlay.Pin())
		{
			return Overlay->GetTickSpaceGeometry().GetAbsoluteSize();
		}
	}

	return FVector2f::ZeroVector;
}

static void ImGui_SetWindowFocus(ImGuiViewport* Viewport)
{
	const FImGuiViewportData* ViewportData = FImGuiViewportData::GetOrCreate(Viewport);
	if (ViewportData)
	{
		if (const TSharedPtr<SWindow> Window = ViewportData->Window.Pin())
		{
			if (const TSharedPtr<FGenericWindow> NativeWindow = Window->GetNativeWindow())
			{
				NativeWindow->BringToFront();
				NativeWindow->SetWindowFocus();
			}
		}
	}
}

static bool ImGui_GetWindowFocus(ImGuiViewport* Viewport)
{
	const FImGuiViewportData* ViewportData = FImGuiViewportData::GetOrCreate(Viewport);
	if (ViewportData)
	{
		if (const TSharedPtr<SWindow> Window = ViewportData->Window.Pin())
		{
			if (const TSharedPtr<FGenericWindow> NativeWindow = Window->GetNativeWindow())
			{
				return NativeWindow->IsForegroundWindow();
			}
		}
	}

	return false;
}

static bool ImGui_GetWindowMinimized(ImGuiViewport* Viewport)
{
	const FImGuiViewportData* ViewportData = FImGuiViewportData::GetOrCreate(Viewport);
	if (ViewportData)
	{
		if (const TSharedPtr<SWindow> Window = ViewportData->Window.Pin())
		{
			return Window->IsWindowMinimized();
		}
	}

	return false;
}

static void ImGui_SetWindowTitle(ImGuiViewport* Viewport, const char* TitleAnsi)
{
	const FImGuiViewportData* ViewportData = FImGuiViewportData::GetOrCreate(Viewport);
	if (ViewportData)
	{
		if (const TSharedPtr<SWindow> Window = ViewportData->Window.Pin())
		{
			Window->SetTitle(FText::FromString(ANSI_TO_TCHAR(TitleAnsi)));
		}
	}
}

static void ImGui_SetWindowAlpha(ImGuiViewport* Viewport, float Alpha)
{
	const FImGuiViewportData* ViewportData = FImGuiViewportData::GetOrCreate(Viewport);
	if (ViewportData)
	{
		if (const TSharedPtr<SWindow> Window = ViewportData->Window.Pin())
		{
			Window->SetOpacity(Alpha);
		}
	}
}

static void ImGui_RenderWindow(ImGuiViewport* Viewport, void* Data)
{
	const FImGuiViewportData* ViewportData = FImGuiViewportData::GetOrCreate(Viewport);
	if (ViewportData)
	{
		if (const TSharedPtr<SImGuiOverlay> Overlay = ViewportData->Overlay.Pin())
		{
			Overlay->SetDrawData(Viewport->DrawData);
		}
	}
}

FImGuiViewportData* FImGuiViewportData::GetOrCreate(ImGuiViewport* Viewport)
{
	if (!Viewport)
	{
		return nullptr;
	}

	FImGuiViewportData* ViewportData = static_cast<FImGuiViewportData*>(Viewport->PlatformUserData);
	if (!ViewportData)
	{
		ViewportData = new FImGuiViewportData();
		Viewport->PlatformUserData = ViewportData;
	}

	return ViewportData;
}

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
	if (!Context)
	{
		IMGUI_CHECKVERSION();

		Context = ImGui::CreateContext();
		PlotContext = ImPlot::CreateContext();

		bContextOwner = true;

		ImGui::FScopedContextSwitcher ContextSwitcher(Context);

		ImGuiIO& IO = ImGui::GetIO();

		IO.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
		IO.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
		IO.ConfigFlags |= ImGuiConfigFlags_NavEnableSetMousePos;
		IO.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
		IO.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

		IO.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;
		IO.BackendFlags |= ImGuiBackendFlags_HasSetMousePos;
		IO.BackendFlags |= ImGuiBackendFlags_PlatformHasViewports;
		IO.BackendFlags |= ImGuiBackendFlags_RendererHasViewports;
		IO.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;

		const FString IniFilename = FPaths::GeneratedConfigDir() / FPlatformProperties::PlatformName() / TEXT("ImGui.ini");
		FCStringAnsi::Strncpy(IniFilenameAnsi, TCHAR_TO_ANSI(*IniFilename), UE_ARRAY_COUNT(IniFilenameAnsi));
		IO.IniFilename = IniFilenameAnsi;

		const FString LogFilename = FPaths::ProjectLogDir() / (GPlayInEditorID != INDEX_NONE ? FString::Printf(TEXT("ImGui_%d.log"), static_cast<int32>(GPlayInEditorID)) : TEXT("ImGui.log"));
		FCStringAnsi::Strncpy(LogFilenameAnsi, TCHAR_TO_ANSI(*LogFilename), UE_ARRAY_COUNT(LogFilenameAnsi));
		IO.LogFilename = LogFilenameAnsi;

		ImGuiPlatformIO& PlatformIO = ImGui::GetPlatformIO();

		PlatformIO.Platform_CreateWindow = ImGui_CreateWindow;
		PlatformIO.Platform_DestroyWindow = ImGui_DestroyWindow;
		PlatformIO.Platform_ShowWindow = ImGui_ShowWindow;
		PlatformIO.Platform_SetWindowPos = ImGui_SetWindowPos;
		PlatformIO.Platform_GetWindowPos = ImGui_GetWindowPos;
		PlatformIO.Platform_SetWindowSize = ImGui_SetWindowSize;
		PlatformIO.Platform_GetWindowSize = ImGui_GetWindowSize;
		PlatformIO.Platform_SetWindowFocus = ImGui_SetWindowFocus;
		PlatformIO.Platform_GetWindowFocus = ImGui_GetWindowFocus;
		PlatformIO.Platform_GetWindowMinimized = ImGui_GetWindowMinimized;
		PlatformIO.Platform_SetWindowTitle = ImGui_SetWindowTitle;
		PlatformIO.Platform_SetWindowAlpha = ImGui_SetWindowAlpha;
		PlatformIO.Platform_RenderWindow = ImGui_RenderWindow;

		const FString FontPath = FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf");
		IO.Fonts->AddFontFromFileTTF(TCHAR_TO_ANSI(*FontPath), 16);

		InputProcessor = MakeShared<FImGuiInputProcessor>(this);
		FSlateApplication::Get().RegisterInputPreProcessor(InputProcessor.ToSharedRef(), 0);

		if (const TSharedPtr<GenericApplication> PlatformApplication = FSlateApplication::Get().GetPlatformApplication())
		{
			FDisplayMetrics DisplayMetrics;
			PlatformApplication->GetInitialDisplayMetrics(DisplayMetrics);
			PlatformApplication->OnDisplayMetricsChanged().AddSP(this, &SImGuiOverlay::OnDisplayMetricsChanged);
			OnDisplayMetricsChanged(DisplayMetrics);
		}

		FCoreDelegates::OnBeginFrame.AddSP(this, &SImGuiOverlay::BeginFrame);
		FCoreDelegates::OnEndFrame.AddSP(this, &SImGuiOverlay::EndFrame);

		if (GFrameCounter != 0)
		{
			// Create viewport data so ImGui can start immediately when midway through a frame
			FImGuiViewportData::GetOrCreate(ImGui::GetMainViewport());
			BeginFrame();
		}
	}
}

SImGuiOverlay::~SImGuiOverlay()
{
	if (FSlateApplication::IsInitialized())
	{
		if (InputProcessor.IsValid())
		{
			FSlateApplication::Get().UnregisterInputPreProcessor(InputProcessor);
			InputProcessor.Reset();
		}

		if (const TSharedPtr<GenericApplication> PlatformApplication = FSlateApplication::Get().GetPlatformApplication())
		{
			PlatformApplication->OnDisplayMetricsChanged().RemoveAll(this);
		}
	}

	FCoreDelegates::OnBeginFrame.RemoveAll(this);
	FCoreDelegates::OnEndFrame.RemoveAll(this);

	if (bContextOwner)
	{
		ImPlot::DestroyContext(PlotContext);
		ImGui::DestroyContext(Context);
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

ImGuiContext* SImGuiOverlay::GetContext() const
{
	return Context;
}

void SImGuiOverlay::SetDrawData(const ImDrawData* InDrawData)
{
	DrawData = FImGuiDrawData(InDrawData);
}

void SImGuiOverlay::BeginFrame()
{
	ImGui::FScopedContextSwitcher ContextSwitcher(Context);

	ImGuiIO& IO = ImGui::GetIO();

	IO.DeltaTime = FApp::GetDeltaTime();
	IO.DisplaySize = GetTickSpaceGeometry().GetAbsoluteSize();

	if (!IO.Fonts->IsBuilt() || !FontAtlasTexturePtr.IsValid())
	{
		uint8* TextureDataRaw;
		int32 TextureWidth, TextureHeight, BytesPerPixel;
		IO.Fonts->GetTexDataAsRGBA32(&TextureDataRaw, &TextureWidth, &TextureHeight, &BytesPerPixel);

		UTexture2D* FontAtlasTexture = UTexture2D::CreateTransient(TextureWidth, TextureHeight, PF_R8G8B8A8, TEXT("ImGuiFontAtlas"));
		FontAtlasTexture->Filter = TF_Bilinear;
		FontAtlasTexture->AddressX = TA_Wrap;
		FontAtlasTexture->AddressY = TA_Wrap;

		uint8* FontAtlasTextureData = static_cast<uint8*>(FontAtlasTexture->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE));
		FMemory::Memcpy(FontAtlasTextureData, TextureDataRaw, TextureWidth * TextureHeight * BytesPerPixel);
		FontAtlasTexture->GetPlatformData()->Mips[0].BulkData.Unlock();
		FontAtlasTexture->UpdateResource();

		IO.Fonts->SetTexID(FontAtlasTexture);
		FontAtlasTexturePtr.Reset(FontAtlasTexture);
	}

	ImGui::NewFrame();
}

void SImGuiOverlay::EndFrame()
{
	ImGui::FScopedContextSwitcher ContextSwitcher(Context);

	ImGui::Render();
	SetDrawData(ImGui::GetDrawData());

	ImGui::UpdatePlatformWindows();
	ImGui::RenderPlatformWindowsDefault();
}

void SImGuiOverlay::OnDisplayMetricsChanged(const FDisplayMetrics& DisplayMetrics) const
{
	ImGui::FScopedContextSwitcher ContextSwitcher(Context);

	ImGuiPlatformIO& PlatformIO = ImGui::GetPlatformIO();
	PlatformIO.Monitors.resize(0);

	for (const FMonitorInfo& Monitor : DisplayMetrics.MonitorInfo)
	{
		ImGuiPlatformMonitor ImGuiMonitor;
		ImGuiMonitor.MainPos = FIntPoint(Monitor.DisplayRect.Left, Monitor.DisplayRect.Top);
		ImGuiMonitor.MainSize = FIntPoint(Monitor.DisplayRect.Right - Monitor.DisplayRect.Left, Monitor.DisplayRect.Bottom - Monitor.DisplayRect.Top);
		ImGuiMonitor.WorkPos = FIntPoint(Monitor.WorkArea.Left, Monitor.WorkArea.Top);
		ImGuiMonitor.WorkSize = FIntPoint(Monitor.WorkArea.Right - Monitor.WorkArea.Left, Monitor.WorkArea.Bottom - Monitor.WorkArea.Top);
		ImGuiMonitor.DpiScale = Monitor.DPI;

		if (Monitor.bIsPrimary)
		{
			PlatformIO.Monitors.push_front(ImGuiMonitor);
		}
		else
		{
			PlatformIO.Monitors.push_back(ImGuiMonitor);
		}
	}
}
