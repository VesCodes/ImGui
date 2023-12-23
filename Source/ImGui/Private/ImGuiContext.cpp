#include "ImGuiContext.h"

#include <Framework/Application/SlateApplication.h>
#include <HAL/LowLevelMemTracker.h>
#include <HAL/UnrealMemory.h>
#include <Widgets/SWindow.h>

#if WITH_ENGINE
#include <TextureResource.h>
#endif

#include <imgui.h>
#include <imgui_internal.h>
#include <implot.h>

#include "SImGuiOverlay.h"

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

static void* ImGui_MemAlloc(size_t Size, void* UserData)
{
	LLM_SCOPE_BYNAME(TEXT("ImGui"));
	return FMemory::Malloc(Size);
}

static void ImGui_MemFree(void* Ptr, void* UserData)
{
	FMemory::Free(Ptr);
}

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
				SAssignNew(ViewportData->Overlay, SImGuiOverlay)
				.Context(FImGuiContext::Get(ImGui::GetCurrentContext()))
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

TSharedRef<FImGuiContext> FImGuiContext::Create()
{
	TSharedRef<FImGuiContext> Context = MakeShared<FImGuiContext>();
	Context->Initialize();

	return Context;
}

TSharedPtr<FImGuiContext> FImGuiContext::Get(ImGuiContext* Context)
{
	if (Context && Context->IO.UserData)
	{
		// #TODO(Ves): Should probably track managed contexts internally, this is dodgy
		return static_cast<FImGuiContext*>(Context->IO.UserData)->AsShared();
	}

	return nullptr;
}

void FImGuiContext::Initialize()
{
	ImGui::SetAllocatorFunctions(ImGui_MemAlloc, ImGui_MemFree);

	IMGUI_CHECKVERSION();

	Context = ImGui::CreateContext();
	PlotContext = ImPlot::CreateContext();

	ImGui::FScopedContext ScopedContext(AsShared());

	ImGuiIO& IO = ImGui::GetIO();
	IO.UserData = this;

	IO.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	IO.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
	IO.ConfigFlags |= ImGuiConfigFlags_NavEnableSetMousePos;
	IO.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

	IO.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;
	IO.BackendFlags |= ImGuiBackendFlags_HasSetMousePos;
	IO.BackendFlags |= ImGuiBackendFlags_PlatformHasViewports;
	IO.BackendFlags |= ImGuiBackendFlags_RendererHasViewports;
	IO.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;

	// Ensure each PIE session has a uniquely identifiable context
	const FString ContextName = (GPlayInEditorID > 0 ? FString::Printf(TEXT("ImGui_%d"), static_cast<int32>(GPlayInEditorID)) : TEXT("ImGui"));

	const FString IniFilename = FPaths::GeneratedConfigDir() / FPlatformProperties::PlatformName() / ContextName + TEXT(".ini");
	FCStringAnsi::Strncpy(IniFilenameAnsi, TCHAR_TO_ANSI(*IniFilename), UE_ARRAY_COUNT(IniFilenameAnsi));
	IO.IniFilename = IniFilenameAnsi;

	const FString LogFilename = FPaths::ProjectLogDir() / ContextName + TEXT(".log");
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

	if (FSlateApplication::IsInitialized())
	{
		// Enable multi-viewports support for Slate applications
		IO.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

		if (const TSharedPtr<GenericApplication> PlatformApplication = FSlateApplication::Get().GetPlatformApplication())
		{
			FDisplayMetrics DisplayMetrics;
			PlatformApplication->GetInitialDisplayMetrics(DisplayMetrics);
			PlatformApplication->OnDisplayMetricsChanged().AddSP(this, &FImGuiContext::OnDisplayMetricsChanged);
			OnDisplayMetricsChanged(DisplayMetrics);
		}
	}

	// Ensure main viewport data is created ahead of time
	FImGuiViewportData::GetOrCreate(ImGui::GetMainViewport());

	TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateSP(this, &FImGuiContext::OnTick));
}

FImGuiContext::~FImGuiContext()
{
	FTSTicker::RemoveTicker(TickHandle);

	if (FSlateApplication::IsInitialized())
	{
		if (const TSharedPtr<GenericApplication> PlatformApplication = FSlateApplication::Get().GetPlatformApplication())
		{
			PlatformApplication->OnDisplayMetricsChanged().RemoveAll(this);
		}
	}

	if (PlotContext)
	{
		ImPlot::DestroyContext(PlotContext);
		PlotContext = nullptr;
	}

	if (Context)
	{
		ImGui::DestroyContext(Context);
		Context = nullptr;
	}
}

FImGuiContext::operator ImGuiContext*() const
{
	return Context;
}

FImGuiContext::operator ImPlotContext*() const
{
	return PlotContext;
}

void FImGuiContext::OnDisplayMetricsChanged(const FDisplayMetrics& DisplayMetrics) const
{
	ImGui::FScopedContext ScopedContext(AsShared());

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

bool FImGuiContext::OnTick(float DeltaTime)
{
	EndFrame();
	BeginFrame();

	return true;
}

void FImGuiContext::BeginFrame()
{
	if (Context->WithinFrameScope)
	{
		return;
	}

	ImGui::FScopedContext ScopedContext(AsShared());

	ImGuiIO& IO = ImGui::GetIO();

	IO.DeltaTime = FApp::GetDeltaTime();
	IO.DisplaySize = ImGui_GetWindowSize(ImGui::GetMainViewport());

	if (!IO.Fonts->IsBuilt() || !FontAtlasTexturePtr.IsValid())
	{
		uint8* TextureDataRaw;
		int32 TextureWidth, TextureHeight, BytesPerPixel;
		IO.Fonts->GetTexDataAsRGBA32(&TextureDataRaw, &TextureWidth, &TextureHeight, &BytesPerPixel);

#if WITH_ENGINE
		UTexture2D* FontAtlasTexture = UTexture2D::CreateTransient(TextureWidth, TextureHeight, PF_R8G8B8A8, TEXT("ImGuiFontAtlas"));
		FontAtlasTexture->Filter = TF_Bilinear;
		FontAtlasTexture->AddressX = TA_Wrap;
		FontAtlasTexture->AddressY = TA_Wrap;

		uint8* FontAtlasTextureData = static_cast<uint8*>(FontAtlasTexture->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE));
		FMemory::Memcpy(FontAtlasTextureData, TextureDataRaw, TextureWidth * TextureHeight * BytesPerPixel);
		FontAtlasTexture->GetPlatformData()->Mips[0].BulkData.Unlock();
		FontAtlasTexture->UpdateResource();

		FontAtlasTexturePtr.Reset(FontAtlasTexture);
#else
		FontAtlasTexturePtr = FSlateDynamicImageBrush::CreateWithImageData(
			TEXT("ImGuiFontAtlas"), FVector2D(TextureWidth, TextureHeight),
			TArray(TextureDataRaw, TextureWidth * TextureHeight * BytesPerPixel));
#endif

		IO.Fonts->SetTexID(FontAtlasTexturePtr.Get());
	}

	ImGui::NewFrame();
}

void FImGuiContext::EndFrame() const
{
	if (!Context->WithinFrameScope)
	{
		return;
	}

	ImGui::FScopedContext ScopedContext(AsShared());

	ImGui::Render();
	ImGui_RenderWindow(ImGui::GetMainViewport(), nullptr);

	ImGui::UpdatePlatformWindows();
	ImGui::RenderPlatformWindowsDefault();
}
