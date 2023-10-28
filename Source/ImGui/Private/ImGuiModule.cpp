#include "ImGuiModule.h"

#include <Engine/Engine.h>
#include <Engine/GameViewportClient.h>
#include <HAL/LowLevelMemTracker.h>
#include <HAL/UnrealMemory.h>
#include <Widgets/SWindow.h>

#if WITH_EDITOR
#include <Interfaces/IMainFrameModule.h>
#endif

#include "ImGuiContext.h"
#include "SImGuiOverlay.h"

static void* ImGui_MemAlloc(size_t Size, void* UserData)
{
	LLM_SCOPE_BYNAME(TEXT("ImGui"));
	return FMemory::Malloc(Size);
}

static void ImGui_MemFree(void* Ptr, void* UserData)
{
	FMemory::Free(Ptr);
}

void FImGuiModule::StartupModule()
{
	ImGui::SetAllocatorFunctions(ImGui_MemAlloc, ImGui_MemFree);
}

FImGuiModule& FImGuiModule::Get()
{
	static FImGuiModule& Module = FModuleManager::LoadModuleChecked<FImGuiModule>(UE_MODULE_NAME);
	return Module;
}

TSharedPtr<FImGuiContext> FImGuiModule::FindOrCreateContext(const int32 PieInstance)
{
	TSharedPtr<FImGuiContext> Context = Contexts.FindRef(PieInstance).Pin();
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
				Contexts.Add(PieInstance, Context);
			}
		}
		else
#endif
		{
			const FWorldContext* WorldContext = GEngine->GetWorldContextFromPIEInstance(PieInstance);
			UGameViewportClient* GameViewport = WorldContext ? WorldContext->GameViewport : GEngine->GameViewport;
			if (IsValid(GameViewport))
			{
				Context = CreateContextForViewport(GameViewport);
				Contexts.Add(PieInstance, Context);
			}
		}
	}

	return Context;
}

TSharedPtr<FImGuiContext> FImGuiModule::CreateContextForWindow(const TSharedRef<SWindow>& Window)
{
	const TSharedRef<SImGuiOverlay> Overlay = SNew(SImGuiOverlay);
	Window->AddOverlaySlot(TNumericLimits<int32>::Max())[Overlay];

	TSharedPtr<FImGuiContext> Context = Overlay->GetContext();

	ImGui::FScopedContextSwitcher ContextSwitcher(Context);

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
	if (!IsValid(GameViewport))
	{
		return nullptr;
	}

	const TSharedRef<SImGuiOverlay> Overlay = SNew(SImGuiOverlay);
	GameViewport->AddViewportWidgetContent(Overlay, TNumericLimits<int32>::Max());

	TSharedPtr<FImGuiContext> Context = Overlay->GetContext();

	ImGui::FScopedContextSwitcher ContextSwitcher(Context);

	FImGuiViewportData* ViewportData = FImGuiViewportData::GetOrCreate(ImGui::GetMainViewport());
	if (ViewportData)
	{
		ViewportData->Window = GameViewport->GetWindow();
		ViewportData->Overlay = Overlay;
	}

	return Context;
}

IMPLEMENT_MODULE(FImGuiModule, ImGui);
