// Copyright 2025 WiloMyst. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class OpenWorldARPGTarget : TargetRules
{
	public OpenWorldARPGTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V5;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_5;
		ExtraModuleNames.Add("OpenWorldARPG");
	}
}
