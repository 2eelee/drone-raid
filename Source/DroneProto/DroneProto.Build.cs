// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class DroneProto : ModuleRules
{
	public DroneProto(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate"
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			"SlateCore",
			// FUniqueNetIdWrapper::ToString (DroneReport PlayerKey) 링크에 필요
			"CoreOnline"
		});

		PublicIncludePaths.AddRange(new string[] {
			"DroneProto",
			"DroneProto/Lobby",
			"DroneProto/Raid",
			"DroneProto/Variant_Platforming",
			"DroneProto/Variant_Platforming/Animation",
			"DroneProto/Variant_Combat",
			"DroneProto/Variant_Combat/AI",
			"DroneProto/Variant_Combat/Animation",
			"DroneProto/Variant_Combat/Gameplay",
			"DroneProto/Variant_Combat/Interfaces",
			"DroneProto/Variant_Combat/UI",
			"DroneProto/Variant_SideScrolling",
			"DroneProto/Variant_SideScrolling/AI",
			"DroneProto/Variant_SideScrolling/Gameplay",
			"DroneProto/Variant_SideScrolling/Interfaces",
			"DroneProto/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
