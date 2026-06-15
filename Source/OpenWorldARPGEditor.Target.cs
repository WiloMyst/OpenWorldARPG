// Copyright 2025 WiloMyst. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class OpenWorldARPGEditorTarget : TargetRules
{
	public OpenWorldARPGEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V5;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_5;
		ExtraModuleNames.Add("OpenWorldARPG");
	}
}
