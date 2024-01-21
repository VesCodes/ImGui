# ImGui for Unreal Engine

Supercharge your Unreal Engine development with [Dear ImGui](https://github.com/ocornut/imgui). This plugin is designed
to be as frictionless and easy to use as possible while seamlessly integrating all of ImGui's features into UE's
ecosystem.

## Features

* **Multi-viewports**: Pull ImGui windows out of the application's frame ([read more](https://github.com/ocornut/imgui/wiki/Multi-Viewports))
* **Docking**: Combine and tear apart ImGui windows to create custom layouts ([read more](https://github.com/ocornut/imgui/wiki/Docking))
* **Editor support**: Draw ImGui windows in Unreal Editor outside of game sessions
* **Play-in-Editor support**: Each PIE session has its own ImGui context
* **Program support**: Use ImGui in programs and standalone Slate applications
* **Remote drawing**: Connect to headless (e.g. dedicated server) or remote sessions

## Usage

1. Clone this repository into your project's `Plugins` directory
2. Add `ImGui` as a public or private dependency to your module's `Build.cs` file:

	```c#
	PublicDependencyModuleNames.Add("ImGui");
	```

3. Include `imgui.h` and prior to using any ImGui functions create a local scoped context:

	```c++
	#pragma once

	#include <GameFramework/Actor.h>
	#include <imgui.h>

	#include "ImGuiActor.generated.h"

	UCLASS()
	class AImGuiActor : public AActor
	{
		GENERATED_BODY()

	public:
		AImGuiActor()
		{
			PrimaryActorTick.bCanEverTick = true;
		}

		virtual void Tick(float DeltaTime) override
		{
			Super::Tick(DeltaTime);

			const ImGui::FScopedContext ScopedContext;
			if (ScopedContext)
			{
				// Your ImGui code goes here!
				ImGui::ShowDemoWindow();
			}
		}
	};
	```

This "scoped context" mechanism will push the appropriate ImGui context and pop it once it's gone out of scope. It's
advised to check the `ScopedContext` like the example above to ensure that it's safe to draw.

## Remote drawing

A prebuilt binary of the [NetImGui Server](https://github.com/sammyfreg/netImgui) application is included in
`Source/ThirdParty/NetImGuiServer`. This application allows connecting to ImGui sessions either locally or on a remote
device (e.g. dedicated server, console, mobile device).

There are two command-line arguments allowing you to automatically connect to a NetImGui Server:
- Specifying `-ImGuiHost=Host` will attempt to automatically connect to a NetImGui Server on the specified host using
the default port (8888) though this can be overridden using `-ImGuiPort=Port`; note that the host must already be
running the NetImGui Server application otherwise this will be unsuccessful.
- Specifying just `-ImGuiPort=Port` will attempt to automatically listen for a NetImGui Server connection. This is often
the more convenient approach as you can then connect using NetImGui Server at any point in your session.

Alternatively you can drive this in code using `FImGuiContext::Connect` and `FImGuiContext::Listen` respectively:

```c++
const ImGui::FScopedContext ScopedContext;
if (ScopedContext.IsValid())
{
	ScopedContext->Listen(8888);
}
```

## Usage in programs

You can utilise this plugin in Unreal programs and Slate applications, though for releases prior to UE 5.4 the latter
will require integrating [this PR](https://github.com/EpicGames/UnrealEngine/pull/11088).

You can either initialise an ImGui context manually using `FImGuiContext::Create` and use the remote drawing
functionality outlined in the section above, or attach ImGui to a Slate application using
`FImGuiModule::CreateWindowContext`.

Either way you will likely need to manually call `FImGuiContext::BeginFrame` and `FImGuiContext::EndFrame` at
appropriate points in your application's main loop, as they usually rely on delegates fired from `FEngineLoop::Tick`
which is often not executed in standalone programs for obvious reasons.