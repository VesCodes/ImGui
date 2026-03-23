#include "SImGuiWindow.h"

#ifndef IMGUI_DISABLE

THIRD_PARTY_INCLUDES_START
#include <imgui.h>
THIRD_PARTY_INCLUDES_END

void SImGuiWindow::Construct(const FArguments& Args)
{
	const bool bTooltipWindow = (Args._Viewport->Flags & ImGuiViewportFlags_TopMost);
	const bool bPopupWindow = (Args._Viewport->Flags & ImGuiViewportFlags_NoTaskBarIcon);
	const bool bNoFocusOnAppearing = (Args._Viewport->Flags & ImGuiViewportFlags_NoFocusOnAppearing);

	static FWindowStyle WindowStyle = FWindowStyle()
	                                  .SetActiveTitleBrush(FSlateNoResource())
	                                  .SetInactiveTitleBrush(FSlateNoResource())
	                                  .SetFlashTitleBrush(FSlateNoResource())
	                                  .SetOutlineBrush(FSlateNoResource())
	                                  .SetBorderBrush(FSlateNoResource())
	                                  .SetBackgroundBrush(FSlateNoResource())
	                                  .SetChildBackgroundBrush(FSlateNoResource());

	SWindow::Construct(
		SWindow::FArguments()
		.Type(bTooltipWindow ? EWindowType::ToolTip : EWindowType::Normal)
		.Style(&WindowStyle)
		.ScreenPosition(FVector2f(Args._Viewport->Pos))
		.ClientSize(FVector2f(Args._Viewport->Size))
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
			Args._Content.Widget
		]
	);
}

bool SImGuiWindow::OnIsActiveChanged(const FWindowActivateEvent& ActivateEvent)
{
	// Force mouse activation to be handled as normal so focus is processed
	if (ActivateEvent.GetActivationType() == FWindowActivateEvent::EA_ActivateByMouse)
	{
		const FWindowActivateEvent ModifiedActivateEvent(FWindowActivateEvent::EA_Activate, ActivateEvent.GetAffectedWindow());
		return SWindow::OnIsActiveChanged(ModifiedActivateEvent);
	}

	return SWindow::OnIsActiveChanged(ActivateEvent);
}

#endif // #ifndef IMGUI_DISABLE
