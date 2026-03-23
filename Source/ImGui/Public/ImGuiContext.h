#pragma once

#ifndef IMGUI_DISABLE

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
struct ImTextureData;

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

#if WITH_NETIMGUI
	/// Listens for remote connections
	bool Listen(uint16 Port);

	/// Connects to a remote host
	bool Connect(const FString& Host, int16 Port);

	/// Closes all remote connections
	void Disconnect();
#endif

	/// Access to the underlying ImGui context
	operator ImGuiContext*() const;

#if WITH_IMPLOT
	/// Access to the underlying ImPlot context
	operator ImPlotContext*() const;
#endif

private:
	void Initialize();

	void OnDisplayMetricsChanged(const FDisplayMetrics& DisplayMetrics);

	void CreateTexture(ImTextureData* TextureData);
	void UpdateTexture(ImTextureData* TextureData);
	void DestroyTexture(ImTextureData* TextureData);

	ImGuiContext* Context = nullptr;

#if WITH_IMPLOT
	ImPlotContext* PlotContext = nullptr;
#endif

	char IniFilenameUtf8[1024] = {};
	char LogFilenameUtf8[1024] = {};
	TArray<char> ClipboardBuffer;

#if WITH_NETIMGUI
	bool bIsRemote = false;
#endif

#if WITH_ENGINE
	using FTextureRef = TStrongObjectPtr<UTexture>;
#else
	using FTextureRef = TSharedPtr<FSlateBrush>;
#endif

	TArray<FTextureRef> Textures;
};

#endif // #ifndef IMGUI_DISABLE
