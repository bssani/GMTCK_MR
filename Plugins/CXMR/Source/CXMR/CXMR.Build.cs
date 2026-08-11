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
			// Public: CXMRControlPanelWidget.h derives from UUserWidget and CXMRPawn.h exposes
			// WidgetComponent / WidgetInteractionComponent, so any module including those headers needs UMG.
			"UMG",
			// Varjo plugin — CXMR is the single façade over its API (markers, depth, MR, view offset).
			"VarjoOpenXR",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Slate",
			"SlateCore",
			// Marker calibration is written to Saved/CXMR as JSON: a cooked build cannot write back to
			// its own data assets, and text is what makes a remote session diagnosable over the phone.
			"Json",
		});
	}
}
