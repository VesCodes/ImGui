#pragma once

#include <Modules/ModuleManager.h>

struct ImGuiContext;
class SWindow;
class SWidget;
class UGameViewportClient;

class IMGUI_API FImGuiModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/// Returns this module's instance, loading it on demand if needed
	/// @note Beware of calling this during the shutdown phase as the module might have been unloaded already
	static FImGuiModule& Get();

	/// Finds an existing ImGui context for a given session
	ImGuiContext* GetContext(const int32 ContextIdx = GPlayInEditorID) const;

private:
	void CreateContextForViewport(UGameViewportClient* Viewport);
	void CreateContextForWindow(const TSharedRef<SWindow>& Window);

	void OnMainFrameCreated(const TSharedPtr<SWindow> Window, bool bStartupDialog);
	void OnViewportCreated();

	TMap<int32, ImGuiContext*> Contexts;
};
