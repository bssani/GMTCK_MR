// Copyright GMTCK CX. CXMR — Customer Experience Mixed Reality template.

using UnrealBuildTool;

public class CXMR : ModuleRules
{
	public CXMR(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"HeadMountedDisplay",
			// Varjo plugin — CXMR is the single façade over its API (markers, depth, MR, view offset).
			"VarjoOpenXR",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"UMG",
			"Slate",
			"SlateCore",
		});
	}
}
