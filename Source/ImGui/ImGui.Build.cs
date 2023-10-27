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
			"ImPlotLibrary"
		});

		PrivateDependencyModuleNames.AddRange(new[]
		{
			"ApplicationCore",
			"CoreUObject",
			"Engine",
			"InputCore",
			"Slate",
			"SlateCore"
		});

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.Add("MainFrame");
		}

		PublicDefinitions.Add("IMGUI_USER_CONFIG=\"ImGuiConfig.h\"");
		PublicDefinitions.Add("IMPLOT_API=IMGUI_API");
	}
}