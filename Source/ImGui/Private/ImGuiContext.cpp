#include "ImGuiContext.h"

#include <Framework/Application/SlateApplication.h>
#include <HAL/LowLevelMemTracker.h>
#include <HAL/PlatformApplicationMisc.h>
#include <HAL/PlatformProcess.h>
#include <HAL/PlatformString.h>
#include <HAL/UnrealMemory.h>
#include <Misc/EngineVersionComparison.h>
#include <Widgets/SWindow.h>

#if WITH_ENGINE
#include <TextureResource.h>
#endif

THIRD_PARTY_INCLUDES_START
#include <imgui.h>
#include <imgui_internal.h>
#include <implot.h>
#define NETIMGUI_IMPLEMENTATION
#include <NetImGui_Api.h>
THIRD_PARTY_INCLUDES_END

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
		const bool bNoFocusOnAppearing = (Viewport->Flags & ImGuiViewportFlags_NoFocusOnAppearing);

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
			.FocusWhenFirstShown(!bNoFocusOnAppearing)
			.ActivationPolicy(bNoFocusOnAppearing ? EWindowActivationPolicy::Never : EWindowActivationPolicy::Always)
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
				.HandleInput(false)
			];

		if (ParentWindow.IsValid())
		{
			FSlateApplication::Get().AddWindowAsNativeChild(Window, ParentWindow.ToSharedRef());
		}
		else
		{
			FSlateApplication::Get().AddWindow(Window);
		}

		if (!(Viewport->Flags & ImGuiViewportFlags_OwnedByApp))
		{
			FSlateThrottleManager::Get().DisableThrottle(true);
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

			FSlateThrottleManager::Get().DisableThrottle(false);
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

static void ImGui_SetWindowTitle(ImGuiViewport* Viewport, const char* Title)
{
	const FImGuiViewportData* ViewportData = FImGuiViewportData::GetOrCreate(Viewport);
	if (ViewportData)
	{
		if (const TSharedPtr<SWindow> Window = ViewportData->Window.Pin())
		{
			Window->SetTitle(FText::FromString(UTF8_TO_TCHAR(Title)));
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

static void ImGui_RenderWindow(ImGuiViewport* Viewport, void* UserData)
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

const char* ImGui_GetClipboardText(ImGuiContext* Context)
{
	TArray<char>* ClipboardBuffer = static_cast<TArray<char>*>(Context->PlatformIO.Platform_ClipboardUserData);
	if (ClipboardBuffer)
	{
		FString ClipboardText;
		FPlatformApplicationMisc::ClipboardPaste(ClipboardText);

		ClipboardBuffer->SetNumUninitialized(FPlatformString::ConvertedLength<UTF8CHAR>(*ClipboardText));
		FPlatformString::Convert(reinterpret_cast<UTF8CHAR*>(ClipboardBuffer->GetData()), ClipboardBuffer->Num(), *ClipboardText, ClipboardText.Len() + 1);

		return ClipboardBuffer->GetData();
	}

	return nullptr;
}

void ImGui_SetClipboardText(ImGuiContext* Context, const char* ClipboardText)
{
	FPlatformApplicationMisc::ClipboardCopy(UTF8_TO_TCHAR(ClipboardText));
}

static bool ImGui_OpenInShell(ImGuiContext* Context, const char* Path)
{
	return FPlatformProcess::LaunchFileInDefaultExternalApplication(UTF8_TO_TCHAR(Path));
}

TSharedRef<FImGuiContext> FImGuiContext::Create()
{
	TSharedRef<FImGuiContext> Context = MakeShared<FImGuiContext>();
	Context->Initialize();

	return Context;
}

TSharedPtr<FImGuiContext> FImGuiContext::Get(const ImGuiContext* Context)
{
	if (Context && Context->IO.UserData)
	{
		// Expects that the provided context is owned by a live FImGuiContext
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

	NetImgui::Startup();

	ImGuiIO& IO = ImGui::GetIO();
	IO.UserData = this;

	IO.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	IO.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
	IO.ConfigFlags |= ImGuiConfigFlags_NavEnableSetMousePos;
	IO.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	IO.ConfigFlags |= ImGuiConfigFlags_DpiEnableScaleViewports;

	IO.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;
	IO.BackendFlags |= ImGuiBackendFlags_HasSetMousePos;
	IO.BackendFlags |= ImGuiBackendFlags_PlatformHasViewports;
	IO.BackendFlags |= ImGuiBackendFlags_RendererHasViewports;
	IO.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;

#if UE_VERSION_OLDER_THAN(5, 5, 0)
	const int32 PieSessionId = GPlayInEditorID;
#else
	const int32 PieSessionId = UE::GetPlayInEditorID();
#endif

	// Ensure each PIE session has a uniquely identifiable context
	const FString ContextName = (PieSessionId > 0 ? FString::Printf(TEXT("ImGui_%d"), PieSessionId) : TEXT("ImGui"));

	const FString IniFilename = FPaths::GeneratedConfigDir() / FPlatformProperties::PlatformName() / ContextName + TEXT(".ini");
	FPlatformString::Convert(reinterpret_cast<UTF8CHAR*>(IniFilenameUtf8), UE_ARRAY_COUNT(IniFilenameUtf8), *IniFilename, IniFilename.Len() + 1);
	IO.IniFilename = IniFilenameUtf8;

	const FString LogFilename = FPaths::ProjectLogDir() / ContextName + TEXT(".log");
	FPlatformString::Convert(reinterpret_cast<UTF8CHAR*>(LogFilenameUtf8), UE_ARRAY_COUNT(LogFilenameUtf8), *LogFilename, LogFilename.Len() + 1);
	IO.LogFilename = LogFilenameUtf8;

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

	PlatformIO.Platform_ClipboardUserData = &ClipboardBuffer;
	PlatformIO.Platform_GetClipboardTextFn = ImGui_GetClipboardText;
	PlatformIO.Platform_SetClipboardTextFn = ImGui_SetClipboardText;
	PlatformIO.Platform_OpenInShellFn = ImGui_OpenInShell;

	const FString FontPath = FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf");
	if (FPaths::FileExists(*FontPath))
	{
		IO.Fonts->AddFontFromFileTTF(TCHAR_TO_UTF8(*FontPath), 16);
	}

	if (FSlateApplication::IsInitialized())
	{
		// Enable multi-viewports support for Slate applications
		IO.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

		if (const TSharedPtr<GenericApplication> PlatformApplication = FSlateApplication::Get().GetPlatformApplication())
		{
			FDisplayMetrics DisplayMetrics;
			FDisplayMetrics::RebuildDisplayMetrics(DisplayMetrics);
			PlatformApplication->OnDisplayMetricsChanged().AddSP(this, &FImGuiContext::OnDisplayMetricsChanged);
			OnDisplayMetricsChanged(DisplayMetrics);
		}
	}

	// Ensure main viewport data is created ahead of time
	FImGuiViewportData::GetOrCreate(ImGui::GetMainViewport());

	FCoreDelegates::OnBeginFrame.AddSP(this, &FImGuiContext::BeginFrame);
	FCoreDelegates::OnEndFrame.AddSP(this, &FImGuiContext::EndFrame);
}

FImGuiContext::~FImGuiContext()
{
	FCoreDelegates::OnBeginFrame.RemoveAll(this);
	FCoreDelegates::OnEndFrame.RemoveAll(this);

	if (FSlateApplication::IsInitialized())
	{
		if (const TSharedPtr<GenericApplication> PlatformApplication = FSlateApplication::Get().GetPlatformApplication())
		{
			PlatformApplication->OnDisplayMetricsChanged().RemoveAll(this);
		}
	}

	NetImgui::Shutdown();

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

bool FImGuiContext::Listen(int16 Port)
{
	ImGui::FScopedContext ScopedContext(AsShared());

	TAnsiStringBuilder<128> ClientName;
	ClientName << FApp::GetProjectName();

#if UE_VERSION_OLDER_THAN(5, 5, 0)
	const int32 PieSessionId = GPlayInEditorID;
#else
	const int32 PieSessionId = UE::GetPlayInEditorID();
#endif

	if (PieSessionId > 0)
	{
		ClientName << " (" << PieSessionId << ")";
	}

	NetImgui::ConnectFromApp(ClientName.ToString(), Port);
	bIsRemote = true;

	return true;
}

bool FImGuiContext::Connect(const FString& Host, int16 Port)
{
	ImGui::FScopedContext ScopedContext(AsShared());

	TAnsiStringBuilder<128> ClientName;
	ClientName << FApp::GetProjectName();

#if UE_VERSION_OLDER_THAN(5, 5, 0)
	const int32 PieSessionId = GPlayInEditorID;
#else
	const int32 PieSessionId = UE::GetPlayInEditorID();
#endif

	if (PieSessionId > 0)
	{
		ClientName << " (" << PieSessionId << ")";
	}

	NetImgui::ConnectToApp(ClientName.ToString(), TCHAR_TO_ANSI(*Host), Port);
	bIsRemote = true;

	return true;
}

void FImGuiContext::Disconnect()
{
	if (bIsRemote)
	{
		NetImgui::Disconnect();
		bIsRemote = false;
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

void FImGuiContext::OnDisplayMetricsChanged(const FDisplayMetrics& DisplayMetrics)
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
		ImGuiMonitor.DpiScale = Monitor.DPI / 96.0f;

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

void FImGuiContext::BeginFrame()
{
	if (Context->WithinFrameScope || (bIsRemote && !NetImgui::IsConnected()))
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

void FImGuiContext::EndFrame()
{
	if (!Context->WithinFrameScope)
	{
		return;
	}

	ImGui::FScopedContext ScopedContext(AsShared());

	ImGui::Render();
	ImGui::UpdatePlatformWindows();

	if (!bIsRemote)
	{
		ImGui_RenderWindow(ImGui::GetMainViewport(), nullptr);
		ImGui::RenderPlatformWindowsDefault();
	}
}
