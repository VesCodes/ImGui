#pragma once

#include <Templates/SharedPointer.h>

#if WITH_ENGINE
#include <Engine/Texture2D.h>
#include <UObject/StrongObjectPtr.h>
#endif

class SWindow;
class SImGuiOverlay;
struct FDisplayMetrics;
struct FSlateBrush;
struct ImGuiContext;
struct ImGuiViewport;
struct ImPlotContext;

struct IMGUI_API FImGuiViewportData
{
	/// Returns the existing viewport data or creates one
	static FImGuiViewportData* GetOrCreate(ImGuiViewport* Viewport);

	TWeakPtr<SWindow> Window = nullptr;
	TWeakPtr<SImGuiOverlay> Overlay = nullptr;
};

class IMGUI_API FImGuiContext : public TSharedFromThis<FImGuiContext>
{
public:
	/// Creates a managed ImGui context
	static TSharedRef<FImGuiContext> Create();

	/// Returns an existing managed ImGui context
	static TSharedPtr<FImGuiContext> Get(const ImGuiContext* Context);

	~FImGuiContext();

	/// Begins a new frame
	void BeginFrame();

	/// Ends the current frame
	void EndFrame();

	/// Listens for remote connections
	bool Listen(int16 Port);

	/// Connects to a remote host
	bool Connect(const FString& Host, int16 Port);

	/// Closes all remote connections
	void Disconnect();

	/// Access to the underlying ImGui context
	operator ImGuiContext*() const;

	/// Access to the underlying ImPlot context
	operator ImPlotContext*() const;

private:
	void Initialize();

	void OnDisplayMetricsChanged(const FDisplayMetrics& DisplayMetrics);

	ImGuiContext* Context = nullptr;
	ImPlotContext* PlotContext = nullptr;

	char IniFilenameAnsi[1024] = {};
	char LogFilenameAnsi[1024] = {};
	bool bIsRemote = false;

#if WITH_ENGINE
	TStrongObjectPtr<UTexture2D> FontAtlasTexturePtr = nullptr;
#else
	TSharedPtr<FSlateBrush> FontAtlasTexturePtr = nullptr;
#endif
};
