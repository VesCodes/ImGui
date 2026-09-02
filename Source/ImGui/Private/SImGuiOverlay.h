#pragma once

#ifndef IMGUI_DISABLE

#include <Framework/Application/IInputProcessor.h>
#include <Widgets/SLeafWidget.h>

#include "ImGuiDrawData.h"

class FImGuiSlateElement;

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
	void SetDrawData(ImDrawData* InDrawData);

private:
	TSharedPtr<FImGuiContext> Context = nullptr;
	TSharedPtr<IInputProcessor> InputProcessor = nullptr;

#if WITH_IMGUI_NATIVE_RENDERING
	/// Number of Slate elements to rotate between; two are enough to keep the render thread from
	/// reading the element the game thread is writing to, as it never trails by more than a frame.
	static constexpr int32 SlateElementCount = 2;

	TStaticArray<TSharedPtr<FImGuiSlateElement>, SlateElementCount> SlateElements;
	int32 CurrentSlateElementIdx = 0;
#else
	FImGuiDrawData DrawData;
#endif
};

#endif // #ifndef IMGUI_DISABLE
