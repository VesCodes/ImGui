#pragma once

#include <Engine/Texture2D.h>
#include <Framework/Application/IInputProcessor.h>
#include <UObject/StrongObjectPtr.h>
#include <Widgets/SLeafWidget.h>

#include <imgui.h>
#include <implot.h>

class SImGuiOverlay;

struct FImGuiViewportData
{
	static FImGuiViewportData* GetOrCreate(ImGuiViewport* Viewport);

	TWeakPtr<SWindow> Window = nullptr;
	TWeakPtr<SImGuiOverlay> Overlay = nullptr;
};

struct FImGuiDrawList
{
	FImGuiDrawList() = default;
	explicit FImGuiDrawList(ImDrawList* Source);

	ImVector<ImDrawVert> VtxBuffer;
	ImVector<ImDrawIdx> IdxBuffer;
	ImVector<ImDrawCmd> CmdBuffer;
	ImDrawListFlags Flags = ImDrawListFlags_None;
};

struct FImGuiDrawData
{
	FImGuiDrawData() = default;
	explicit FImGuiDrawData(const ImDrawData* Source);

	bool bValid = false;

	int32 TotalIdxCount = 0;
	int32 TotalVtxCount = 0;

	TArray<FImGuiDrawList> DrawLists;

	FVector2f DisplayPos = FVector2f::ZeroVector;
	FVector2f DisplaySize = FVector2f::ZeroVector;
	FVector2f FrameBufferScale = FVector2f::ZeroVector;
};

class SImGuiOverlay : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SImGuiOverlay)
		{
		}

		SLATE_ARGUMENT_DEFAULT(ImGuiContext*, Context) = nullptr;
	SLATE_END_ARGS()

	void Construct(const FArguments& Args);
	virtual ~SImGuiOverlay() override;

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override;
	virtual bool SupportsKeyboardFocus() const override;
	virtual FReply OnKeyChar(const FGeometry& MyGeometry, const FCharacterEvent& Event) override;

	ImGuiContext* GetContext() const;
	void SetDrawData(const ImDrawData* InDrawData);

private:
	void BeginFrame();
	void EndFrame();

	void OnDisplayMetricsChanged(const FDisplayMetrics& DisplayMetrics) const;

	ImGuiContext* Context = nullptr;
	ImPlotContext* PlotContext = nullptr;

	bool bContextOwner = false;

	char IniFilenameAnsi[1024] = {};
	char LogFilenameAnsi[1024] = {};

	TSharedPtr<IInputProcessor> InputProcessor = nullptr;
	TStrongObjectPtr<UTexture2D> FontAtlasTexturePtr = nullptr;

	FImGuiDrawData DrawData;
};
