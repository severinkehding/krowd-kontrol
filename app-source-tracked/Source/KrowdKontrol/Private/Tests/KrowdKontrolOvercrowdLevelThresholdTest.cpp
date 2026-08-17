// Confirms UOvercrowdDetectionComponent::LevelThresholds/NotifyLevelReached (PRD 08
// REQ-1, issue #23) make the 3 Overcrowd trigger thresholds (OvercrowdCrowdThreshold/
// OvercrowdRadiusUnits/OvercrowdUncontrolledDurationSeconds) per-level rather than a
// single global constant: the same enemy-convergence scenario must trigger Overcrowd
// under one level's configured threshold and not under a different (looser) level's,
// NotifyLevelReached must no-op (with a warning) on a non-empty LevelThresholds with
// no matching entry, no-op silently on an empty LevelThresholds, and reset
// UncontrolledSeconds so an in-progress accumulation under the old thresholds never
// carries over to the new ones.
//
// AdvancePanicOverloadState() is called directly (never via a real TickComponent()
// loop), same rationale as KrowdKontrolOvercrowdDetectionComponentTest.cpp. Each
// scenario that spawns enemies and drives detection uses its own
// FAutomationEditorCommonUtils::CreateNewMap() World, for the same reason that test
// documents: CountHotUncontrolledEnemiesNearby() iterates every AEnemyBase in the
// component's GetWorld(), so scenarios must not share a World. The no-op scenarios
// (3/4) only assert field values, not detection behavior, so they use a bare
// NewObject() with no World, mirroring KrowdKontrolAbilityUnlockSequenceTest.cpp.
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the other
// KrowdKontrol.Unit.* tests.

