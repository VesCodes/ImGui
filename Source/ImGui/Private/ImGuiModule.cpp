#include "ImGuiModule.h"

#include <Widgets/SWindow.h>

#if WITH_ENGINE
#include <Engine/Engine.h>
#include <Engine/GameViewportClient.h>
#endif

#if WITH_EDITOR
#include <Editor.h>
#include <Interfaces/IMainFrameModule.h>
#endif

#include "ImGuiContext.h"
#include "SImGuiOverlay.h"

void FImGuiModule::StartupModule()
{
#if WITH_EDITOR
	FEditorDelegates::EndPIE.AddRaw(this, &FImGuiModule::OnEndPIE);
#endif
}

void FImGuiModule::ShutdownModule()
{
#if WITH_EDITOR
	FEditorDelegates::EndPIE.RemoveAll(this);
#endif

	SessionContexts.Reset();
}

FImGuiModule& FImGuiModule::Get()
{
	static FImGuiModule& Module = FModuleManager::LoadModuleChecked<FImGuiModule>(UE_MODULE_NAME);
	return Module;
}

TSharedPtr<FImGuiContext> FImGuiModule::FindOrCreateSessionContext(const int32 PIEInstance)
{
	TSharedPtr<FImGuiContext> Context = SessionContexts.FindRef(PIEInstance);
	if (!Context.IsValid())
	{
		FString Host;
		const bool bShouldConnect = FParse::Value(FCommandLine::Get(), TEXT("-ImGuiHost="), Host) && !Host.IsEmpty();

		uint16 Port = bShouldConnect ? 8888 : 8889;
		const bool bShouldListen = FParse::Value(FCommandLine::Get(), TEXT("-ImGuiPort="), Port) && Port != 0;

		if (!bShouldConnect)
		{
			// Bind consecutive listen ports for PIE sessions
			Port += PIEInstance + 1;
		}

#if WITH_EDITOR
		if (GIsEditor && PIEInstance == INDEX_NONE)
		{
			const IMainFrameModule* MainFrameModule = FModuleManager::GetModulePtr<IMainFrameModule>("MainFrame");
			const TSharedPtr<SWindow> MainFrameWindow = MainFrameModule ? MainFrameModule->GetParentWindow() : nullptr;
			if (MainFrameWindow.IsValid())
			{
				Context = CreateWindowContext(MainFrameWindow.ToSharedRef());
			}
		}
		else
#endif
		{
#if WITH_ENGINE
			const FWorldContext* WorldContext = GEngine->GetWorldContextFromPIEInstance(PIEInstance);
			UGameViewportClient* GameViewport = WorldContext ? WorldContext->GameViewport : GEngine->GameViewport;
			if (IsValid(GameViewport))
			{
				Context = CreateViewportContext(GameViewport);
			}
			else
			{
				Context = FImGuiContext::Create();
			}
#endif
		}

		if (Context.IsValid())
		{
			if (bShouldConnect && !Context->Connect(Host, Port) || bShouldListen && !Context->Listen(Port))
			{
				Context.Reset();
				Context = nullptr;
			}
			else
			{
				SessionContexts.Add(PIEInstance, Context);
			}
		}
	}

	return Context;
}

void FImGuiModule::OnEndPIE(bool bIsSimulating)
{
	SessionContexts.Reset();
}

TSharedPtr<FImGuiContext> FImGuiModule::CreateWindowContext(const TSharedRef<SWindow>& Window)
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

TSharedPtr<FImGuiContext> FImGuiModule::CreateViewportContext(UGameViewportClient* GameViewport)
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
