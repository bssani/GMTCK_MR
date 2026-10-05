using UnrealBuildTool;

public class CXMREditor : ModuleRules
{
	public CXMREditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine", "CXMR", "UnrealEd", "ToolMenus", "Slate", "SlateCore"
		});
	}
}
