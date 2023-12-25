using UnrealBuildTool;

public class ImGui : ModuleRules
{
	public ImGui(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"ImGuiLibrary",
			"ImPlotLibrary",
			"NetImGuiLibrary"
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

		PublicDefinitions.AddRange(new[]
		{
			"IMGUI_USER_CONFIG=\"ImGuiConfig.h\"",
			"IMPLOT_API=IMGUI_API",
			"NETIMGUI_API=IMGUI_API"
		});
	}
}