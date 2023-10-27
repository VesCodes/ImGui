#include "ImGuiModule.h"

#include <Engine/Engine.h>
#include <Engine/GameViewportClient.h>
#include <HAL/LowLevelMemTracker.h>
#include <HAL/UnrealMemory.h>
#include <Widgets/SWindow.h>

#if WITH_EDITOR
#include <Interfaces/IMainFrameModule.h>
#endif

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

#if WITH_EDITOR
	IMainFrameModule& MainFrameModule = FModuleManager::LoadModuleChecked<IMainFrameModule>("MainFrame");
	MainFrameModule.OnMainFrameCreationFinished().AddRaw(this, &FImGuiModule::OnMainFrameCreated);
#endif

	FSlateApplication::Get().OnPostTick().AddLambda([](float DeltaTime)
	{
		ImGui::FScopedContextSwitcher ContextSwitcher(INDEX_NONE);
		if (ContextSwitcher)
		{
			ImGui::ShowDemoWindow();
		}
	});

	UGameViewportClient::OnViewportCreated().AddRaw(this, &FImGuiModule::OnViewportCreated);
}

void FImGuiModule::ShutdownModule()
{
#if WITH_EDITOR
	IMainFrameModule* MainFrameModule = FModuleManager::GetModulePtr<IMainFrameModule>("MainFrame");
	if (MainFrameModule)
	{
		MainFrameModule->OnMainFrameCreationFinished().RemoveAll(this);
	}
#endif

	UGameViewportClient::OnViewportCreated().RemoveAll(this);
}

FImGuiModule& FImGuiModule::Get()
{
	static FImGuiModule& Module = FModuleManager::LoadModuleChecked<FImGuiModule>(UE_MODULE_NAME);
	return Module;
}

void FImGuiModule::CreateContextForViewport(UGameViewportClient* Viewport)
{
	if (IsValid(Viewport))
	{
		const TSharedRef<SImGuiOverlay> Overlay = SNew(SImGuiOverlay);
		Viewport->AddViewportWidgetContent(Overlay, TNumericLimits<int32>::Max());

		Contexts.Add(GPlayInEditorID, Overlay->GetContext());

		ImGui::FScopedContextSwitcher ContextSwitcher(Overlay->GetContext());

		FImGuiViewportData* ViewportData = FImGuiViewportData::GetOrCreate(ImGui::GetMainViewport());
		if (ViewportData)
		{
			ViewportData->Window = Viewport->GetWindow();
			ViewportData->Overlay = Overlay;
		}
	}
}

void FImGuiModule::CreateContextForWindow(const TSharedRef<SWindow>& Window)
{
	const TSharedRef<SImGuiOverlay> Overlay = SNew(SImGuiOverlay);
	Window->AddOverlaySlot(TNumericLimits<int32>::Max())[Overlay];

	Contexts.Add(INDEX_NONE, Overlay->GetContext());

	ImGui::FScopedContextSwitcher ContextSwitcher(Overlay->GetContext());

	FImGuiViewportData* ViewportData = FImGuiViewportData::GetOrCreate(ImGui::GetMainViewport());
	if (ViewportData)
	{
		ViewportData->Window = Window;
		ViewportData->Overlay = Overlay;
	}
}

ImGuiContext* FImGuiModule::GetContext(const int32 ContextIdx) const
{
	return Contexts.FindRef(ContextIdx);
}

void FImGuiModule::OnMainFrameCreated(const TSharedPtr<SWindow> Window, bool bStartupDialog)
{
	if (Window.IsValid())
	{
		CreateContextForWindow(Window.ToSharedRef());
	}
}

void FImGuiModule::OnViewportCreated()
{
	if (GEngine)
	{
		CreateContextForViewport(GEngine->GameViewport);
	}
}

IMPLEMENT_MODULE(FImGuiModule, ImGui);
