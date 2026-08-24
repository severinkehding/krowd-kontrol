// Confirms every shipped gameplay level (L_Level01, L_Level02, and future L_Level*
// maps as they ship) contains a playable spawn: a placed pawn carrying a
// UPlayerEnergyComponent, the same thing AEnemyBase::FindPlayerEnergyComponent()
// (EnemyBase.cpp) and ARootSurgeBoss::FindPlayerEnergyComponent() (RootSurgeBoss.cpp)
// search for. Without one, PIE silently falls back to the engine's invisible
// free-fly DefaultPawn (AKrowdKontrolGameMode intentionally sets no
// DefaultPawnClass - see its header comment / issue #132) and every system that
// depends on finding the player pawn becomes a no-op with no test failure to catch
// it - exactly what happened to L_Level01/L_Level02 (issue #185, PRD "Level
// Playability & Presentation" REQ-1).
//
// GameplayLevelMapPaths is the single point of extension for future L_Level03-05
// maps (MISSION.md's 5-level Alpha roster) - append their /Game/Maps path here,
// nothing else in this file needs to change.
//
// Uses FAutomationEditorCommonUtils::LoadMap (not CreateNewMap), mirroring
// KrowdKontrolLevel01Test.cpp/KrowdKontrolLevel02Test.cpp/
// KrowdKontrolFlatCamera3DPipelineSmokeTest.cpp's own level-asset regression tests -
// this is coverage for the shipped map content itself, not the pawn class in
// isolation (that's KrowdKontrolFlatCamera3DPipelineSmokeTest.cpp's job).
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the other
// KrowdKontrol.Unit.* tests.

#include "Misc/AutomationTest.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerStart.h"
#include "PlayerEnergyComponent.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Editor.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolGameplayLevelPlayableSpawnTest,
	"KrowdKontrol.Unit.GameplayLevelsHavePlayableSpawn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolGameplayLevelPlayableSpawnTest::RunTest(const FString& Parameters)
{
	// Every shipped gameplay map - append future L_Level03-05 entries here as they ship.
	static const TArray<FString> GameplayLevelMapPaths = {
		TEXT("/Game/Maps/L_Level01"),
		TEXT("/Game/Maps/L_Level02"),
		TEXT("/Game/Maps/L_Level03"),
	};

	for (const FString& MapPath : GameplayLevelMapPaths)
	{
		FAutomationEditorCommonUtils::LoadMap(MapPath);

		UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
		if (!TestNotNull(FString::Printf(TEXT("%s should load into a valid World"), *MapPath), World))
		{
			continue;
		}

		bool bHasPlayerStart = false;
		for (TActorIterator<APlayerStart> It(World); It; ++It)
		{
			bHasPlayerStart = true;
			break;
		}
		TestTrue(FString::Printf(TEXT("%s should contain a PlayerStart (PRD Level Playability REQ-1)"), *MapPath),
			bHasPlayerStart);

		UPlayerEnergyComponent* FoundEnergyComponent = nullptr;
		for (TActorIterator<APawn> It(World); It; ++It)
		{
			if (UPlayerEnergyComponent* Energy = It->FindComponentByClass<UPlayerEnergyComponent>())
			{
				FoundEnergyComponent = Energy;
				break;
			}
		}
		TestNotNull(
			FString::Printf(TEXT("%s should contain a placed pawn with a UPlayerEnergyComponent (PRD Level ")
				TEXT("Playability REQ-1) - otherwise AEnemyBase::FindPlayerEnergyComponent() finds nothing and ")
				TEXT("PIE falls back to the invisible free-fly DefaultPawn"), *MapPath),
			FoundEnergyComponent);
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
