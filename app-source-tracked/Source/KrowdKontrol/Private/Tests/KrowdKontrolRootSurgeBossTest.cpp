// Confirms ARootSurgeBoss (issue #50, PRD 04 Mid-boss 2): (1) it reaches Armed with
// the arming tell lit within the first moment of BeginPlay() (AC #3), (2) its
// UWaveSpawnerComponent waves are all TR-UPR/ATrooperEnemy with every DelaySeconds
// strictly accelerated below its BaselineWaveDelaySeconds counterpart, and
// WaveDelayAccelerationMultiplier is strictly < 1.0 (AC #1, #5 - the literal,
// operator-clarified contract), (3) the boss stays Armed with no Root-locked add
// present, (4) a Root-locked enemy that is NOT one of this boss's own spawned adds
// does not count, (5) the boss advances to Vulnerable the first time one of its own
// spawned adds is found Controlled with Root (AC #4), (6) a wrong-ability-controlled
// own-add does not trigger it, (7) Vulnerable never reverts even once the qualifying
// add stops being Controlled, (8) the attack telegraph fires and damages the player's
// UPlayerEnergyComponent by exactly UPlayerEnergyComponent::MaxDamagePerHit (not the
// raw AttackDamageAmount) independently of wave-spawn state (AC #2), (9) a Banked boss
// stops attacking and stops re-checking Vulnerable, (10) a never-spawned-into-a-world
// instance does not crash, (11) Tick() itself (not just CheckVulnerableState()
// directly) drives the Vulnerable check, and (12) the attack telegraph is
// range-gated - a player beyond AttackRangeUnits takes no damage and the attack tell
// does not light even once the telegraph timer fully elapses (AC #2).
//
// CheckVulnerableState()/AdvanceAttackTelegraph() are called directly where
// determinism is needed (never via a real per-frame Tick() loop, except where Tick()
// itself is under test), same synchronous-determinism reasoning
// KrowdKontrolSleepShieldBossTest.cpp documents for its own directly-driven private
// methods. UWaveSpawnerComponent::TriggerNextWave() (WaveSpawnerComponent.h) is used
// to fire a scheduled wave synchronously instead of waiting on its FTimerManager delay
// - the component's own documented "no opinion on what triggers it" contract.
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the other
// KrowdKontrol.Unit.* tests.

