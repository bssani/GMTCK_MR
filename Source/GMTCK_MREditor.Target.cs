// Copyright GMTCK CX.

using UnrealBuildTool;
using System.Collections.Generic;

public class GMTCK_MREditorTarget : TargetRules
{
	public GMTCK_MREditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V6;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("GMTCK_MR");
	}
}
