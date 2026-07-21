// Copyright GMTCK CX.

using UnrealBuildTool;
using System.Collections.Generic;

public class GMTCK_MRTarget : TargetRules
{
	public GMTCK_MRTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V6;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("GMTCK_MR");
	}
}
