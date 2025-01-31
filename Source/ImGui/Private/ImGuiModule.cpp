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

#if WITH_ENGINE
	UGameViewportClient::OnViewportCreated().AddRaw(this, &FImGuiModule::OnViewportCreated);
#endif
}

void FImGuiModule::ShutdownModule()
{
#if WITH_EDITOR
	FEditorDelegates::EndPIE.RemoveAll(this);
#endif

#if WITH_ENGINE
	UGameViewportClient::OnViewportCreated().RemoveAll(this);
#endif

	SessionContexts.Reset();
}

FImGuiModule& FImGuiModule::Get()
{
	static FImGuiModule& Module = FModuleManager::LoadModuleChecked<FImGuiModule>(UE_MODULE_NAME);
	return Module;
}

TSharedPtr<FImGuiContext> FImGuiModule::FindOrCreateSessionContext(const int32 PieSessionId)
{
	TSharedPtr<FImGuiContext> Context = SessionContexts.FindRef(PieSessionId);
	if (!Context.IsValid())
	{
		FString Host;
		const bool bShouldConnect = FParse::Value(FCommandLine::Get(), TEXT("-ImGuiHost="), Host) && !Host.IsEmpty();

		uint16 Port = bShouldConnect ? 8888 : 8889;
		const bool bShouldListen = FParse::Value(FCommandLine::Get(), TEXT("-ImGuiPort="), Port) && Port != 0;

		if (!bShouldConnect)
		{
			// Bind consecutive listen ports for PIE sessions
			Port += PieSessionId + 1;
		}

#if WITH_ENGINE
		const FWorldContext* WorldContext = GEngine->GetWorldContextFromPIEInstance(PIEInstance);
		UGameViewportClient* GameViewport = WorldContext ? WorldContext->GameViewport : GEngine->GameViewport;
		if (IsValid(GameViewport))
		{
			Context = CreateViewportContext(GameViewport);
		}
#if WITH_EDITOR
		else if (GIsEditor)
		{
			const IMainFrameModule* MainFrameModule = FModuleManager::GetModulePtr<IMainFrameModule>("MainFrame");
			const TSharedPtr<SWindow> MainFrameWindow = MainFrameModule ? MainFrameModule->GetParentWindow() : nullptr;
			if (MainFrameWindow.IsValid())
			{
				Context = CreateWindowContext(MainFrameWindow.ToSharedRef());
			}
		}
#endif
#endif
		
		if (!Context.IsValid())
		{
			Context = FImGuiContext::Create();
		}

		if (Context.IsValid())
		{
			if ((bShouldConnect && !Context->Connect(Host, Port)) || (bShouldListen && !Context->Listen(Port)))
			{
				Context.Reset();
				Context = nullptr;
			}
			else
			{
				SessionContexts.Add(PieSessionId, Context);
			}
		}
	}

	return Context;
}

void FImGuiModule::OnEndPIE(bool bIsSimulating)
{
	for (auto ContextIt = SessionContexts.CreateIterator(); ContextIt; ++ContextIt)
	{
		if (ContextIt->Key != INDEX_NONE)
		{
			ContextIt.RemoveCurrent();
		}
	}
}

void FImGuiModule::OnViewportCreated() const
{
#if WITH_ENGINE
	UGameViewportClient* GameViewport = GEngine->GameViewport;
	if (!IsValid(GameViewport))
	{
		return;
	}

#if UE_VERSION_OLDER_THAN(5, 5, 0)
	const int32 PieSessionId = GPlayInEditorID;
#else
	const int32 PieSessionId = UE::GetPlayInEditorID();
#endif

	const TSharedPtr<FImGuiContext> Context = SessionContexts.FindRef(PieSessionId);
	if (!Context.IsValid())
	{
		return;
	}

	ImGui::FScopedContext ScopedContext(Context);

	FImGuiViewportData* ViewportData = FImGuiViewportData::GetOrCreate(ImGui::GetMainViewport());
	if (ViewportData && !ViewportData->Overlay.IsValid())
	{
		const TSharedRef<SImGuiOverlay> Overlay = SNew(SImGuiOverlay).Context(Context);

		ViewportData->Window = GameViewport->GetWindow();
		ViewportData->Overlay = Overlay;

		GameViewport->AddViewportWidgetContent(Overlay, TNumericLimits<int32>::Max());
	}
#endif
}

TSharedPtr<FImGuiContext> FImGuiModule::CreateWindowContext(const TSharedRef<SWindow>& Window)
{
	const TSharedRef<FImGuiContext> Context = FImGuiContext::Create();

	ImGui::FScopedContext ScopedContext(Context);

	FImGuiViewportData* ViewportData = FImGuiViewportData::GetOrCreate(ImGui::GetMainViewport());
	if (ViewportData)
	{
		const TSharedRef<SImGuiOverlay> Overlay = SNew(SImGuiOverlay).Context(Context);

		ViewportData->Window = Window;
		ViewportData->Overlay = Overlay;

		Window->AddOverlaySlot(TNumericLimits<int32>::Max())[Overlay];
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

	const TSharedRef<FImGuiContext> Context = FImGuiContext::Create();

	ImGui::FScopedContext ScopedContext(Context);

	FImGuiViewportData* ViewportData = FImGuiViewportData::GetOrCreate(ImGui::GetMainViewport());
	if (ViewportData)
	{
		const TSharedRef<SImGuiOverlay> Overlay = SNew(SImGuiOverlay).Context(Context);

		ViewportData->Window = GameViewport->GetWindow();
		ViewportData->Overlay = Overlay;

		GameViewport->AddViewportWidgetContent(Overlay, TNumericLimits<int32>::Max());
	}

	return Context;
#else
	return nullptr;
#endif
}

IMPLEMENT_MODULE(FImGuiModule, ImGui);
