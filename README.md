# ImGui for Unreal Engine

Supercharge your Unreal Engine development with [Dear ImGui](https://github.com/ocornut/imgui). This plugin is designed to be as frictionless and easy to use as possible while seamlessly integrating all of ImGui's features into UE's ecosystem.

## Features

* **Multi-viewports support**: Pull ImGui windows out of the application's frame ([read more](https://github.com/ocornut/imgui/wiki/Multi-Viewports))
* **Docking support**: Combine and tear apart ImGui windows to create advanced layouts ([read more](https://github.com/ocornut/imgui/wiki/Docking))
* **Editor support**: Draw ImGui windows in Unreal Editor outside of game sessions
* **Play-in-Editor (PIE) support**: Each PIE game session has it's own ImGui context

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
			ImGui::FScopedContext ScopedContext;
			if (ScopedContext)
			{
				// Your ImGui code goes here!
				ImGui::ShowDemoWindow();
			}
		}
	};
	```

This "scoped context" mechanism will push the appropriate ImGui context and pop it once it's gone out of scope. It's advised to check the `ScopedContext` like the example above to ensure that it's safe to draw.