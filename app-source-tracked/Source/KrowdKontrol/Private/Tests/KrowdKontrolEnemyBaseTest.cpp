// Confirms AEnemyBase (issue #12, PRD 03) guarantees: (1) the state machine cannot
// reach a "defeated" state other than Banked - no path skips a state, no path reaches
// Banked from anywhere but Controlled, and nothing ever leaves Banked once reached -
// (2) proximity-driven Idle->Alert->Attack transitions occur correctly in isolation,
// one transition per TickCheckDetection call, and (3) ReceiveControl works from both
// Alert and Attack, recording the ability that triggered it.
//
// Uses NewObject rather than spawning into a UWorld for most cases: AEnemyBase never
// calls GetWorld()/SpawnActor in its testable paths (Tick() does, but
// TickCheckDetection is called directly via the friend, with an explicit FVector
// player location, never through Tick() itself), same rationale
// KrowdKontrolBossBaseTest.cpp documents. Cases (k)/(m)/(n) are the exceptions: they
// exercise the real Tick() override and FindPlayerEnergyComponent()'s
// TActorIterator/GetWorld() usage respectively, both of which need a real World.
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the other
// KrowdKontrol.Unit.* tests.

#include "Misc/AutomationTest.h"
#include "EnemyBase.h"
#include "EnemyBaseTestActor.h"
#include "EnemyBankedTestListener.h"
#include "PlayerEnergyComponent.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolEnemyBaseTest,
	"KrowdKontrol.Unit.EnemyBase",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolEnemyBaseTest::RunTest(const FString& Parameters)
{
	// (a) default state.
	AEnemyBaseTestActor* Enemy = NewObject<AEnemyBaseTestActor>();
	if (!TestNotNull(TEXT("AEnemyBaseTestActor should construct"), Enemy))
	{
		return false;
	}
	TestEqual(TEXT("Default enemy state should be Idle"),
		static_cast<uint8>(Enemy->GetEnemyState()), static_cast<uint8>(EEnemyState::Idle));

	// (b) far outside DetectionRangeUnits leaves state at Idle.
	const FVector FarAwayLocation(Enemy->DetectionRangeUnits + 500.0f, 0.0f, 0.0f);
	Enemy->TickCheckDetection(FarAwayLocation);
	TestEqual(TEXT("TickCheckDetection outside DetectionRangeUnits should leave state at Idle"),
		static_cast<uint8>(Enemy->GetEnemyState()), static_cast<uint8>(EEnemyState::Idle));

	// (c) inside DetectionRangeUnits advances Idle -> Alert.
	const FVector NearLocation(100.0f, 0.0f, 0.0f);
	Enemy->TickCheckDetection(NearLocation);
	TestEqual(TEXT("TickCheckDetection inside DetectionRangeUnits should advance Idle to Alert"),
		static_cast<uint8>(Enemy->GetEnemyState()), static_cast<uint8>(EEnemyState::Alert));

	// (d) a second call, still within DetectionRangeUnits but outside the base's
	// default 0.0f GetAttackRangeUnits(), stays at Alert - proves the else-if's
	// single-transition-per-call guard and that GetAttackRangeUnits()'s default
	// genuinely gates the transition.
	Enemy->TickCheckDetection(NearLocation);
	TestEqual(TEXT("TickCheckDetection outside GetAttackRangeUnits() should leave state at Alert"),
		static_cast<uint8>(Enemy->GetEnemyState()), static_cast<uint8>(EEnemyState::Alert));

	// (e) a fresh actor at exactly 0.0f distance (within the base's 0.0f default
	// GetAttackRangeUnits(), satisfying Distance <= GetAttackRangeUnits() at the
	// boundary) advances Idle -> Alert -> Attack across two calls.
	AEnemyBaseTestActor* BoundaryEnemy = NewObject<AEnemyBaseTestActor>();
	const FVector ZeroDistanceLocation(0.0f, 0.0f, 0.0f);
	BoundaryEnemy->TickCheckDetection(ZeroDistanceLocation);
	TestEqual(TEXT("Zero-distance TickCheckDetection should advance Idle to Alert"),
		static_cast<uint8>(BoundaryEnemy->GetEnemyState()), static_cast<uint8>(EEnemyState::Alert));
	BoundaryEnemy->TickCheckDetection(ZeroDistanceLocation);
	TestEqual(TEXT("Zero-distance TickCheckDetection should advance Alert to Attack at the boundary"),
		static_cast<uint8>(BoundaryEnemy->GetEnemyState()), static_cast<uint8>(EEnemyState::Attack));
	TestEqual(TEXT("OnAttackEntry should have fired exactly once"), BoundaryEnemy->AttackEntryCallCount, 1);

	// (f) no-skip guards: ReceiveControl/TransitionToBanked are no-ops from every
	// predecessor state that isn't the exact one their guard requires.
	AEnemyBaseTestActor* GuardEnemy = NewObject<AEnemyBaseTestActor>();
	GuardEnemy->ReceiveControl(EAbilitySlot::Sleep);
	TestEqual(TEXT("ReceiveControl from Idle should be a no-op"),
		static_cast<uint8>(GuardEnemy->GetEnemyState()), static_cast<uint8>(EEnemyState::Idle));
	TestEqual(TEXT("OnControlledEntry should not have fired"), GuardEnemy->ControlledEntryCallCount, 0);
	GuardEnemy->TransitionToBanked();
	TestEqual(TEXT("TransitionToBanked from Idle should be a no-op"),
		static_cast<uint8>(GuardEnemy->GetEnemyState()), static_cast<uint8>(EEnemyState::Idle));

	GuardEnemy->TickCheckDetection(ZeroDistanceLocation); // Idle -> Alert
	GuardEnemy->TransitionToBanked();
	TestEqual(TEXT("TransitionToBanked from Alert should be a no-op"),
		static_cast<uint8>(GuardEnemy->GetEnemyState()), static_cast<uint8>(EEnemyState::Alert));

	GuardEnemy->TickCheckDetection(ZeroDistanceLocation); // Alert -> Attack
	GuardEnemy->TransitionToBanked();
	TestEqual(TEXT("TransitionToBanked from Attack should be a no-op"),
		static_cast<uint8>(GuardEnemy->GetEnemyState()), static_cast<uint8>(EEnemyState::Attack));

	// (g) valid progression: ReceiveControl works from Alert AND from Attack, each
	// recording the specific ability that triggered it.
	AEnemyBaseTestActor* AlertControlled = NewObject<AEnemyBaseTestActor>();
	AlertControlled->TickCheckDetection(ZeroDistanceLocation); // Idle -> Alert
	AlertControlled->ReceiveControl(EAbilitySlot::Sleep);
	TestEqual(TEXT("ReceiveControl from Alert should move to Controlled"),
		static_cast<uint8>(AlertControlled->GetEnemyState()), static_cast<uint8>(EEnemyState::Controlled));
	TestEqual(TEXT("OnControlledEntry should have fired exactly once"),
		AlertControlled->ControlledEntryCallCount, 1);
	TestEqual(TEXT("LastControlledEntryAbility should record Sleep"),
		static_cast<uint8>(AlertControlled->LastControlledEntryAbility), static_cast<uint8>(EAbilitySlot::Sleep));

	AEnemyBaseTestActor* AttackControlled = NewObject<AEnemyBaseTestActor>();
	AttackControlled->TickCheckDetection(ZeroDistanceLocation); // Idle -> Alert
	AttackControlled->TickCheckDetection(ZeroDistanceLocation); // Alert -> Attack
	AttackControlled->ReceiveControl(EAbilitySlot::Root);
	TestEqual(TEXT("ReceiveControl from Attack should move to Controlled"),
		static_cast<uint8>(AttackControlled->GetEnemyState()), static_cast<uint8>(EEnemyState::Controlled));
	TestEqual(TEXT("OnControlledEntry should have fired exactly once"),
		AttackControlled->ControlledEntryCallCount, 1);
	TestEqual(TEXT("LastControlledEntryAbility should record Root"),
		static_cast<uint8>(AttackControlled->LastControlledEntryAbility), static_cast<uint8>(EAbilitySlot::Root));

	// (h) TransitionToBanked from Controlled moves to Banked and fires OnEnemyBanked
	// exactly once.
	UEnemyBankedTestListener* Listener = NewObject<UEnemyBankedTestListener>();
	AlertControlled->OnEnemyBanked.AddDynamic(Listener, &UEnemyBankedTestListener::HandleEnemyBanked);
	AlertControlled->TransitionToBanked();
	TestEqual(TEXT("TransitionToBanked from Controlled should move to Banked"),
		static_cast<uint8>(AlertControlled->GetEnemyState()), static_cast<uint8>(EEnemyState::Banked));
	TestEqual(TEXT("OnEnemyBanked should have fired exactly once"), Listener->CallCount, 1);

	// (i) terminal/idempotent: once Banked, nothing moves it anywhere else, and the
	// delegate never re-fires.
	AlertControlled->ReceiveControl(EAbilitySlot::Stun);
	TestEqual(TEXT("ReceiveControl after Banked should be a no-op"),
		static_cast<uint8>(AlertControlled->GetEnemyState()), static_cast<uint8>(EEnemyState::Banked));
	TestEqual(TEXT("OnControlledEntry should still have fired exactly once"),
		AlertControlled->ControlledEntryCallCount, 1);
	AlertControlled->TransitionToBanked();
	TestEqual(TEXT("TransitionToBanked after Banked should be a no-op"),
		static_cast<uint8>(AlertControlled->GetEnemyState()), static_cast<uint8>(EEnemyState::Banked));
	TestEqual(TEXT("OnEnemyBanked should still have fired exactly once"), Listener->CallCount, 1);

	// (j) actor is pacified, not destroyed - no code path in AEnemyBase ever calls
	// Destroy(). Directly enforces MISSION.md Hard Invariant 2 (no enemy is ever
	// killed).
	TestFalse(TEXT("Enemy actor should not be destroyed by reaching Banked"),
		AlertControlled->IsActorBeingDestroyed());

	// (k) the real Tick() override, not just the friend-called TickCheckDetection
	// helper, must not crash when GetPlayerPawn returns nullptr (a headless test map
	// with no PlayerController spawned) - proves the null-pawn guard actually wires
	// into the per-frame loop, not just that it reads correctly in isolation.
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	if (TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
	{
		AEnemyBaseTestActor* TickedEnemy = World->SpawnActor<AEnemyBaseTestActor>();
		if (TestNotNull(TEXT("AEnemyBaseTestActor should spawn into the test World"), TickedEnemy))
		{
			// No PlayerController/pawn spawned - exercises the GetPlayerPawn-returns-
			// nullptr branch instead of a real detection check.
			TickedEnemy->Tick(0.1f);
			TestEqual(TEXT("Tick() with no player pawn present should leave state at Idle"),
				static_cast<uint8>(TickedEnemy->GetEnemyState()), static_cast<uint8>(EEnemyState::Idle));
		}
	}

	// (l) TransitionToBanked()'s flip-before-broadcast ordering must be re-entrancy
	// safe: a listener that re-enters TransitionToBanked() on the same actor from
	// inside OnEnemyBanked must not cause a double-fire. Mirrors
	// KrowdKontrolBossBaseTest.cpp case (h)'s UBossBankedTestListener coverage of
	// the same pattern on ABossBase::TransitionToBanked().
	AEnemyBaseTestActor* ReentrantEnemy = NewObject<AEnemyBaseTestActor>();
	ReentrantEnemy->TickCheckDetection(ZeroDistanceLocation); // Idle -> Alert
	ReentrantEnemy->ReceiveControl(EAbilitySlot::Stun); // Alert -> Controlled

	UEnemyBankedTestListener* ReentrantListener = NewObject<UEnemyBankedTestListener>();
	ReentrantListener->ActorToReenter = ReentrantEnemy;
	ReentrantEnemy->OnEnemyBanked.AddDynamic(ReentrantListener, &UEnemyBankedTestListener::HandleEnemyBanked);

	ReentrantEnemy->TransitionToBanked();
	TestEqual(TEXT("Re-entrant TransitionToBanked() during broadcast must not re-fire"),
		ReentrantListener->CallCount, 1);

	// (m) FindPlayerEnergyComponent finds the world's UPlayerEnergyComponent when a
	// pawn carries one. Shared with issue #15's Bomber-enemy work-in-progress (see
	// app-changelog/issue-25.md's "Deviations from plan"); tested here since this is
	// the only AEnemyBase capability in this PR's tracked diff with no prior coverage.
	UWorld* EnergyWorld = FAutomationEditorCommonUtils::CreateNewMap();
	if (TestNotNull(TEXT("CreateNewMap should return a valid World"), EnergyWorld))
	{
		AEnemyBaseTestActor* EnergyEnemy = EnergyWorld->SpawnActor<AEnemyBaseTestActor>();
		APawn* PlayerPawn = EnergyWorld->SpawnActor<APawn>();
		if (TestNotNull(TEXT("AEnemyBaseTestActor should spawn into the test World"), EnergyEnemy)
			&& TestNotNull(TEXT("APawn should spawn into the test World"), PlayerPawn))
		{
			UPlayerEnergyComponent* Energy = NewObject<UPlayerEnergyComponent>(PlayerPawn);
			Energy->RegisterComponent();

			TestEqual(TEXT("FindPlayerEnergyComponent should find the pawn's UPlayerEnergyComponent"),
				EnergyEnemy->FindPlayerEnergyComponent(), Energy);
		}
	}

	// (n) not-found path: no pawn with the component anywhere in the world returns
	// nullptr (and logs a warning) rather than crashing.
	UWorld* EmptyWorld = FAutomationEditorCommonUtils::CreateNewMap();
	if (TestNotNull(TEXT("CreateNewMap should return a valid World"), EmptyWorld))
	{
		AEnemyBaseTestActor* LonelyEnemy = EmptyWorld->SpawnActor<AEnemyBaseTestActor>();
		if (TestNotNull(TEXT("AEnemyBaseTestActor should spawn into the test World"), LonelyEnemy))
		{
			AddExpectedError(TEXT("found no APawn with a UPlayerEnergyComponent"), EAutomationExpectedErrorFlags::Contains, 1);
			TestNull(TEXT("FindPlayerEnergyComponent should return nullptr when no pawn carries the component"),
				LonelyEnemy->FindPlayerEnergyComponent());
		}
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
