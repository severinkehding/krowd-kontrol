// Confirms AKrowdKontrolPlayerController::ApplyStarterSkillEffects (issue #375,
// docs/prd-mastery-skill-tree.md REQ-3) actually mutates the possessed pawn's real
// tunables at run start for every currently-unlocked Crowd Mastery bubble, leaves a
// locked bubble's tunable untouched, and never double-applies on a repeat
// OnPossess(). Mirrors KrowdKontrolHUDWiringTest.cpp's spawn/possess/DispatchBeginPlay
// scaffold, KrowdKontrolCrowdMasteryTotalSubsystemSpendTest.cpp's in-code DataTable
// fixture, and KrowdKontrolLevelFailedTest.cpp's friend-injected subsystem idiom
// (CreateNewMap() worlds have a null GetGameInstance()). Covers 2 of the 4 wired
// effects (cooldown reduction, move speed) plus the locked-bubble and idempotency
// cases; PenZoneRadiusBonus shares the identical Contains()-gated branch shape as
// these two and is not separately asserted here, same "don't need N tests for N
// near-identical branches" judgment call KrowdKontrolMasteryTreeDataTest.cpp's own
// BuildFourBubbles() helper already makes.

#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "KrowdKontrolPlayerController.h"
#include "CrowdMasteryTotalSubsystem.h"
#include "MasteryTreeData.h"
#include "FlatCamera3DPrototypePawn.h"
#include "AbilityCooldownComponent.h"
#include "PlayerEnergyComponent.h"
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

	// Fixture tree: a single root node (no prerequisite) with 3 bubbles - two spent
	// (cooldown reduction, move speed), one deliberately left locked (energy max), so
	// the test proves both halves of the AC in one pass: an unlocked bubble's effect
	// applies, a locked one's doesn't.
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

	Table->AddRow(FName(TEXT("Node_Root")), RootRow);

	UGameInstance* GameInstanceOuter = NewObject<UGameInstance>();
	UCrowdMasteryTotalSubsystem* MasterySubsystem = NewObject<UCrowdMasteryTotalSubsystem>(GameInstanceOuter);
	MasterySubsystem->MasteryTreeTable = Table;

	TestTrue(TEXT("Spending on the cooldown-reduction bubble should succeed"),
		MasterySubsystem->TrySpendOnBubble(FName(TEXT("B_Cooldown"))));
	TestTrue(TEXT("Spending on the move-speed bubble should succeed"),
		MasterySubsystem->TrySpendOnBubble(FName(TEXT("B_Speed"))));
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

	// Idempotency: a second possession of the same controller must not re-multiply
	// MaxSpeed again.
	const float MaxSpeedAfterFirstApply = Movement->MaxSpeed;
	Controller->OnPossess(Pawn);
	TestEqual(TEXT("A repeat OnPossess should not double-apply the move-speed bonus"),
		Movement->MaxSpeed, MaxSpeedAfterFirstApply);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
