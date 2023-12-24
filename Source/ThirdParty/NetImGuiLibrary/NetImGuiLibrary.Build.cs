using UnrealBuildTool;

public class NetImGuiLibrary : ModuleRules
{
	public NetImGuiLibrary(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;
		PublicSystemIncludePaths.Add(ModuleDirectory);

		PublicDependencyModuleNames.Add("Sockets");
	}
}