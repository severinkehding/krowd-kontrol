// Confirms AKrowdKontrolPlayerController::ApplyStarterSkillEffects (issue #375,
// docs/prd-mastery-skill-tree.md REQ-3) actually mutates the possessed pawn's real
// tunables at run start for every currently-unlocked Crowd Mastery bubble, leaves a
// locked bubble's tunable untouched, and never double-applies on a repeat
// OnPossess(). Mirrors KrowdKontrolHUDWiringTest.cpp's spawn/possess/DispatchBeginPlay
// scaffold, KrowdKontrolCrowdMasteryTotalSubsystemSpendTest.cpp's in-code DataTable
// fixture, and KrowdKontrolLevelFailedTest.cpp's friend-injected subsystem idiom
// (CreateNewMap() worlds have a null GetGameInstance()). Covers all 4 wired effects
// (cooldown reduction, move speed, world-scoped pen-zone radius, and - via a second,
// isolated pawn/controller/subsystem fixture, since the effect-hook gate is
// per-hook-ID not per-bubble and can't coexist with the shared-component locked-case
// assertion below - energy max), plus the locked-bubble, null-InPawn-guard,
// missing-subsystem-warn-once, and idempotency cases.

#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "KrowdKontrolPlayerController.h"
#include "CrowdMasteryTotalSubsystem.h"
#include "MasteryTreeData.h"
#include "FlatCamera3DPrototypePawn.h"
#include "AbilityCooldownComponent.h"
#include "PlayerEnergyComponent.h"
#include "TargetZone.h"
#include "Components/BoxComponent.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Engine.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolStarterSkillEffectWiringTest,
	"KrowdKontrol.Unit.StarterSkillEffectWiring",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolStarterSkillEffectWiringTest::RunTest(const FString& Parameters)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
	{
		return false;
	}

	AFlatCamera3DPrototypePawn* Pawn = World->SpawnActor<AFlatCamera3DPrototypePawn>();
	if (!TestNotNull(TEXT("Pawn should spawn"), Pawn))
	{
		return false;
	}

	AKrowdKontrolPlayerController* Controller = World->SpawnActor<AKrowdKontrolPlayerController>();
	if (!TestNotNull(TEXT("Controller should spawn"), Controller))
	{
		return false;
	}

	// A null-InPawn call must not mark the one-shot guard applied (KrowdKontrolPlayerController.h's
	// doc comment) - if it wrongly did, the real Possess() below would short-circuit and none of
	// the effect assertions further down would hold.
	Controller->ApplyStarterSkillEffects(nullptr);

	ATargetZone* Zone = World->SpawnActor<ATargetZone>();
	if (!TestNotNull(TEXT("Zone should spawn"), Zone)
		|| !TestNotNull(TEXT("Zone should have a ZoneCollisionComponent"), Zone->ZoneCollisionComponent.Get()))
	{
		return false;
	}
	const FVector PreZoneExtent = Zone->ZoneCollisionComponent->GetUnscaledBoxExtent();

	UAbilityCooldownComponent* CooldownComponent = Pawn->FindComponentByClass<UAbilityCooldownComponent>();
	UFloatingPawnMovement* Movement = Pawn->FindComponentByClass<UFloatingPawnMovement>();
	UPlayerEnergyComponent* EnergyComponent = Pawn->FindComponentByClass<UPlayerEnergyComponent>();
	if (!TestNotNull(TEXT("Pawn should have an UAbilityCooldownComponent"), CooldownComponent)
		|| !TestNotNull(TEXT("Pawn should have an UFloatingPawnMovement"), Movement)
		|| !TestNotNull(TEXT("Pawn should have an UPlayerEnergyComponent"), EnergyComponent))
	{
		return false;
	}

	// Recorded before Possess() below - Possess() synchronously triggers OnPossess(),
	// which is one of ApplyStarterSkillEffects' two call sites, so pre-values must be
	// captured ahead of it, not ahead of DispatchBeginPlay().
	const TArray<float> PreCooldownDurations = CooldownComponent->AbilityCooldownDurations;
	const float PreMaxSpeed = Movement->MaxSpeed;
	const float PreMaxEnergy = EnergyComponent->MaxEnergy;

	// Fixture tree: a single root node (no prerequisite) with 4 bubbles - three spent
	// (cooldown reduction, move speed, pen-zone radius), one deliberately left locked
	// (energy max), so the test proves both halves of the AC in one pass: an unlocked
	// bubble's effect applies, a locked one's doesn't.
	UDataTable* Table = NewObject<UDataTable>();
	Table->RowStruct = FMasteryTreeNode::StaticStruct();

	FMasteryTreeNode RootRow;
	RootRow.ParentNodeId = NAME_None;
	RootRow.Phase = EMasteryTreePhase::Phase1;

	FMasterySkillBubble CooldownBubble;
	CooldownBubble.BubbleId = FName(TEXT("B_Cooldown"));
	CooldownBubble.PointCost = 0;
	CooldownBubble.EffectHookId = FName(TEXT("AbilityCooldownReduction"));
	RootRow.Bubbles.Add(CooldownBubble);

	FMasterySkillBubble SpeedBubble;
	SpeedBubble.BubbleId = FName(TEXT("B_Speed"));
	SpeedBubble.PointCost = 0;
	SpeedBubble.EffectHookId = FName(TEXT("MovementSpeedBonus"));
	RootRow.Bubbles.Add(SpeedBubble);

	FMasterySkillBubble EnergyLockedBubble;
	EnergyLockedBubble.BubbleId = FName(TEXT("B_EnergyLocked"));
	EnergyLockedBubble.PointCost = 0;
	EnergyLockedBubble.EffectHookId = FName(TEXT("EnergyMaxIncrease"));
	RootRow.Bubbles.Add(EnergyLockedBubble);

	FMasterySkillBubble PenZoneBubble;
	PenZoneBubble.BubbleId = FName(TEXT("B_PenZone"));
	PenZoneBubble.PointCost = 0;
	PenZoneBubble.EffectHookId = FName(TEXT("PenZoneRadiusBonus"));
	RootRow.Bubbles.Add(PenZoneBubble);

	Table->AddRow(FName(TEXT("Node_Root")), RootRow);

	UGameInstance* GameInstanceOuter = NewObject<UGameInstance>();
	UCrowdMasteryTotalSubsystem* MasterySubsystem = NewObject<UCrowdMasteryTotalSubsystem>(GameInstanceOuter);
	MasterySubsystem->MasteryTreeTable = Table;

	TestTrue(TEXT("Spending on the cooldown-reduction bubble should succeed"),
		MasterySubsystem->TrySpendOnBubble(FName(TEXT("B_Cooldown"))));
	TestTrue(TEXT("Spending on the move-speed bubble should succeed"),
		MasterySubsystem->TrySpendOnBubble(FName(TEXT("B_Speed"))));
	TestTrue(TEXT("Spending on the pen-zone-radius bubble should succeed"),
		MasterySubsystem->TrySpendOnBubble(FName(TEXT("B_PenZone"))));
	// B_EnergyLocked is deliberately never spent.

	// Injected before Possess() (which synchronously calls OnPossess() ->
	// ApplyStarterSkillEffects()), so the real effect-application path - not just
	// DispatchBeginPlay()'s fallback branch - sees a live subsystem.
	Controller->CachedCrowdMasteryTotalSubsystem = MasterySubsystem;
	Controller->Possess(Pawn);
	Controller->Player = NewObject<ULocalPlayer>(GEngine);
	Controller->SetAsLocalPlayerController();

	// BeginPlay()'s own ApplyStarterSkillEffects() call is exercised here too, but the
	// bStarterSkillEffectsApplied guard (already set by the Possess() call above) means
	// this is itself the idempotency case: a second call site running into an
	// already-applied guard must not compound the bonus a second time.
	Controller->DispatchBeginPlay();

	if (TestEqual(TEXT("AbilityCooldownDurations entry count should be unchanged"),
		CooldownComponent->AbilityCooldownDurations.Num(), PreCooldownDurations.Num()))
	{
		for (int32 Index = 0; Index < PreCooldownDurations.Num(); ++Index)
		{
			TestEqual(TEXT("Each cooldown duration should be scaled down by the starter multiplier"),
				CooldownComponent->AbilityCooldownDurations[Index], PreCooldownDurations[Index] * 0.8f);
		}
	}

	TestEqual(TEXT("MaxSpeed should be scaled up by the starter multiplier for the unlocked move-speed bubble"),
		Movement->MaxSpeed, PreMaxSpeed * 1.15f);

	TestEqual(TEXT("MaxEnergy should be unchanged - EnergyMaxIncrease's bubble was never unlocked"),
		EnergyComponent->MaxEnergy, PreMaxEnergy);

	TestEqual(TEXT("ZoneCollisionComponent extent should be scaled up by the starter multiplier for the unlocked pen-zone bubble"),
		Zone->ZoneCollisionComponent->GetUnscaledBoxExtent(), PreZoneExtent * 1.2f);

	// Idempotency: a second possession of the same controller must not re-multiply
	// MaxSpeed again.
	const float MaxSpeedAfterFirstApply = Movement->MaxSpeed;
	Controller->OnPossess(Pawn);
	TestEqual(TEXT("A repeat OnPossess should not double-apply the move-speed bonus"),
		Movement->MaxSpeed, MaxSpeedAfterFirstApply);

	// EnergyMaxIncrease's positive (unlocked) path can't share the fixture above -
	// ApplyStarterSkillEffects gates each effect on hook-ID presence in
	// GetUnlockedEffectHookIds(), not per-bubble, so unlocking a second bubble mapped to
	// the same "EnergyMaxIncrease" hook would also flip the locked-case assertion above.
	// A second, isolated pawn/controller/subsystem proves the unlocked case without
	// disturbing that assertion.
	AFlatCamera3DPrototypePawn* EnergyPawn = World->SpawnActor<AFlatCamera3DPrototypePawn>();
	AKrowdKontrolPlayerController* EnergyController = World->SpawnActor<AKrowdKontrolPlayerController>();
	if (!TestNotNull(TEXT("Energy-unlocked pawn should spawn"), EnergyPawn)
		|| !TestNotNull(TEXT("Energy-unlocked controller should spawn"), EnergyController))
	{
		return false;
	}
	UPlayerEnergyComponent* EnergyPawnEnergyComponent = EnergyPawn->FindComponentByClass<UPlayerEnergyComponent>();
	if (!TestNotNull(TEXT("Energy-unlocked pawn should have an UPlayerEnergyComponent"), EnergyPawnEnergyComponent))
	{
		return false;
	}
	const float PreUnlockedMaxEnergy = EnergyPawnEnergyComponent->MaxEnergy;

	UDataTable* EnergyUnlockedTable = NewObject<UDataTable>();
	EnergyUnlockedTable->RowStruct = FMasteryTreeNode::StaticStruct();

	FMasteryTreeNode EnergyUnlockedRootRow;
	EnergyUnlockedRootRow.ParentNodeId = NAME_None;
	EnergyUnlockedRootRow.Phase = EMasteryTreePhase::Phase1;

	FMasterySkillBubble EnergyUnlockedBubble;
	EnergyUnlockedBubble.BubbleId = FName(TEXT("B_EnergyUnlocked"));
	EnergyUnlockedBubble.PointCost = 0;
	EnergyUnlockedBubble.EffectHookId = FName(TEXT("EnergyMaxIncrease"));
	EnergyUnlockedRootRow.Bubbles.Add(EnergyUnlockedBubble);

	EnergyUnlockedTable->AddRow(FName(TEXT("Node_Root")), EnergyUnlockedRootRow);

	UGameInstance* EnergyGameInstanceOuter = NewObject<UGameInstance>();
	UCrowdMasteryTotalSubsystem* EnergyMasterySubsystem = NewObject<UCrowdMasteryTotalSubsystem>(EnergyGameInstanceOuter);
	EnergyMasterySubsystem->MasteryTreeTable = EnergyUnlockedTable;

	TestTrue(TEXT("Spending on the energy-unlocked bubble should succeed"),
		EnergyMasterySubsystem->TrySpendOnBubble(FName(TEXT("B_EnergyUnlocked"))));

	EnergyController->CachedCrowdMasteryTotalSubsystem = EnergyMasterySubsystem;
	EnergyController->Possess(EnergyPawn);

	TestEqual(TEXT("MaxEnergy should be scaled up by the starter multiplier for the unlocked energy bubble"),
		EnergyPawnEnergyComponent->MaxEnergy, PreUnlockedMaxEnergy * 1.25f);

	// ResolveCrowdMasteryTotalSubsystem's missing-subsystem warning must fire exactly
	// once (warn-once), not on every ApplyStarterSkillEffects call - mirrors
	// KrowdKontrolLevelFailedTest.cpp's analogous ULevelClearTimeSubsystem case. No
	// CachedCrowdMasteryTotalSubsystem is injected here, and GetGameInstance() is null
	// in this CreateNewMap() World, so resolution genuinely fails both times.
	AFlatCamera3DPrototypePawn* UnresolvedPawn = World->SpawnActor<AFlatCamera3DPrototypePawn>();
	AKrowdKontrolPlayerController* UnresolvedController = World->SpawnActor<AKrowdKontrolPlayerController>();
	if (!TestNotNull(TEXT("Unresolved-subsystem pawn should spawn"), UnresolvedPawn)
		|| !TestNotNull(TEXT("Unresolved-subsystem controller should spawn"), UnresolvedController))
	{
		return false;
	}

	AddExpectedError(TEXT("no UCrowdMasteryTotalSubsystem available"), EAutomationExpectedErrorFlags::Contains, 1);
	UnresolvedController->ApplyStarterSkillEffects(UnresolvedPawn);
	TestTrue(TEXT("ApplyStarterSkillEffects must not crash with no resolvable subsystem"), true);

	// A second call must not log the warning again (warn-once) - and, per the guard-timing
	// fix, must genuinely retry resolution rather than short-circuit on a guard the first
	// (failed) call left set.
	UnresolvedController->ApplyStarterSkillEffects(UnresolvedPawn);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