#include "Misc/AutomationTest.h"
#include "RootSurgeBoss.h"
#include "EnemyBaseTestActor.h"
#include "TrooperEnemy.h"
#include "AbilitySlot.h"
#include "PlayerEnergyComponent.h"
#include "WaveSpawnerComponent.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"
#include "Components/PointLightComponent.h"
#include "GameFramework/Pawn.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolRootSurgeBossTest,
	"KrowdKontrol.Unit.RootSurgeBoss",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolRootSurgeBossTest::RunTest(const FString& Parameters)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
	{
		return false;
	}

	// --- Scenario 1: Armed + arming tell lit immediately on BeginPlay() (AC #3). ---
	ARootSurgeBoss* Boss = World->SpawnActor<ARootSurgeBoss>();
	if (!TestNotNull(TEXT("ARootSurgeBoss should spawn into the test World"), Boss))
	{
		return false;
	}
	Boss->DispatchBeginPlay();

	TestEqual(TEXT("Boss should be Armed immediately after BeginPlay()"),
		static_cast<uint8>(Boss->GetBossState()), static_cast<uint8>(EBossState::Armed));
	TestEqual(TEXT("Arming tell light should be lit at ArmingTellIntensity immediately after BeginPlay()"),
		Boss->ArmingTellLightComponent->Intensity, Boss->ArmingTellIntensity);

	// --- Scenario 2: wave cadence is strictly accelerated (AC #1, #5 - the literal
	// operator-clarified contract: config-level comparison, multiplier < 1.0). ---
	TestTrue(TEXT("WaveDelayAccelerationMultiplier should be strictly less than 1.0"),
		Boss->WaveDelayAccelerationMultiplier < 1.0f);
	// AC #2: boss's own attack range must exceed every existing enemy's -
	// ASniperEnemy::GetAttackRangeUnits() (SniperEnemy.cpp) is the current max at 1400.0f.
	TestTrue(TEXT("AttackRangeUnits should exceed ASniperEnemy's 1400.0f (current max range)"),
		Boss->AttackRangeUnits > 1400.0f);
	TestEqual(TEXT("WaveSpawnerComponent should have one Waves entry per BaselineWaveDelaySeconds entry"),
		Boss->WaveSpawnerComponent->Waves.Num(), Boss->BaselineWaveDelaySeconds.Num());
	for (int32 i = 0; i < Boss->WaveSpawnerComponent->Waves.Num(); ++i)
	{
		const FWaveEntry& Entry = Boss->WaveSpawnerComponent->Waves[i];
		TestTrue(TEXT("Each wave's DelaySeconds should be strictly less than its baseline"),
			Entry.DelaySeconds < Boss->BaselineWaveDelaySeconds[i]);
		TestEqual(TEXT("Each wave should spawn ATrooperEnemy (TR-UPR)"),
			Entry.EnemyClass.Get(), ATrooperEnemy::StaticClass());
		TestEqual(TEXT("Each wave's EnemyType tag should be TR-UPR"),
			static_cast<uint8>(Entry.EnemyType), static_cast<uint8>(EEnemyType::TR_UPR));
	}

	// --- Scenario 3: boss stays Armed with no Root-locked add present. ---
	TestEqual(TEXT("Boss should still be Armed with no Root-locked add present"),
		static_cast<uint8>(Boss->GetBossState()), static_cast<uint8>(EBossState::Armed));

	// --- Scenario 4: Vulnerable requires this boss's OWN spawned adds, not just any
	// nearby Root-controlled enemy. ---
	AEnemyBaseTestActor* OutsideAdd = World->SpawnActor<AEnemyBaseTestActor>();
	if (!TestNotNull(TEXT("Standalone (non-boss-spawned) AEnemyBaseTestActor should spawn"), OutsideAdd))
	{
		return false;
	}
	OutsideAdd->TickCheckDetection(FVector::ZeroVector); // Idle -> Alert
	OutsideAdd->ReceiveControl(EAbilitySlot::Root); // Alert -> Controlled, right ability
	OutsideAdd->SetActorLocation(Boss->GetActorLocation());

	Boss->CheckVulnerableState();
	TestEqual(TEXT("Boss should stay Armed - a Root-locked enemy not spawned by its own WaveSpawnerComponent does not count"),
		static_cast<uint8>(Boss->GetBossState()), static_cast<uint8>(EBossState::Armed));

	// --- Scenario 5: Vulnerable advances once one of the boss's own spawned adds is
	// Root-locked (AC #4, core contract). ---
	ARootSurgeBoss* VulnerableBoss = World->SpawnActor<ARootSurgeBoss>();
	if (!TestNotNull(TEXT("Second ARootSurgeBoss for Vulnerable-transition coverage should spawn"), VulnerableBoss))
	{
		return false;
	}
	VulnerableBoss->WaveSpawnerComponent->Waves[0].EnemyClass = AEnemyBaseTestActor::StaticClass();
	VulnerableBoss->DispatchBeginPlay();
	VulnerableBoss->WaveSpawnerComponent->TriggerNextWave(); // fire wave 0 immediately instead of waiting on its timer

	if (!TestEqual(TEXT("Wave 0 should have spawned exactly one actor"),
		VulnerableBoss->WaveSpawnerComponent->GetSpawnedActors().Num(), 1))
	{
		return false;
	}
	AEnemyBaseTestActor* OwnAdd = Cast<AEnemyBaseTestActor>(VulnerableBoss->WaveSpawnerComponent->GetSpawnedActors()[0]);
	if (!TestNotNull(TEXT("The boss's own spawned add should be an AEnemyBaseTestActor"), OwnAdd))
	{
		return false;
	}
	OwnAdd->TickCheckDetection(FVector::ZeroVector); // Idle -> Alert
	OwnAdd->ReceiveControl(EAbilitySlot::Root); // Alert -> Controlled, right ability

	VulnerableBoss->CheckVulnerableState();
	TestEqual(TEXT("Boss should advance to Vulnerable once one of its own spawned adds is Root-locked"),
		static_cast<uint8>(VulnerableBoss->GetBossState()), static_cast<uint8>(EBossState::Vulnerable));

	// --- Scenario 6: a wrong-ability-controlled own-add does not trigger Vulnerable. ---
	ARootSurgeBoss* WrongAbilityBoss = World->SpawnActor<ARootSurgeBoss>();
	if (!TestNotNull(TEXT("Third ARootSurgeBoss for wrong-ability coverage should spawn"), WrongAbilityBoss))
	{
		return false;
	}
	WrongAbilityBoss->WaveSpawnerComponent->Waves[0].EnemyClass = AEnemyBaseTestActor::StaticClass();
	WrongAbilityBoss->DispatchBeginPlay();
	WrongAbilityBoss->WaveSpawnerComponent->TriggerNextWave();

	if (!TestEqual(TEXT("Wave 0 should have spawned exactly one actor for the wrong-ability boss"),
		WrongAbilityBoss->WaveSpawnerComponent->GetSpawnedActors().Num(), 1))
	{
		return false;
	}
	AEnemyBaseTestActor* WrongAbilityAdd = Cast<AEnemyBaseTestActor>(WrongAbilityBoss->WaveSpawnerComponent->GetSpawnedActors()[0]);
	if (!TestNotNull(TEXT("The wrong-ability boss's own spawned add should be an AEnemyBaseTestActor"), WrongAbilityAdd))
	{
		return false;
	}
	WrongAbilityAdd->TickCheckDetection(FVector::ZeroVector); // Idle -> Alert
	WrongAbilityAdd->ReceiveControl(EAbilitySlot::Stun); // Alert -> Controlled, WRONG ability

	WrongAbilityBoss->CheckVulnerableState();
	TestEqual(TEXT("Boss should stay Armed - a wrong-ability-controlled own-add does not count"),
		static_cast<uint8>(WrongAbilityBoss->GetBossState()), static_cast<uint8>(EBossState::Armed));

	// --- Scenario 7: Vulnerable is one-way - even after the qualifying add's Root
	// control naturally expires (reverted Controlled -> Alert), the boss remains
	// Vulnerable (mirrors ASleepShieldBoss's "never reverts" guarantee). ---
	OwnAdd->TickControlledDuration(9999.0f); // force Controlled -> Alert
	VulnerableBoss->CheckVulnerableState();
	TestEqual(TEXT("Boss should remain Vulnerable even after its qualifying add's Root control expires"),
		static_cast<uint8>(VulnerableBoss->GetBossState()), static_cast<uint8>(EBossState::Vulnerable));

	// --- Scenario 8: attack telegraph fires and damages the player independently of
	// wave-spawn state (AC #2). ---
	ARootSurgeBoss* AttackingBoss = World->SpawnActor<ARootSurgeBoss>();
	if (!TestNotNull(TEXT("Fourth ARootSurgeBoss for attack-telegraph coverage should spawn"), AttackingBoss))
	{
		return false;
	}
	APawn* PlayerPawn = World->SpawnActor<APawn>();
	if (!TestNotNull(TEXT("Player pawn should spawn into the test World"), PlayerPawn))
	{
		return false;
	}
	UPlayerEnergyComponent* Energy = NewObject<UPlayerEnergyComponent>(PlayerPawn);
	Energy->RegisterComponent();
	const float EnergyBeforeAttack = Energy->GetCurrentEnergy();

	AttackingBoss->DispatchBeginPlay();
	AttackingBoss->Tick(AttackingBoss->AttackTelegraphSeconds); // one tick covering the full telegraph

	TestEqual(TEXT("Player energy should drop by exactly MaxDamagePerHit, not the raw AttackDamageAmount"),
		Energy->GetCurrentEnergy(), EnergyBeforeAttack - Energy->MaxDamagePerHit);
	TestEqual(TEXT("Attack tell light should be lit once the telegraph fires"),
		AttackingBoss->AttackTellLightComponent->Intensity, AttackingBoss->AttackTellIntensity);

	// --- Scenario 9: a Banked boss stops attacking and stops re-checking Vulnerable
	// (mirrors ASleepShieldBoss Scenario 8's Banked early-return guard coverage). ---
	AttackingBoss->AdvanceToVulnerable();
	AttackingBoss->TransitionToBanked();
	const float EnergyBeforeBankedTick = Energy->GetCurrentEnergy();

	AttackingBoss->Tick(AttackingBoss->AttackTelegraphSeconds);
	TestEqual(TEXT("Player energy should be unchanged once the boss is Banked"),
		Energy->GetCurrentEnergy(), EnergyBeforeBankedTick);
	TestEqual(TEXT("Boss should remain Banked"),
		static_cast<uint8>(AttackingBoss->GetBossState()), static_cast<uint8>(EBossState::Banked));

	// --- Scenario 10: a never-spawned-into-a-world instance must not crash. ---
	ARootSurgeBoss* UnspawnedBoss = NewObject<ARootSurgeBoss>();
	if (!TestNotNull(TEXT("NewObject<ARootSurgeBoss> should succeed"), UnspawnedBoss))
	{
		return false;
	}
	UnspawnedBoss->CheckVulnerableState();
	UnspawnedBoss->AdvanceAttackTelegraph(1.0f);
	TestTrue(TEXT("A never-spawned boss should not crash when checked"), true);

	// --- Scenario 11: Tick() itself (not just CheckVulnerableState() directly) must
	// drive the Vulnerable check - mirrors ASleepShieldBoss's own Tick()-driven
	// regression coverage for the same two-call Tick() dispatcher shape. ---
	ARootSurgeBoss* TickBoss = World->SpawnActor<ARootSurgeBoss>();
	if (!TestNotNull(TEXT("Fifth ARootSurgeBoss for Tick()-driven Vulnerable coverage should spawn"), TickBoss))
	{
		return false;
	}
	TickBoss->WaveSpawnerComponent->Waves[0].EnemyClass = AEnemyBaseTestActor::StaticClass();
	TickBoss->DispatchBeginPlay();
	TickBoss->WaveSpawnerComponent->TriggerNextWave();

	if (!TestEqual(TEXT("Wave 0 should have spawned exactly one actor for the Tick()-coverage boss"),
		TickBoss->WaveSpawnerComponent->GetSpawnedActors().Num(), 1))
	{
		return false;
	}
	AEnemyBaseTestActor* TickBossAdd = Cast<AEnemyBaseTestActor>(TickBoss->WaveSpawnerComponent->GetSpawnedActors()[0]);
	if (!TestNotNull(TEXT("TickBoss's own spawned add should be an AEnemyBaseTestActor"), TickBossAdd))
	{
		return false;
	}
	TickBossAdd->TickCheckDetection(FVector::ZeroVector); // Idle -> Alert
	TickBossAdd->ReceiveControl(EAbilitySlot::Root); // Alert -> Controlled, right ability

	TickBoss->Tick(0.0f);
	TestEqual(TEXT("Tick() should advance the boss to Vulnerable via CheckVulnerableState()"),
		static_cast<uint8>(TickBoss->GetBossState()), static_cast<uint8>(EBossState::Vulnerable));

	// --- Scenario 12: the attack telegraph is range-gated (AC #2) - a player beyond
	// AttackRangeUnits takes no damage and the attack tell does not light, even once
	// the telegraph timer fully elapses. Reuses Scenario 8's PlayerPawn/Energy rather
	// than spawning a second player pawn - FindPlayerEnergyComponent() returns the
	// first UPlayerEnergyComponent-carrying APawn TActorIterator finds, not the
	// nearest, so two such pawns in the World at once would make which one this
	// scenario actually exercises ambiguous. Moves the BOSS away from the player
	// (rather than the player away from the boss) - a bare APawn has no
	// RootComponent, so SetActorLocation()/GetActorLocation() on PlayerPawn is a
	// silent no-op (see AActor::TemplateGetActorLocation's nullptr-RootComponent
	// fallback to FVector::ZeroVector); ARootSurgeBoss always has a real
	// RootComponent (ArmingTellLightComponent), so moving it is reliable. ---
	ARootSurgeBoss* OutOfRangeBoss = World->SpawnActor<ARootSurgeBoss>();
	if (!TestNotNull(TEXT("Sixth ARootSurgeBoss for range-gating coverage should spawn"), OutOfRangeBoss))
	{
		return false;
	}
	OutOfRangeBoss->SetActorLocation(FVector(OutOfRangeBoss->AttackRangeUnits * 2.0f, 0.0f, 0.0f));
	const float EnergyBeforeOutOfRangeAttack = Energy->GetCurrentEnergy();

	OutOfRangeBoss->DispatchBeginPlay();
	OutOfRangeBoss->Tick(OutOfRangeBoss->AttackTelegraphSeconds); // one tick covering the full telegraph

	TestEqual(TEXT("Player energy should be unchanged when the player is beyond AttackRangeUnits"),
		Energy->GetCurrentEnergy(), EnergyBeforeOutOfRangeAttack);
	TestEqual(TEXT("Attack tell light should not light when the telegraph elapses out of range"),
		OutOfRangeBoss->AttackTellLightComponent->Intensity, 0.0f);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
