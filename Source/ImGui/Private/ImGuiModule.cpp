#include "ImGuiModule.h"

#include <Widgets/SWindow.h>

#if WITH_ENGINE
#include <Engine/Engine.h>
#include <Engine/GameViewportClient.h>
#endif

#if WITH_EDITOR
#include <Interfaces/IMainFrameModule.h>
#endif

#include "ImGuiContext.h"
#include "SImGuiOverlay.h"

FImGuiModule& FImGuiModule::Get()
{
	static FImGuiModule& Module = FModuleManager::LoadModuleChecked<FImGuiModule>(UE_MODULE_NAME);
	return Module;
}

TSharedPtr<FImGuiContext> FImGuiModule::FindOrCreateContextForSession(const int32 PieInstance)
{
	TSharedPtr<FImGuiContext> Context = SessionContexts.FindRef(PieInstance).Pin();
	if (!Context.IsValid())
	{
#if WITH_EDITOR
		if (GIsEditor && PieInstance == INDEX_NONE)
		{
			const IMainFrameModule* MainFrameModule = FModuleManager::GetModulePtr<IMainFrameModule>("MainFrame");
			const TSharedPtr<SWindow> MainFrameWindow = MainFrameModule ? MainFrameModule->GetParentWindow() : nullptr;
			if (MainFrameWindow.IsValid())
			{
				Context = CreateContextForWindow(MainFrameWindow.ToSharedRef());
				SessionContexts.Add(PieInstance, Context);
			}
		}
		else
#endif
		{
#if WITH_ENGINE
			const FWorldContext* WorldContext = GEngine->GetWorldContextFromPIEInstance(PieInstance);
			UGameViewportClient* GameViewport = WorldContext ? WorldContext->GameViewport : GEngine->GameViewport;
			if (IsValid(GameViewport))
			{
				Context = CreateContextForViewport(GameViewport);
				SessionContexts.Add(PieInstance, Context);
			}
#endif
		}
	}

	return Context;
}

TSharedPtr<FImGuiContext> FImGuiModule::CreateContextForWindow(const TSharedRef<SWindow>& Window)
{
	const TSharedRef<SImGuiOverlay> Overlay = SNew(SImGuiOverlay);
	Window->AddOverlaySlot(TNumericLimits<int32>::Max())[Overlay];

	TSharedPtr<FImGuiContext> Context = Overlay->GetContext();

	ImGui::FScopedContext ScopedContext(Context);

	FImGuiViewportData* ViewportData = FImGuiViewportData::GetOrCreate(ImGui::GetMainViewport());
	if (ViewportData)
	{
		ViewportData->Window = Window;
		ViewportData->Overlay = Overlay;
	}

	return Context;
}

TSharedPtr<FImGuiContext> FImGuiModule::CreateContextForViewport(UGameViewportClient* GameViewport)
{
#if WITH_ENGINE
	if (!IsValid(GameViewport))
	{
		return nullptr;
	}

	const TSharedRef<SImGuiOverlay> Overlay = SNew(SImGuiOverlay);
	GameViewport->AddViewportWidgetContent(Overlay, TNumericLimits<int32>::Max());

	TSharedPtr<FImGuiContext> Context = Overlay->GetContext();

	ImGui::FScopedContext ScopedContext(Context);

	FImGuiViewportData* ViewportData = FImGuiViewportData::GetOrCreate(ImGui::GetMainViewport());
	if (ViewportData)
	{
		ViewportData->Window = GameViewport->GetWindow();
		ViewportData->Overlay = Overlay;
	}

	return Context;
#else
	return nullptr;
#endif
}

IMPLEMENT_MODULE(FImGuiModule, ImGui);
