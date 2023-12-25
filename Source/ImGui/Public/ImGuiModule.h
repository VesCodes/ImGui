#pragma once

#include <Modules/ModuleManager.h>

class FImGuiContext;
class SWindow;
class UGameViewportClient;

class IMGUI_API FImGuiModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/// Returns this module's instance, loading it on demand if needed
	/// @note Beware of calling this during the shutdown phase as the module might have been unloaded already
	static FImGuiModule& Get();

	/// Finds or creates an ImGui context for an editor or game session
	/// @param PIEInstance Optional target Play-in-Editor instance, defaults to the current instance
	TSharedPtr<FImGuiContext> FindOrCreateSessionContext(const int32 PIEInstance = GPlayInEditorID);

	/// Creates an ImGui context for a Slate window
	static TSharedPtr<FImGuiContext> CreateWindowContext(const TSharedRef<SWindow>& Window);

	/// Creates an ImGui context for a game viewport
	static TSharedPtr<FImGuiContext> CreateViewportContext(UGameViewportClient* GameViewport);

private:
	void OnEndPIE(bool bIsSimulating);

	TMap<int32, TSharedPtr<FImGuiContext>> SessionContexts;
};
