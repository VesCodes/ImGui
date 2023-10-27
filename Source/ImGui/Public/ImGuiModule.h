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

	/// Returns this module's instance, loading it on demand if needed
	/// @note Beware of calling this during the shutdown phase as the module might have been unloaded already
	static FImGuiModule& Get();

	/// Finds or creates an ImGui context for an editor or game session
	/// @param PieInstance Optional target Play-in-Editor instance, defaults to the current instance
	ImGuiContext* FindOrCreateContext(const int32 PieInstance = GPlayInEditorID);

	/// Creates an ImGui context for a Slate window
	static ImGuiContext* CreateContextForWindow(const TSharedRef<SWindow>& Window);

private:
	TMap<int32, ImGuiContext*> Contexts;
};
