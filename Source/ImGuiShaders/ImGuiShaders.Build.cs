using UnrealBuildTool;

public class ImGuiShaders : ModuleRules
{
	public ImGuiShaders(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"RenderCore"
		});

		PrivateDependencyModuleNames.AddRange(new[]
		{
			"Projects"
		});
	}
}
