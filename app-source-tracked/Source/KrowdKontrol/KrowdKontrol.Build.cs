// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;

public class KrowdKontrol : ModuleRules
{
	public KrowdKontrol(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Module headers (e.g. PlaceholderCubeActor.h, RoomEnemyBudgetController.h) live
		// directly in the module root rather than under a Public/ folder, so files under
		// Private/Tests/ can't find them via bare #include without this - discovered as a
		// pre-existing gap while building the issue #82 test (the module root was never on
		// the include path once UBT stopped treating this as a from-cache incremental build).
		PrivateIncludePaths.Add(ModuleDirectory);

		// Paper2D for APaper2DPrototypePawn (issue #55) - the Paper2D half of PRD 14 REQ-1's
		// Paper2D-vs-flat-camera-3D pipeline comparison. Paper2D is EnabledByDefault in its
		// own .uplugin, so only this module dependency is needed - no .uproject Plugins
		// array entry.
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "Paper2D" });

		// UMG for UPostRunSummaryWidget (issue #74). No Widget Blueprint asset involved;
		// the class builds its own tree in C++ via WidgetTree->ConstructWidget<T>(), which
		// still requires the UMG module on the include/link path - same pattern as
		// UAbilityCooldownTrayWidget/UEnergyMeterWidget, whose own app-source-tracked/
		// Build.cs mirror was never updated for it until now.
		PrivateDependencyModuleNames.AddRange(new string[] { "UMG", "Slate", "SlateCore" });

		if (Target.bBuildEditor)
		{
			// Needed only for FAutomationEditorCommonUtils::CreateNewMap() in
			// KrowdKontrolRoomEnemyBudgetControllerTest.cpp (issue #82) - editor-only, never
			// linked into the packaged KrowdKontrolTarget (Game) build.
			PrivateDependencyModuleNames.Add("UnrealEd");
		}

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
