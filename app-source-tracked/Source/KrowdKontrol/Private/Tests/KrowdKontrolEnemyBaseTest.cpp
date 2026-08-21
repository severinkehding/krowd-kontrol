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
#include "EnemyBaseNoTrimLightTestActor.h"
#include "EnemyBankedTestListener.h"
#include "EnemyControlledExpiredTestListener.h"
#include "PlayerEnergyComponent.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "ReservedGameplayColours.h"
#include "Components/PointLightComponent.h"
#include "AbilityData.h"

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

	// (v)-(y) Elite state (issue #19). Checked here (early, on freshly-constructed
	// actors) rather than at the end of this test after dozens more NewObject<>()
	// instances and several CreateNewMap() calls have run - a NewObject<>()-constructed
	// actor held only by a local pointer has no GC roots, and asserting on it this late
	// risked it having already been collected by an incidental GC pass triggered by the
	// later, heavier cases (reproduced empirically: intermittent null failures when
	// checked late - see app-changelog/issue-19.md's "Design note" and the same
	// rationale comment in KrowdKontrolBomberEnemyTest.cpp etc.).

	// (v) default Elite state: bIsElite false, effective speed equals the base
	// GetMovementSpeedUnitsPerSecond() unmultiplied, trim light off.
	AEnemyBaseTestActor* EliteDefaultEnemy = NewObject<AEnemyBaseTestActor>();
	TestFalse(TEXT("bIsElite should default to false"), EliteDefaultEnemy->bIsElite);
	TestEqual(TEXT("Default effective movement speed should equal the unmultiplied base speed"),
		EliteDefaultEnemy->GetEffectiveMovementSpeedUnitsPerSecond(), 600.0f);
	if (TestNotNull(TEXT("EliteTrimLightComponent should exist"), EliteDefaultEnemy->EliteTrimLightComponent.Get()))
	{
		TestEqual(TEXT("Elite trim light should be off by default"),
			EliteDefaultEnemy->EliteTrimLightComponent->Intensity, 0.0f);
	}

	// (w) SetIsElite(true) turns the trim light on and multiplies effective speed.
	AEnemyBaseTestActor* EliteEnemy = NewObject<AEnemyBaseTestActor>();
	EliteEnemy->SetIsElite(true);
	TestTrue(TEXT("SetIsElite(true) should set bIsElite"), EliteEnemy->bIsElite);
	TestEqual(TEXT("Elite trim light should turn on once bIsElite is true"),
		EliteEnemy->EliteTrimLightComponent->Intensity, EliteEnemy->EliteTrimIntensity);
	TestEqual(TEXT("Effective movement speed should be multiplied while Elite"),
		EliteEnemy->GetEffectiveMovementSpeedUnitsPerSecond(), 600.0f * EliteEnemy->EliteMovementSpeedMultiplier);

	// (x) SetIsElite(false) reverts both the flag and the light intensity.
	EliteEnemy->SetIsElite(false);
	TestFalse(TEXT("SetIsElite(false) should clear bIsElite"), EliteEnemy->bIsElite);
	TestEqual(TEXT("Elite trim light should turn back off once bIsElite is false"),
		EliteEnemy->EliteTrimLightComponent->Intensity, 0.0f);
	TestEqual(TEXT("Effective movement speed should revert to the unmultiplied base speed"),
		EliteEnemy->GetEffectiveMovementSpeedUnitsPerSecond(), 600.0f);

	// (y) Elite trim colour must not collide with any of the 5 reserved gameplay-
	// information colours.
	TestFalse(TEXT("EliteTrimLightComponent colour should not collide with a reserved colour"),
		ReservedGameplayColours::GetAll().ContainsByPredicate(
			[EliteEnemy](const FLinearColor& Reserved) { return Reserved.Equals(EliteEnemy->EliteTrimLightComponent->GetLightColor(), 0.01f); }));

	// (z) SetIsElite() on a subclass that doesn't override GetEliteTrimLightComponent()
	// (base default nullptr) must not crash, and must still flip bIsElite - the
	// trim-light half of SetIsElite() is null-guarded specifically so a future concrete
	// subclass that forgets the override doesn't dereference a null component.
	AEnemyBaseNoTrimLightTestActor* NoTrimLightEnemy = NewObject<AEnemyBaseNoTrimLightTestActor>();
	NoTrimLightEnemy->SetIsElite(true);
	TestTrue(TEXT("SetIsElite(true) should set bIsElite even with no trim light component"),
		NoTrimLightEnemy->bIsElite);

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
	TestEqual(TEXT("GetControllingAbility should report Sleep after ReceiveControl from Alert"),
		static_cast<uint8>(AlertControlled->GetControllingAbility()), static_cast<uint8>(EAbilitySlot::Sleep));

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
	TestEqual(TEXT("GetControllingAbility should report Root after ReceiveControl from Attack"),
		static_cast<uint8>(AttackControlled->GetControllingAbility()), static_cast<uint8>(EAbilitySlot::Root));

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

	// (i2) duration-expiry reversion (issue #138): a Controlled enemy whose duration
	// elapses before TransitionToBanked() is called reverts to Alert, not stuck, not
	// Banked. Also fires OnEnemyControlledExpired exactly once (issue #174).
	AEnemyBaseTestActor* ExpiryEnemy = NewObject<AEnemyBaseTestActor>();
	UEnemyControlledExpiredTestListener* ExpiryListener = NewObject<UEnemyControlledExpiredTestListener>();
	ExpiryEnemy->OnEnemyControlledExpired.AddDynamic(ExpiryListener, &UEnemyControlledExpiredTestListener::HandleEnemyControlledExpired);
	ExpiryEnemy->TickCheckDetection(ZeroDistanceLocation); // Idle -> Alert
	ExpiryEnemy->ReceiveControl(EAbilitySlot::Stun); // Alert -> Controlled, duration = AbilityData::Get(Stun).BaseDurationSeconds
	const float StunDurationSeconds = AbilityData::Get(EAbilitySlot::Stun).BaseDurationSeconds;
	ExpiryEnemy->TickControlledDuration(StunDurationSeconds - 1.0f);
	TestEqual(TEXT("Controlled state should persist before the duration elapses"),
		static_cast<uint8>(ExpiryEnemy->GetEnemyState()), static_cast<uint8>(EEnemyState::Controlled));
	TestEqual(TEXT("OnEnemyControlledExpired must not fire before the duration elapses"),
		ExpiryListener->CallCount, 0);
	ExpiryEnemy->TickControlledDuration(1.5f); // total elapsed now exceeds StunDurationSeconds
	TestEqual(TEXT("Controlled duration elapsing before banking should revert to Alert"),
		static_cast<uint8>(ExpiryEnemy->GetEnemyState()), static_cast<uint8>(EEnemyState::Alert));
	TestEqual(TEXT("OnEnemyControlledExpired should fire exactly once when the duration elapses"),
		ExpiryListener->CallCount, 1);

	// (i3) banking before expiry prevents the reversion - TickControlledDuration is
	// already guarded by CurrentState != Controlled, this proves it explicitly for the
	// new method. Also proves OnEnemyControlledExpired never fires on the
	// TransitionToBanked exit path (issue #174) - the two delegates are mutually
	// exclusive per state-exit.
	AEnemyBaseTestActor* BankedBeforeExpiryEnemy = NewObject<AEnemyBaseTestActor>();
	UEnemyControlledExpiredTestListener* BankedBeforeExpiryListener = NewObject<UEnemyControlledExpiredTestListener>();
	BankedBeforeExpiryEnemy->OnEnemyControlledExpired.AddDynamic(BankedBeforeExpiryListener, &UEnemyControlledExpiredTestListener::HandleEnemyControlledExpired);
	BankedBeforeExpiryEnemy->TickCheckDetection(ZeroDistanceLocation); // Idle -> Alert
	BankedBeforeExpiryEnemy->ReceiveControl(EAbilitySlot::Stun); // Alert -> Controlled
	BankedBeforeExpiryEnemy->TransitionToBanked(); // Controlled -> Banked
	BankedBeforeExpiryEnemy->TickControlledDuration(10000.0f);
	TestEqual(TEXT("TickControlledDuration after banking should not move the actor off Banked"),
		static_cast<uint8>(BankedBeforeExpiryEnemy->GetEnemyState()), static_cast<uint8>(EEnemyState::Banked));
	TestEqual(TEXT("OnEnemyControlledExpired must never fire on the TransitionToBanked exit path"),
		BankedBeforeExpiryListener->CallCount, 0);

	// (i4) duration-expiry reversion driven through the real Tick() override, not just
	// the friend-called TickControlledDuration helper directly (as (i2)/(i3) above do) -
	// proves the "not gated on a live player pawn" comment at EnemyBase.cpp:116-117
	// actually holds at the Tick() call site itself. No PlayerController/pawn spawned
	// in this World, matching case (k) below's same no-player-pawn setup.
	UWorld* ExpiryTickWorld = FAutomationEditorCommonUtils::CreateNewMap();
	if (TestNotNull(TEXT("CreateNewMap should return a valid World"), ExpiryTickWorld))
	{
		AEnemyBaseTestActor* TickedExpiryEnemy = ExpiryTickWorld->SpawnActor<AEnemyBaseTestActor>();
		if (TestNotNull(TEXT("AEnemyBaseTestActor should spawn into the test World"), TickedExpiryEnemy))
		{
			TickedExpiryEnemy->TickCheckDetection(ZeroDistanceLocation); // Idle -> Alert
			TickedExpiryEnemy->ReceiveControl(EAbilitySlot::Stun); // Alert -> Controlled
			const float TickedStunDurationSeconds = AbilityData::Get(EAbilitySlot::Stun).BaseDurationSeconds;
			for (float Elapsed = 0.0f; Elapsed < TickedStunDurationSeconds + 1.0f; Elapsed += 0.5f)
			{
				TickedExpiryEnemy->Tick(0.5f);
			}
			TestEqual(TEXT("Tick() alone, with no live player pawn, should still drive Controlled -> Alert reversion"),
				static_cast<uint8>(TickedExpiryEnemy->GetEnemyState()), static_cast<uint8>(EEnemyState::Alert));
		}
	}

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

	// (o) GetMovementSpeedUnitsPerSecond() base default matches the engine's own
	// MaxWalkSpeed default (600.0f) - the "normal" reference point BomberEnemy.h's
	// MovementSpeed comment cites.
	AEnemyBaseTestActor* SpeedEnemy = NewObject<AEnemyBaseTestActor>();
	TestEqual(TEXT("base default movement speed is 600.0f"),
		SpeedEnemy->GetMovementSpeedUnitsPerSecond(), 600.0f);

	// (p) TickChaseMovement is a no-op outside Alert: Idle.
	AEnemyBaseTestActor* IdleChaser = NewObject<AEnemyBaseTestActor>();
	const FVector StartLocation = IdleChaser->GetActorLocation();
	const FVector FarPlayerLocation(5000.0f, 0.0f, 0.0f);
	IdleChaser->TickChaseMovement(FarPlayerLocation, 1.0f);
	const float IdleDistanceMoved = FVector::Dist(IdleChaser->GetActorLocation(), StartLocation);
	TestEqual(TEXT("TickChaseMovement while Idle should not move the actor"),
		IdleDistanceMoved, 0.0f);

	// (q) TickChaseMovement moves the actor toward the player at the base default
	// speed while in Alert, advancing exactly speed*DeltaSeconds in one tick.
	AEnemyBaseTestActor* AlertChaser = NewObject<AEnemyBaseTestActor>();
	AlertChaser->TickCheckDetection(FVector(1000.0f, 0.0f, 0.0f)); // Idle -> Alert
	TestEqual(TEXT("precondition: chaser is Alert"),
		static_cast<uint8>(AlertChaser->GetEnemyState()), static_cast<uint8>(EEnemyState::Alert));
	const FVector BeforeChase = AlertChaser->GetActorLocation();
	AlertChaser->TickChaseMovement(FVector(1000.0f, 0.0f, 0.0f), 0.5f);
	const float DistanceMoved = FVector::Dist(AlertChaser->GetActorLocation(), BeforeChase);
	TestEqual(TEXT("Alert-state chase moves at base default speed * DeltaSeconds"),
		DistanceMoved, 600.0f * 0.5f);

	// (r) TickChaseMovement is a no-op outside Alert: Attack.
	AEnemyBaseTestActor* AttackChaser = NewObject<AEnemyBaseTestActor>();
	AttackChaser->TickCheckDetection(ZeroDistanceLocation); // Idle -> Alert
	AttackChaser->TickCheckDetection(ZeroDistanceLocation); // Alert -> Attack
	const FVector AttackStart = AttackChaser->GetActorLocation();
	AttackChaser->TickChaseMovement(FVector(5000.0f, 0.0f, 0.0f), 1.0f);
	const float AttackDistanceMoved = FVector::Dist(AttackChaser->GetActorLocation(), AttackStart);
	TestEqual(TEXT("TickChaseMovement while Attack should not move the actor"),
		AttackDistanceMoved, 0.0f);

	// (p2) TickChaseMovement is a no-op outside Alert: Controlled.
	AEnemyBaseTestActor* ControlledChaser = NewObject<AEnemyBaseTestActor>();
	ControlledChaser->TickCheckDetection(ZeroDistanceLocation); // Idle -> Alert
	ControlledChaser->TickCheckDetection(ZeroDistanceLocation); // Alert -> Attack
	ControlledChaser->ReceiveControl(EAbilitySlot::Stun); // Attack -> Controlled
	const FVector ControlledStart = ControlledChaser->GetActorLocation();
	ControlledChaser->TickChaseMovement(FVector(5000.0f, 0.0f, 0.0f), 1.0f);
	const float ControlledDistanceMoved = FVector::Dist(ControlledChaser->GetActorLocation(), ControlledStart);
	TestEqual(TEXT("TickChaseMovement while Controlled should not move the actor"),
		ControlledDistanceMoved, 0.0f);

	// (r2) TickChaseMovement is a no-op outside Alert: Banked.
	AEnemyBaseTestActor* BankedChaser = NewObject<AEnemyBaseTestActor>();
	BankedChaser->TickCheckDetection(ZeroDistanceLocation); // Idle -> Alert
	BankedChaser->TickCheckDetection(ZeroDistanceLocation); // Alert -> Attack
	BankedChaser->ReceiveControl(EAbilitySlot::Stun); // Attack -> Controlled
	BankedChaser->TransitionToBanked(); // Controlled -> Banked
	const FVector BankedStart = BankedChaser->GetActorLocation();
	BankedChaser->TickChaseMovement(FVector(5000.0f, 0.0f, 0.0f), 1.0f);
	const float BankedDistanceMoved = FVector::Dist(BankedChaser->GetActorLocation(), BankedStart);
	TestEqual(TEXT("TickChaseMovement while Banked should not move the actor"),
		BankedDistanceMoved, 0.0f);

	// (s) no-overshoot clamp: a player closer than speed*DeltaSeconds is reached
	// exactly, not overshot past.
	AEnemyBaseTestActor* CloseChaser = NewObject<AEnemyBaseTestActor>();
	CloseChaser->TickCheckDetection(FVector(50.0f, 0.0f, 0.0f)); // Idle -> Alert (within DetectionRangeUnits, outside base 0.0f attack range)
	CloseChaser->TickChaseMovement(FVector(50.0f, 0.0f, 0.0f), 1.0f); // would move 600 units at base speed - player is only 50 away
	const float ResidualDistance = FVector::Dist(CloseChaser->GetActorLocation(), FVector(50.0f, 0.0f, 0.0f));
	TestEqual(TEXT("chase clamps to the player's location instead of overshooting"),
		ResidualDistance, 0.0f);

	// (t) the real Tick() override wires TickChaseMovement into the per-frame loop,
	// same World-backed shape as case (k)'s detection-only Tick() coverage.
	// UGameplayStatics::GetPlayerPawn (what Tick() actually calls) resolves through
	// World::PlayerControllerList, which UWorld::AddController normally populates from
	// AController::PostInitializeComponents - but CreateNewMap()'s editor world is
	// never driven through World->InitializeActorsForPlay/BeginPlay (see EnemyBase.h's
	// "driven World->BeginPlay()" note and KrowdKontrolWaveSpawnerComponentTest.cpp
	// case (7)'s comment on the same limitation), so that registration never happens
	// for a plain SpawnActor<APlayerController>() here. AddController() is called
	// directly to register it - the same public entry point PostInitializeComponents
	// itself would have called, just invoked manually since this harness never drives
	// the World through the flow that calls it automatically. A raw APawn also has no
	// default RootComponent (unlike ACharacter/ADefaultPawn), so SetActorLocation()
	// on one is a silent no-op (Actor.cpp early-outs when RootComponent is null) -
	// same class of issue KrowdKontrolOvercrowdDetectionComponentTest.cpp's
	// AEnemyBaseTestActor comment documents; give it a scene root for the same reason.
	UWorld* ChaseWorld = FAutomationEditorCommonUtils::CreateNewMap();
	if (TestNotNull(TEXT("CreateNewMap should return a valid World"), ChaseWorld))
	{
		AEnemyBaseTestActor* TickedChaser = ChaseWorld->SpawnActor<AEnemyBaseTestActor>();
		APawn* ChasePlayerPawn = ChaseWorld->SpawnActor<APawn>();
		if (TestNotNull(TEXT("AEnemyBaseTestActor should spawn into the test World"), TickedChaser)
			&& TestNotNull(TEXT("APawn should spawn into the test World"), ChasePlayerPawn))
		{
			APlayerController* ChaseController = ChaseWorld->SpawnActor<APlayerController>();
			if (!TestNotNull(TEXT("Should be able to spawn a controller to possess the pawn"), ChaseController))
			{
				return false;
			}
			ChaseController->Possess(ChasePlayerPawn);
			ChaseWorld->AddController(ChaseController);

			USceneComponent* ChasePlayerPawnRoot = NewObject<USceneComponent>(ChasePlayerPawn);
			ChasePlayerPawnRoot->RegisterComponent();
			ChasePlayerPawn->SetRootComponent(ChasePlayerPawnRoot);
			ChasePlayerPawn->SetActorLocation(FVector(1000.0f, 0.0f, 0.0f));

			TickedChaser->TickCheckDetection(ChasePlayerPawn->GetActorLocation()); // Idle -> Alert
			const FVector BeforeTick = TickedChaser->GetActorLocation();
			TickedChaser->Tick(0.1f);
			const float TickDistanceMoved = FVector::Dist(TickedChaser->GetActorLocation(), BeforeTick);
			TestEqual(TEXT("Tick() should move the chaser toward the player at base default speed * DeltaSeconds"),
				TickDistanceMoved, 600.0f * 0.1f);
		}
	}

	// (u) same-tick transition-and-move: an Idle actor whose Tick() call newly
	// detects the player must both flip to Alert AND advance toward them within that
	// one call - the exact same-frame transition+move interaction scope.md's review
	// focus flagged (TickCheckDetection runs before TickChaseMovement, same tick).
	UWorld* SameTickWorld = FAutomationEditorCommonUtils::CreateNewMap();
	if (TestNotNull(TEXT("CreateNewMap should return a valid World"), SameTickWorld))
	{
		AEnemyBaseTestActor* FreshChaser = SameTickWorld->SpawnActor<AEnemyBaseTestActor>();
		APawn* FreshPlayerPawn = SameTickWorld->SpawnActor<APawn>();
		if (TestNotNull(TEXT("AEnemyBaseTestActor should spawn into the test World"), FreshChaser)
			&& TestNotNull(TEXT("APawn should spawn into the test World"), FreshPlayerPawn))
		{
			APlayerController* SameTickController = SameTickWorld->SpawnActor<APlayerController>();
			if (!TestNotNull(TEXT("Should be able to spawn a controller to possess the pawn"), SameTickController))
			{
				return false;
			}
			SameTickController->Possess(FreshPlayerPawn);
			SameTickWorld->AddController(SameTickController);

			USceneComponent* FreshPlayerPawnRoot = NewObject<USceneComponent>(FreshPlayerPawn);
			FreshPlayerPawnRoot->RegisterComponent();
			FreshPlayerPawn->SetRootComponent(FreshPlayerPawnRoot);
			FreshPlayerPawn->SetActorLocation(FVector(1000.0f, 0.0f, 0.0f));
			TestEqual(TEXT("precondition: chaser starts Idle"),
				static_cast<uint8>(FreshChaser->GetEnemyState()), static_cast<uint8>(EEnemyState::Idle));
			const FVector BeforeFirstTick = FreshChaser->GetActorLocation();

			FreshChaser->Tick(0.1f); // single call: detection AND chase movement

			TestEqual(TEXT("single Tick() call also flips state to Alert"),
				static_cast<uint8>(FreshChaser->GetEnemyState()), static_cast<uint8>(EEnemyState::Alert));
			TestTrue(TEXT("same Tick() call also moves the newly-alerted chaser"),
				FVector::Dist(FreshChaser->GetActorLocation(), BeforeFirstTick) > 0.0f);
		}
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
