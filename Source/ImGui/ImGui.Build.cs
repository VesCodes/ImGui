using UnrealBuildTool;

public class ImGui : ModuleRules
{
	protected virtual bool WithImPlot => true;
	protected virtual bool WithNetImGui => true;
	protected virtual bool WithNativeRendering => Target.bCompileAgainstEngine;

	public ImGui(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"ImGuiLibrary",
		});

		PrivateDependencyModuleNames.AddRange(new[]
		{
			"ApplicationCore",
			"InputCore",
			"Slate",
			"SlateCore"
		});

		if (Target.bCompileAgainstEngine)
		{
			PrivateDependencyModuleNames.AddRange(new[]
			{
				"CoreUObject",
				"Engine"
			});
		}

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new[]
			{
				"MainFrame",
				"UnrealEd"
			});
		}

		PublicDefinitions.Add("IMGUI_USER_CONFIG=\"ImGuiConfig.h\"");

		if (WithImPlot)
		{
			PublicDependencyModuleNames.Add("ImPlotLibrary");
			PublicDefinitions.Add("IMPLOT_API=IMGUI_API");
			PublicDefinitions.Add("WITH_IMPLOT=1");
		}
		else
		{
			PublicDefinitions.Add("WITH_IMPLOT=0");
		}

		if (WithNetImGui)
		{
			PublicDependencyModuleNames.Add("NetImGuiLibrary");
			PublicDefinitions.Add("NETIMGUI_API=IMGUI_API");
			PublicDefinitions.Add("WITH_NETIMGUI=1");
		}
		else
		{
			PublicDefinitions.Add("WITH_NETIMGUI=0");
		}

		if (WithNativeRendering)
		{
			PrivateDependencyModuleNames.AddRange(new[]
			{
				"RHI",
				"RenderCore",
				"ImGuiShaders"
			});

			PublicDefinitions.Add("WITH_IMGUI_NATIVE_RENDERING=1");
		}
		else
		{
			PublicDefinitions.Add("WITH_IMGUI_NATIVE_RENDERING=0");
		}
	}
}
