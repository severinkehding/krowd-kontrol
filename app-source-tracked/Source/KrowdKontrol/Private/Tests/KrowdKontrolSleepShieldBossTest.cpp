// Confirms ASleepShieldBoss (issue #48, PRD 04 Mid-boss 1): (1) it reaches Armed
// with HasShield() true and the tell light lit within the first moment of
// BeginPlay() (well inside the issue's 10 second AC window), (2) a nearby minion
// controlled by any ability other than Sleep never drops the shield, (3) a
// Sleep-controlled minion strictly outside ShieldDropRadiusUnits never drops the
// shield, (4) a Sleep-controlled minion exactly at the radius boundary (inclusive
// <=) drops the shield, turns the tell off, and advances EBossState to
// Vulnerable, (5) once the qualifying minion leaves, HasShield() freely re-raises
// but EBossState never reverts from Vulnerable, and (6) a never-spawned-into-a-
// world instance does not crash.
//
// CheckShieldState() is called directly (never via a real per-frame Tick() loop),
// same synchronous-determinism reasoning
// KrowdKontrolOvercrowdDetectionComponentTest.cpp/KrowdKontrolEnemyBaseTest.cpp
// document for their own directly-driven private methods.
//
// No World->InitializeActorsForPlay(FURL()) call is needed here (unlike
// KrowdKontrolDualZoneBossTest.cpp) - ASleepShieldBoss binds no dynamic delegate to
// another AActor in BeginPlay(); OnShieldChanged is a plain C++ virtual override.
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the other
// KrowdKontrol.Unit.* tests.

