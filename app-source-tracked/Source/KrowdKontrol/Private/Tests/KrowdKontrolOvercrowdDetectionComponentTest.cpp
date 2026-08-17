// Confirms UOvercrowdDetectionComponent (issue #16, PRD 08 Punishment 3) flips
// EPanicOverloadState from Inactive to Active exactly once, only once a
// hot-and-uncontrolled (AEnemyBase::GetEnemyState() == Alert || Attack, never
// Controlled) enemy count has stayed at/above OvercrowdCrowdThreshold, within
// OvercrowdRadiusUnits of the owning pawn, for a continuous
// OvercrowdUncontrolledDurationSeconds - and that a count drop before the duration
// elapses discards the partial accumulation rather than merely pausing it.
//
// AdvancePanicOverloadState() is called directly (never via a real TickComponent()
// loop) for the same synchronous-determinism reasons
// KrowdKontrolAbilityCooldownTest.cpp/KrowdKontrolMusicSubsystemTest.cpp document.
// Each scenario that needs an isolated enemy count uses its own
// FAutomationEditorCommonUtils::CreateNewMap() World, since
// CountHotUncontrolledEnemiesNearby() iterates every AEnemyBase in the component's
// GetWorld() - reusing one World across scenarios would let an earlier scenario's
// enemies leak into a later count.
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the other
// KrowdKontrol.Unit.* tests.

