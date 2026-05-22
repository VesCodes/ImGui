#pragma once

#ifndef IMGUI_DISABLE

#include <Widgets/SWindow.h>

struct ImGuiViewport;
class FImGuiContext;

class SImGuiWindow : public SWindow
{
	SLATE_BEGIN_ARGS(SImGuiWindow)
		{
		}

		SLATE_ARGUMENT(ImGuiViewport*, Viewport);
		SLATE_ARGUMENT(TSharedPtr<FImGuiContext>, Context);
		SLATE_DEFAULT_SLOT(FArguments, Content);
	SLATE_END_ARGS()

	void Construct(const FArguments& Args);

	virtual bool OnIsActiveChanged(const FWindowActivateEvent& ActivateEvent) override;

	virtual FCursorReply OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const override;

private:
	TSharedPtr<FImGuiContext> Context = nullptr;
};

#endif // #ifndef IMGUI_DISABLE