#include "Misc/AutomationTest.h"
#include "SleepShieldBoss.h"
#include "EnemyBaseTestActor.h"
#include "AbilitySlot.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"
#include "Components/SceneComponent.h"
#include "Components/PointLightComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolSleepShieldBossTest,
	"KrowdKontrol.Unit.SleepShieldBoss",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolSleepShieldBossTest::RunTest(const FString& Parameters)
{
	// --- Scenario 1: default/no-minion state - Armed, shielded, tell lit,
	// immediately on BeginPlay() (AC #3, #5 first half). ---
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
	{
		return false;
	}

	ASleepShieldBoss* Boss = World->SpawnActor<ASleepShieldBoss>();
	if (!TestNotNull(TEXT("ASleepShieldBoss should spawn into the test World"), Boss))
	{
		return false;
	}
	Boss->DispatchBeginPlay();

	TestEqual(TEXT("Boss should be Armed immediately after BeginPlay()"),
		static_cast<uint8>(Boss->GetBossState()), static_cast<uint8>(EBossState::Armed));
	TestTrue(TEXT("Boss should have its shield up immediately after BeginPlay()"), Boss->HasShield());
	TestEqual(TEXT("Shield tell light should be lit at ShieldTellIntensity immediately after BeginPlay()"),
		Boss->ShieldTellLightComponent->Intensity, Boss->ShieldTellIntensity);

	// --- Scenario 2: a nearby minion controlled by the wrong ability (Stun, not
	// Sleep) must not drop the shield. ---
	AEnemyBaseTestActor* WrongAbilityMinion = World->SpawnActor<AEnemyBaseTestActor>();
	if (!TestNotNull(TEXT("Wrong-ability AEnemyBaseTestActor should spawn"), WrongAbilityMinion))
	{
		return false;
	}
	USceneComponent* WrongAbilityRoot = NewObject<USceneComponent>(WrongAbilityMinion);
	WrongAbilityRoot->RegisterComponent();
	WrongAbilityMinion->SetRootComponent(WrongAbilityRoot);
	WrongAbilityMinion->TickCheckDetection(FVector::ZeroVector); // Idle -> Alert
	WrongAbilityMinion->ReceiveControl(EAbilitySlot::Stun); // Alert -> Controlled, wrong ability
	WrongAbilityMinion->SetActorLocation(Boss->GetActorLocation());

	Boss->CheckShieldState();
	TestTrue(TEXT("Shield should stay up with only a wrong-ability-controlled minion nearby"), Boss->HasShield());
	TestEqual(TEXT("Boss should stay Armed with only a wrong-ability-controlled minion nearby"),
		static_cast<uint8>(Boss->GetBossState()), static_cast<uint8>(EBossState::Armed));

	// --- Scenario 3: a Sleep-controlled minion strictly outside the radius must
	// not drop the shield. ---
	AEnemyBaseTestActor* FarMinion = World->SpawnActor<AEnemyBaseTestActor>();
	if (!TestNotNull(TEXT("Far Sleep-controlled AEnemyBaseTestActor should spawn"), FarMinion))
	{
		return false;
	}
	USceneComponent* FarRoot = NewObject<USceneComponent>(FarMinion);
	FarRoot->RegisterComponent();
	FarMinion->SetRootComponent(FarRoot);
	FarMinion->TickCheckDetection(FVector::ZeroVector); // Idle -> Alert
	FarMinion->ReceiveControl(EAbilitySlot::Sleep); // Alert -> Controlled, right ability
	FarMinion->SetActorLocation(Boss->GetActorLocation() + FVector(Boss->ShieldDropRadiusUnits * 10.0f, 0.0f, 0.0f));

	Boss->CheckShieldState();
	TestTrue(TEXT("Shield should stay up with a Sleep-controlled minion outside the radius"), Boss->HasShield());
	TestEqual(TEXT("Boss should stay Armed with a Sleep-controlled minion outside the radius"),
		static_cast<uint8>(Boss->GetBossState()), static_cast<uint8>(EBossState::Armed));

	// --- Scenario 4: a Sleep-controlled minion exactly at the radius boundary
	// (inclusive <=) drops the shield, turns off the tell, and advances to
	// Vulnerable (AC #2, #5 second half). ---
	AEnemyBaseTestActor* BoundaryMinion = World->SpawnActor<AEnemyBaseTestActor>();
	if (!TestNotNull(TEXT("Boundary Sleep-controlled AEnemyBaseTestActor should spawn"), BoundaryMinion))
	{
		return false;
	}
	USceneComponent* BoundaryRoot = NewObject<USceneComponent>(BoundaryMinion);
	BoundaryRoot->RegisterComponent();
	BoundaryMinion->SetRootComponent(BoundaryRoot);
	BoundaryMinion->TickCheckDetection(FVector::ZeroVector); // Idle -> Alert
	BoundaryMinion->ReceiveControl(EAbilitySlot::Sleep); // Alert -> Controlled, right ability
	BoundaryMinion->SetActorLocation(Boss->GetActorLocation() + FVector(Boss->ShieldDropRadiusUnits, 0.0f, 0.0f));

	Boss->CheckShieldState();
	TestFalse(TEXT("Shield should drop with a Sleep-controlled minion exactly at the radius boundary"), Boss->HasShield());
	TestEqual(TEXT("Shield tell light should be off once the shield drops"),
		Boss->ShieldTellLightComponent->Intensity, 0.0f);
	TestEqual(TEXT("Boss should advance to Vulnerable once a qualifying minion is found"),
		static_cast<uint8>(Boss->GetBossState()), static_cast<uint8>(EBossState::Vulnerable));

	// --- Scenario 5: once the qualifying minion leaves, HasShield() freely
	// re-raises, but EBossState never reverts from Vulnerable (the plan's key
	// design decision). ---
	BoundaryMinion->SetActorLocation(Boss->GetActorLocation() + FVector(Boss->ShieldDropRadiusUnits * 10.0f, 0.0f, 0.0f));
	Boss->CheckShieldState();
	TestTrue(TEXT("Shield should re-raise once the qualifying minion leaves the radius"), Boss->HasShield());
	TestEqual(TEXT("Boss should remain Vulnerable even after the shield re-raises"),
		static_cast<uint8>(Boss->GetBossState()), static_cast<uint8>(EBossState::Vulnerable));

	// --- Scenario 6: a never-spawned-into-a-world instance must not crash. ---
	ASleepShieldBoss* UnspawnedBoss = NewObject<ASleepShieldBoss>();
	if (!TestNotNull(TEXT("NewObject<ASleepShieldBoss> should succeed"), UnspawnedBoss))
	{
		return false;
	}
	UnspawnedBoss->CheckShieldState();
	TestTrue(TEXT("A never-spawned boss should not crash when checked"), true);

	// --- Scenario 7: a minion whose Sleep control has naturally expired (reverted
	// Controlled -> Alert) must not count, even though GetControllingAbility() still
	// stales-reports Sleep (EnemyBase.h's documented "never reset on reversion"
	// contract) - proves HasNearbySleepControlledMinion()'s state check short-circuits
	// before the stale ability read (issue #138). ---
	AEnemyBaseTestActor* ExpiredMinion = World->SpawnActor<AEnemyBaseTestActor>();
	if (!TestNotNull(TEXT("Expired-control AEnemyBaseTestActor should spawn"), ExpiredMinion))
	{
		return false;
	}
	USceneComponent* ExpiredRoot = NewObject<USceneComponent>(ExpiredMinion);
	ExpiredRoot->RegisterComponent();
	ExpiredMinion->SetRootComponent(ExpiredRoot);
	ExpiredMinion->TickCheckDetection(FVector::ZeroVector); // Idle -> Alert
	ExpiredMinion->ReceiveControl(EAbilitySlot::Sleep); // Alert -> Controlled, right ability
	ExpiredMinion->SetActorLocation(Boss->GetActorLocation()); // well within radius
	ExpiredMinion->TickControlledDuration(9999.0f); // force Controlled -> Alert; ControllingAbility stays stale at Sleep

	const bool bShieldBeforeExpiredCheck = Boss->HasShield();
	Boss->CheckShieldState();
	TestEqual(TEXT("Shield state should be unaffected by a minion whose Sleep control has expired, despite a stale GetControllingAbility()"),
		Boss->HasShield(), bShieldBeforeExpiredCheck);

	// --- Scenario 8: a Banked boss must not re-toggle its shield even with a
	// qualifying minion in radius (SleepShieldBoss.cpp's documented Banked
	// early-return guard). ---
	ASleepShieldBoss* BankedBoss = World->SpawnActor<ASleepShieldBoss>();
	if (!TestNotNull(TEXT("Second ASleepShieldBoss for Banked-state coverage should spawn"), BankedBoss))
	{
		return false;
	}
	BankedBoss->DispatchBeginPlay();
	BankedBoss->AdvanceToVulnerable();
	BankedBoss->TransitionToBanked();
	const bool bShieldBeforeBankedCheck = BankedBoss->HasShield();

	AEnemyBaseTestActor* MinionNearBankedBoss = World->SpawnActor<AEnemyBaseTestActor>();
	if (!TestNotNull(TEXT("Minion near the Banked boss should spawn"), MinionNearBankedBoss))
	{
		return false;
	}
	USceneComponent* BankedMinionRoot = NewObject<USceneComponent>(MinionNearBankedBoss);
	BankedMinionRoot->RegisterComponent();
	MinionNearBankedBoss->SetRootComponent(BankedMinionRoot);
	MinionNearBankedBoss->TickCheckDetection(FVector::ZeroVector); // Idle -> Alert
	MinionNearBankedBoss->ReceiveControl(EAbilitySlot::Sleep); // Alert -> Controlled, right ability
	MinionNearBankedBoss->SetActorLocation(BankedBoss->GetActorLocation());

	BankedBoss->CheckShieldState();
	TestEqual(TEXT("HasShield should be unchanged once Banked, even with a qualifying minion nearby"),
		BankedBoss->HasShield(), bShieldBeforeBankedCheck);
	TestEqual(TEXT("Boss should remain Banked"),
		static_cast<uint8>(BankedBoss->GetBossState()), static_cast<uint8>(EBossState::Banked));

	// --- Scenario 9: Tick() itself (not just CheckShieldState() directly) must drive
	// the shield check - covers Tick()'s two-line body (Super::Tick + CheckShieldState)
	// which every other scenario bypasses by calling CheckShieldState() directly. ---
	ASleepShieldBoss* TickBoss = World->SpawnActor<ASleepShieldBoss>();
	if (!TestNotNull(TEXT("Third ASleepShieldBoss for Tick() coverage should spawn"), TickBoss))
	{
		return false;
	}
	TickBoss->DispatchBeginPlay();
	TestTrue(TEXT("TickBoss should have its shield up immediately after BeginPlay()"), TickBoss->HasShield());

	AEnemyBaseTestActor* MinionNearTickBoss = World->SpawnActor<AEnemyBaseTestActor>();
	if (!TestNotNull(TEXT("Minion near the Tick() coverage boss should spawn"), MinionNearTickBoss))
	{
		return false;
	}
	USceneComponent* TickMinionRoot = NewObject<USceneComponent>(MinionNearTickBoss);
	TickMinionRoot->RegisterComponent();
	MinionNearTickBoss->SetRootComponent(TickMinionRoot);
	MinionNearTickBoss->TickCheckDetection(FVector::ZeroVector); // Idle -> Alert
	MinionNearTickBoss->ReceiveControl(EAbilitySlot::Sleep); // Alert -> Controlled, right ability
	MinionNearTickBoss->SetActorLocation(TickBoss->GetActorLocation());

	TickBoss->Tick(0.0f);
	TestFalse(TEXT("Tick() should drop the shield via CheckShieldState() with a qualifying minion in radius"), TickBoss->HasShield());
	TestEqual(TEXT("Tick() should advance the boss to Vulnerable via CheckShieldState()"),
		static_cast<uint8>(TickBoss->GetBossState()), static_cast<uint8>(EBossState::Vulnerable));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
