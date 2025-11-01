// Copyright 2025 WiloMyst. All Rights Reserved.

using UnrealBuildTool;

public class OpenWorldARPG : ModuleRules
{
	public OpenWorldARPG(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"HeadMountedDisplay",
			"EnhancedInput",
            "UMG",
			"Slate",
			"SlateCore",
            "Niagara",
            "GameplayAbilities",
			"GameplayTags",
			"GameplayTasks",
			"AIModule"
        });
	}
}