#include "Misc/AutomationTest.h"
#include "OvercrowdDetectionComponent.h"
#include "PanicOverloadStateTestListener.h"
#include "EnemyBaseTestActor.h"
#include "AbilitySlot.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Components/SceneComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolOvercrowdDetectionComponentTest,
	"KrowdKontrol.Unit.OvercrowdDetectionComponent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolOvercrowdDetectionComponentTest::RunTest(const FString& Parameters)
{
	// --- Scenario 1: default state, below-threshold, at-threshold-but-not-yet-
	// sustained, sustained-flip, no-re-broadcast (cases a-e). ---
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
	{
		return false;
	}

	APawn* PlayerPawn = World->SpawnActor<APawn>();
	if (!TestNotNull(TEXT("APawn should spawn into the test World"), PlayerPawn))
	{
		return false;
	}

	UOvercrowdDetectionComponent* Component = NewObject<UOvercrowdDetectionComponent>(PlayerPawn);
	Component->RegisterComponent();

	UPanicOverloadStateTestListener* Listener = NewObject<UPanicOverloadStateTestListener>();
	Component->OnPanicOverloadStateChanged.AddDynamic(Listener, &UPanicOverloadStateTestListener::HandlePanicOverloadStateChanged);

	const float Duration = Component->OvercrowdUncontrolledDurationSeconds;

	// (a) default state, before any refresh.
	TestEqual(TEXT("Default panic overload state should be Inactive"),
		static_cast<uint8>(Component->GetPanicOverloadState()), static_cast<uint8>(EPanicOverloadState::Inactive));

	// (b) below-threshold count never triggers, regardless of duration.
	for (int32 Index = 0; Index < Component->OvercrowdCrowdThreshold - 1; ++Index)
	{
		AEnemyBaseTestActor* Enemy = World->SpawnActor<AEnemyBaseTestActor>();
		if (!TestNotNull(TEXT("Below-threshold AEnemyBaseTestActor should spawn"), Enemy))
		{
			return false;
		}
		Enemy->TickCheckDetection(FVector::ZeroVector); // Idle -> Alert, at the player's location
	}
	Component->AdvancePanicOverloadState(Duration + 10.0f);
	TestEqual(TEXT("Below-threshold count should leave state Inactive even past the full duration"),
		static_cast<uint8>(Component->GetPanicOverloadState()), static_cast<uint8>(EPanicOverloadState::Inactive));
	TestEqual(TEXT("OnPanicOverloadStateChanged should not have fired yet"), Listener->CallCount, 0);

	// (c) count reaches threshold, but the sustained duration is not yet satisfied.
	AEnemyBaseTestActor* ThresholdEnemy = World->SpawnActor<AEnemyBaseTestActor>();
	if (!TestNotNull(TEXT("Threshold-reaching AEnemyBaseTestActor should spawn"), ThresholdEnemy))
	{
		return false;
	}
	ThresholdEnemy->TickCheckDetection(FVector::ZeroVector); // Idle -> Alert
	Component->AdvancePanicOverloadState(Duration * 0.25f);
	Component->AdvancePanicOverloadState(Duration * 0.25f);
	TestEqual(TEXT("At-threshold count with insufficient accumulated duration should stay Inactive"),
		static_cast<uint8>(Component->GetPanicOverloadState()), static_cast<uint8>(EPanicOverloadState::Inactive));
	TestEqual(TEXT("OnPanicOverloadStateChanged should still not have fired"), Listener->CallCount, 0);

	// (d) the call that pushes accumulated duration past the threshold flips to Active
	// and broadcasts exactly once.
	Component->AdvancePanicOverloadState(Duration);
	TestEqual(TEXT("Sustained at-threshold count for the full duration should flip state to Active"),
		static_cast<uint8>(Component->GetPanicOverloadState()), static_cast<uint8>(EPanicOverloadState::Active));
	TestEqual(TEXT("OnPanicOverloadStateChanged should have fired exactly once"), Listener->CallCount, 1);
	TestEqual(TEXT("Broadcast should carry Active"),
		static_cast<uint8>(Listener->LastState), static_cast<uint8>(EPanicOverloadState::Active));

	// (e) a further advance must not re-broadcast.
	Component->AdvancePanicOverloadState(1.0f);
	TestEqual(TEXT("A further advance while already Active should not re-fire OnPanicOverloadStateChanged"), Listener->CallCount, 1);

	// --- Scenario 2: a count drop before the duration elapses discards the partial
	// accumulation, so reaching Active afterward requires the full duration again, not
	// just the remainder (case g). Fresh World/component - Scenario 1's now-Active
	// component and its enemies must not leak into this count. ---
	UWorld* DropWorld = FAutomationEditorCommonUtils::CreateNewMap();
	if (!TestNotNull(TEXT("CreateNewMap should return a valid World for the drop scenario"), DropWorld))
	{
		return false;
	}

	APawn* DropPlayerPawn = DropWorld->SpawnActor<APawn>();
	UOvercrowdDetectionComponent* DropComponent = NewObject<UOvercrowdDetectionComponent>(DropPlayerPawn);
	DropComponent->RegisterComponent();

	TArray<AEnemyBaseTestActor*> DropEnemies;
	for (int32 Index = 0; Index < DropComponent->OvercrowdCrowdThreshold; ++Index)
	{
		AEnemyBaseTestActor* Enemy = DropWorld->SpawnActor<AEnemyBaseTestActor>();
		if (!TestNotNull(TEXT("Drop-scenario AEnemyBaseTestActor should spawn"), Enemy))
		{
			return false;
		}
		Enemy->TickCheckDetection(FVector::ZeroVector); // Idle -> Alert
		DropEnemies.Add(Enemy);
	}

	const float DropDuration = DropComponent->OvercrowdUncontrolledDurationSeconds;
	DropComponent->AdvancePanicOverloadState(DropDuration * 0.5f);
	TestEqual(TEXT("Partial accumulation below the full duration should stay Inactive"),
		static_cast<uint8>(DropComponent->GetPanicOverloadState()), static_cast<uint8>(EPanicOverloadState::Inactive));

	// Drop one enemy below the qualifying set by controlling it - count now falls
	// below OvercrowdCrowdThreshold, which must reset the accumulator to 0.
	DropEnemies[0]->ReceiveControl(EAbilitySlot::Stun);
	DropComponent->AdvancePanicOverloadState(0.01f);
	TestEqual(TEXT("A count drop below threshold should leave state Inactive"),
		static_cast<uint8>(DropComponent->GetPanicOverloadState()), static_cast<uint8>(EPanicOverloadState::Inactive));

	// Bring the qualifying count back to threshold with a replacement enemy.
	AEnemyBaseTestActor* ReplacementEnemy = DropWorld->SpawnActor<AEnemyBaseTestActor>();
	if (!TestNotNull(TEXT("Replacement AEnemyBaseTestActor should spawn"), ReplacementEnemy))
	{
		return false;
	}
	ReplacementEnemy->TickCheckDetection(FVector::ZeroVector); // Idle -> Alert

	// Less than the full duration (but more than the discarded partial accumulation
	// would have needed to finish) should still be Inactive - proves the reset was a
	// full discard, not a pause.
	DropComponent->AdvancePanicOverloadState(DropDuration * 0.6f);
	TestEqual(TEXT("Re-arming after a drop should require the full duration again, not just the discarded remainder"),
		static_cast<uint8>(DropComponent->GetPanicOverloadState()), static_cast<uint8>(EPanicOverloadState::Inactive));
	DropComponent->AdvancePanicOverloadState(DropDuration * 0.6f);
	TestEqual(TEXT("Once the full duration has re-accumulated post-drop, state should flip to Active"),
		static_cast<uint8>(DropComponent->GetPanicOverloadState()), static_cast<uint8>(EPanicOverloadState::Active));

	// --- Scenario 3: radius exclusion (case h) - a hot-and-uncontrolled enemy outside
	// OvercrowdRadiusUnits must not count toward the qualifying total. ---
	UWorld* RadiusWorld = FAutomationEditorCommonUtils::CreateNewMap();
	if (!TestNotNull(TEXT("CreateNewMap should return a valid World for the radius scenario"), RadiusWorld))
	{
		return false;
	}

	APawn* RadiusPlayerPawn = RadiusWorld->SpawnActor<APawn>();
	UOvercrowdDetectionComponent* RadiusComponent = NewObject<UOvercrowdDetectionComponent>(RadiusPlayerPawn);
	RadiusComponent->RegisterComponent();

	TArray<AEnemyBaseTestActor*> RadiusEnemies;
	for (int32 Index = 0; Index < RadiusComponent->OvercrowdCrowdThreshold; ++Index)
	{
		AEnemyBaseTestActor* Enemy = RadiusWorld->SpawnActor<AEnemyBaseTestActor>();
		if (!TestNotNull(TEXT("Radius-scenario AEnemyBaseTestActor should spawn"), Enemy))
		{
			return false;
		}
		Enemy->TickCheckDetection(FVector::ZeroVector); // Idle -> Alert
		RadiusEnemies.Add(Enemy);
	}
	// AEnemyBaseTestActor now has a default RootComponent (issue #122), but this test
	// still gives this one enemy its own scene root, swapped in via
	// SetRootComponent(), purely so the test can move it independently of the others
	// without disturbing the shared default root the other RadiusEnemies rely on.
	USceneComponent* MovableRoot = NewObject<USceneComponent>(RadiusEnemies[0]);
	MovableRoot->RegisterComponent();
	RadiusEnemies[0]->SetRootComponent(MovableRoot);

	// Move one qualifying-by-state enemy far outside the radius - the count should
	// drop to one below threshold.
	RadiusEnemies[0]->SetActorLocation(FVector(RadiusComponent->OvercrowdRadiusUnits * 10.0f, 0.0f, 0.0f));

	RadiusComponent->AdvancePanicOverloadState(RadiusComponent->OvercrowdUncontrolledDurationSeconds + 10.0f);
	TestEqual(TEXT("An out-of-radius hot-and-uncontrolled enemy should not count toward the threshold"),
		static_cast<uint8>(RadiusComponent->GetPanicOverloadState()), static_cast<uint8>(EPanicOverloadState::Inactive));

	// The radius check is inclusive (<=) - an enemy placed exactly on the boundary
	// must still count. Bring the qualifying count back up to threshold with an enemy
	// at precisely OvercrowdRadiusUnits from the owner.
	AEnemyBaseTestActor* BoundaryEnemy = RadiusWorld->SpawnActor<AEnemyBaseTestActor>();
	if (!TestNotNull(TEXT("Boundary AEnemyBaseTestActor should spawn"), BoundaryEnemy))
	{
		return false;
	}
	BoundaryEnemy->TickCheckDetection(FVector::ZeroVector); // Idle -> Alert
	USceneComponent* BoundaryRoot = NewObject<USceneComponent>(BoundaryEnemy);
	BoundaryRoot->RegisterComponent();
	BoundaryEnemy->SetRootComponent(BoundaryRoot);
	BoundaryEnemy->SetActorLocation(FVector(RadiusComponent->OvercrowdRadiusUnits, 0.0f, 0.0f));

	RadiusComponent->AdvancePanicOverloadState(RadiusComponent->OvercrowdUncontrolledDurationSeconds + 10.0f);
	TestEqual(TEXT("A hot-and-uncontrolled enemy exactly at OvercrowdRadiusUnits should still count (inclusive boundary)"),
		static_cast<uint8>(RadiusComponent->GetPanicOverloadState()), static_cast<uint8>(EPanicOverloadState::Active));

	// --- Scenario 4: Controlled-exclusion (case i) - the core acceptance criterion.
	// Enough enemies to meet OvercrowdCrowdThreshold if merely Alert/Attack, but all
	// driven to Controlled, must never count. ---
	UWorld* ControlledWorld = FAutomationEditorCommonUtils::CreateNewMap();
	if (!TestNotNull(TEXT("CreateNewMap should return a valid World for the controlled scenario"), ControlledWorld))
	{
		return false;
	}

	APawn* ControlledPlayerPawn = ControlledWorld->SpawnActor<APawn>();
	UOvercrowdDetectionComponent* ControlledComponent = NewObject<UOvercrowdDetectionComponent>(ControlledPlayerPawn);
	ControlledComponent->RegisterComponent();

	for (int32 Index = 0; Index < ControlledComponent->OvercrowdCrowdThreshold; ++Index)
	{
		AEnemyBaseTestActor* Enemy = ControlledWorld->SpawnActor<AEnemyBaseTestActor>();
		if (!TestNotNull(TEXT("Controlled-scenario AEnemyBaseTestActor should spawn"), Enemy))
		{
			return false;
		}
		Enemy->TickCheckDetection(FVector::ZeroVector); // Idle -> Alert
		Enemy->ReceiveControl(EAbilitySlot::Stun); // Alert -> Controlled - excluded here
	}

	ControlledComponent->AdvancePanicOverloadState(ControlledComponent->OvercrowdUncontrolledDurationSeconds + 10.0f);
	TestEqual(TEXT("An all-Controlled crowd must never trigger Panic Overload, even past the full duration"),
		static_cast<uint8>(ControlledComponent->GetPanicOverloadState()), static_cast<uint8>(EPanicOverloadState::Inactive));

	// --- Scenario 5: no owning actor (GetOwner() null) must not crash and must leave
	// the component Inactive - covers CountHotUncontrolledEnemiesNearby()'s defensive
	// early-out. Not RegisterComponent()'d, since that requires an owning Actor. ---
	UOvercrowdDetectionComponent* OwnerlessComponent = NewObject<UOvercrowdDetectionComponent>();
	OwnerlessComponent->AdvancePanicOverloadState(OwnerlessComponent->OvercrowdUncontrolledDurationSeconds + 10.0f);
	TestEqual(TEXT("A component with no owning Actor should not crash and should stay Inactive"),
		static_cast<uint8>(OwnerlessComponent->GetPanicOverloadState()), static_cast<uint8>(EPanicOverloadState::Inactive));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