#include "Misc/AutomationTest.h"
#include "OvercrowdDetectionComponent.h"
#include "EnemyBaseTestActor.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolOvercrowdLevelThresholdTest,
	"KrowdKontrol.Unit.OvercrowdLevelThreshold",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolOvercrowdLevelThresholdTest::RunTest(const FString& Parameters)
{
	// --- Scenario 1: a tight level's configured threshold triggers Overcrowd for a
	// 3-enemy convergence. ---
	UWorld* TightWorld = FAutomationEditorCommonUtils::CreateNewMap();
	if (!TestNotNull(TEXT("CreateNewMap should return a valid World for the tight-level scenario"), TightWorld))
	{
		return false;
	}

	APawn* TightPawn = TightWorld->SpawnActor<APawn>();
	if (!TestNotNull(TEXT("APawn should spawn into the tight-level test World"), TightPawn))
	{
		return false;
	}

	UOvercrowdDetectionComponent* TightComponent = NewObject<UOvercrowdDetectionComponent>(TightPawn);
	TightComponent->RegisterComponent();

	FOvercrowdLevelThreshold TightEntry;
	TightEntry.LevelIndex = 1;
	TightEntry.CrowdThreshold = 3;
	TightEntry.RadiusUnits = 800.0f;
	TightEntry.UncontrolledDurationSeconds = 1.0f;
	TightComponent->LevelThresholds = { TightEntry };

	TightComponent->NotifyLevelReached(1);
	TestEqual(TEXT("NotifyLevelReached should apply the matching entry's CrowdThreshold"),
		TightComponent->OvercrowdCrowdThreshold, TightEntry.CrowdThreshold);
	TestEqual(TEXT("NotifyLevelReached should apply the matching entry's RadiusUnits"),
		TightComponent->OvercrowdRadiusUnits, TightEntry.RadiusUnits);
	TestEqual(TEXT("NotifyLevelReached should apply the matching entry's UncontrolledDurationSeconds"),
		TightComponent->OvercrowdUncontrolledDurationSeconds, TightEntry.UncontrolledDurationSeconds);

	for (int32 Index = 0; Index < 3; ++Index)
	{
		AEnemyBaseTestActor* Enemy = TightWorld->SpawnActor<AEnemyBaseTestActor>();
		if (!TestNotNull(TEXT("Tight-level AEnemyBaseTestActor should spawn"), Enemy))
		{
			return false;
		}
		Enemy->TickCheckDetection(FVector::ZeroVector); // Idle -> Alert, at the player's location
	}
	TightComponent->AdvancePanicOverloadState(1.5f);
	TestEqual(TEXT("A 3-enemy convergence should trigger Overcrowd under the tight level's threshold"),
		static_cast<uint8>(TightComponent->GetPanicOverloadState()), static_cast<uint8>(EPanicOverloadState::Active));

	// --- Scenario 2: the same 3-enemy convergence does not trigger Overcrowd under a
	// looser level's configured threshold. Fresh World/component - Scenario 1's enemies
	// must not leak into this count. ---
	UWorld* LooseWorld = FAutomationEditorCommonUtils::CreateNewMap();
	if (!TestNotNull(TEXT("CreateNewMap should return a valid World for the loose-level scenario"), LooseWorld))
	{
		return false;
	}

	APawn* LoosePawn = LooseWorld->SpawnActor<APawn>();
	if (!TestNotNull(TEXT("APawn should spawn into the loose-level test World"), LoosePawn))
	{
		return false;
	}

	UOvercrowdDetectionComponent* LooseComponent = NewObject<UOvercrowdDetectionComponent>(LoosePawn);
	LooseComponent->RegisterComponent();

	FOvercrowdLevelThreshold LooseEntry;
	LooseEntry.LevelIndex = 4;
	LooseEntry.CrowdThreshold = 8;
	LooseEntry.RadiusUnits = 800.0f;
	LooseEntry.UncontrolledDurationSeconds = 1.0f;
	LooseComponent->LevelThresholds = { LooseEntry };

	LooseComponent->NotifyLevelReached(4);

	for (int32 Index = 0; Index < 3; ++Index)
	{
		AEnemyBaseTestActor* Enemy = LooseWorld->SpawnActor<AEnemyBaseTestActor>();
		if (!TestNotNull(TEXT("Loose-level AEnemyBaseTestActor should spawn"), Enemy))
		{
			return false;
		}
		Enemy->TickCheckDetection(FVector::ZeroVector); // Idle -> Alert, at the player's location
	}
	LooseComponent->AdvancePanicOverloadState(1.5f);
	TestEqual(TEXT("The same 3-enemy convergence should NOT trigger Overcrowd under the looser level's threshold"),
		static_cast<uint8>(LooseComponent->GetPanicOverloadState()), static_cast<uint8>(EPanicOverloadState::Inactive));

	// --- Scenario 3: NotifyLevelReached with no matching entry in a non-empty
	// LevelThresholds logs a warning and leaves the 3 live fields unchanged. No World
	// needed - only field values are asserted. ---
	UOvercrowdDetectionComponent* NoMatchComponent = NewObject<UOvercrowdDetectionComponent>();
	if (!TestNotNull(TEXT("A no-match UOvercrowdDetectionComponent should construct"), NoMatchComponent))
	{
		return false;
	}
	const int32 NoMatchOriginalCrowdThreshold = NoMatchComponent->OvercrowdCrowdThreshold;
	const float NoMatchOriginalRadiusUnits = NoMatchComponent->OvercrowdRadiusUnits;
	const float NoMatchOriginalDuration = NoMatchComponent->OvercrowdUncontrolledDurationSeconds;

	FOvercrowdLevelThreshold NoMatchEntry;
	NoMatchEntry.LevelIndex = 1;
	NoMatchComponent->LevelThresholds = { NoMatchEntry };

	NoMatchComponent->NotifyLevelReached(99);
	TestEqual(TEXT("A non-matching NotifyLevelReached should leave OvercrowdCrowdThreshold unchanged"),
		NoMatchComponent->OvercrowdCrowdThreshold, NoMatchOriginalCrowdThreshold);
	TestEqual(TEXT("A non-matching NotifyLevelReached should leave OvercrowdRadiusUnits unchanged"),
		NoMatchComponent->OvercrowdRadiusUnits, NoMatchOriginalRadiusUnits);
	TestEqual(TEXT("A non-matching NotifyLevelReached should leave OvercrowdUncontrolledDurationSeconds unchanged"),
		NoMatchComponent->OvercrowdUncontrolledDurationSeconds, NoMatchOriginalDuration);

	// --- Scenario 4: NotifyLevelReached with an empty LevelThresholds is a silent
	// no-op. No World needed. ---
	UOvercrowdDetectionComponent* EmptyComponent = NewObject<UOvercrowdDetectionComponent>();
	if (!TestNotNull(TEXT("An empty-LevelThresholds UOvercrowdDetectionComponent should construct"), EmptyComponent))
	{
		return false;
	}
	const int32 EmptyOriginalCrowdThreshold = EmptyComponent->OvercrowdCrowdThreshold;
	const float EmptyOriginalRadiusUnits = EmptyComponent->OvercrowdRadiusUnits;
	const float EmptyOriginalDuration = EmptyComponent->OvercrowdUncontrolledDurationSeconds;

	EmptyComponent->NotifyLevelReached(1);
	TestEqual(TEXT("An empty-LevelThresholds NotifyLevelReached should leave OvercrowdCrowdThreshold unchanged"),
		EmptyComponent->OvercrowdCrowdThreshold, EmptyOriginalCrowdThreshold);
	TestEqual(TEXT("An empty-LevelThresholds NotifyLevelReached should leave OvercrowdRadiusUnits unchanged"),
		EmptyComponent->OvercrowdRadiusUnits, EmptyOriginalRadiusUnits);
	TestEqual(TEXT("An empty-LevelThresholds NotifyLevelReached should leave OvercrowdUncontrolledDurationSeconds unchanged"),
		EmptyComponent->OvercrowdUncontrolledDurationSeconds, EmptyOriginalDuration);

	// --- Scenario 5: NotifyLevelReached resets UncontrolledSeconds, so a partial
	// accumulation under the old thresholds never carries over to the new ones. Fresh
	// World/component. ---
	UWorld* ResetWorld = FAutomationEditorCommonUtils::CreateNewMap();
	if (!TestNotNull(TEXT("CreateNewMap should return a valid World for the reset scenario"), ResetWorld))
	{
		return false;
	}

	APawn* ResetPawn = ResetWorld->SpawnActor<APawn>();
	if (!TestNotNull(TEXT("APawn should spawn into the reset test World"), ResetPawn))
	{
		return false;
	}

	UOvercrowdDetectionComponent* ResetComponent = NewObject<UOvercrowdDetectionComponent>(ResetPawn);
	ResetComponent->RegisterComponent();

	// Enough enemies to satisfy the component's construction-default CrowdThreshold (5).
	for (int32 Index = 0; Index < ResetComponent->OvercrowdCrowdThreshold; ++Index)
	{
		AEnemyBaseTestActor* Enemy = ResetWorld->SpawnActor<AEnemyBaseTestActor>();
		if (!TestNotNull(TEXT("Reset-scenario AEnemyBaseTestActor should spawn"), Enemy))
		{
			return false;
		}
		Enemy->TickCheckDetection(FVector::ZeroVector); // Idle -> Alert
	}

	// Accumulate a partial duration below the default 2.0s duration - stays Inactive.
	ResetComponent->AdvancePanicOverloadState(1.5f);
	TestEqual(TEXT("Partial accumulation below the default duration should stay Inactive"),
		static_cast<uint8>(ResetComponent->GetPanicOverloadState()), static_cast<uint8>(EPanicOverloadState::Inactive));

	// A level-threshold change with the same qualifying enemy count but a much shorter
	// duration must not inherit the 1.5s already accumulated - if it did, the next
	// advance (0.9s < the new 1.0s duration) would still flip to Active (1.5 + 0.9 = 2.4
	// > 1.0), which the assertion below would catch.
	FOvercrowdLevelThreshold ResetEntry;
	ResetEntry.LevelIndex = 2;
	ResetEntry.CrowdThreshold = ResetComponent->OvercrowdCrowdThreshold;
	ResetEntry.RadiusUnits = ResetComponent->OvercrowdRadiusUnits;
	ResetEntry.UncontrolledDurationSeconds = 1.0f;
	ResetComponent->LevelThresholds = { ResetEntry };
	ResetComponent->NotifyLevelReached(2);

	ResetComponent->AdvancePanicOverloadState(0.9f);
	TestEqual(TEXT("NotifyLevelReached should discard the old accumulation, not carry it into the new duration"),
		static_cast<uint8>(ResetComponent->GetPanicOverloadState()), static_cast<uint8>(EPanicOverloadState::Inactive));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
