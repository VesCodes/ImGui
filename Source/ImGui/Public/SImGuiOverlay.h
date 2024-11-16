#pragma once

#include <Framework/Application/IInputProcessor.h>
#include <Widgets/SLeafWidget.h>

#include <imgui.h>

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

		SLATE_ARGUMENT(TSharedPtr<FImGuiContext>, Context);
		SLATE_ARGUMENT_DEFAULT(bool, HandleInput) = true;
	SLATE_END_ARGS()

	void Construct(const FArguments& Args);
	virtual ~SImGuiOverlay() override;

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override;
	virtual bool SupportsKeyboardFocus() const override;
	virtual FReply OnKeyChar(const FGeometry& MyGeometry, const FCharacterEvent& Event) override;

	TSharedPtr<FImGuiContext> GetContext() const;
	void SetDrawData(const ImDrawData* InDrawData);

private:
	TSharedPtr<FImGuiContext> Context = nullptr;
	TSharedPtr<IInputProcessor> InputProcessor = nullptr;
	FImGuiDrawData DrawData;
};
