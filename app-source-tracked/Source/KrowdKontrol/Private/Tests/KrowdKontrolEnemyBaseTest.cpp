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
	TestEqual(TEXT("Default RemainingControlledSeconds should be 0.0 before any Controlled entry"),
		Enemy->GetRemainingControlledSeconds(), 0.0f);
	TestEqual(TEXT("Default TotalControlledSeconds should be 0.0 before any Controlled entry"),
		Enemy->GetTotalControlledSeconds(), 0.0f);

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

	// (g2) issue #224: GetRemainingControlledSeconds()/GetTotalControlledSeconds()
	// read as a 1.0 fraction immediately on entering Controlled (no override
	// applies to AEnemyBaseTestActor - base -1.0f "no override" default), then
	// decrease monotonically as TickControlledDuration advances simulated time.
	AEnemyBaseTestActor* DurationEnemy = NewObject<AEnemyBaseTestActor>();
	DurationEnemy->TickCheckDetection(ZeroDistanceLocation); // Idle -> Alert
	DurationEnemy->ReceiveControl(EAbilitySlot::Stun); // Alert -> Controlled
	const float StunBaseDurationSeconds = AbilityData::Get(EAbilitySlot::Stun).BaseDurationSeconds;
	TestEqual(TEXT("GetTotalControlledSeconds should equal the base Stun duration with no override"),
		DurationEnemy->GetTotalControlledSeconds(), StunBaseDurationSeconds);
	TestEqual(TEXT("Remaining/Total fraction should be 1.0 immediately on entering Controlled"),
		DurationEnemy->GetRemainingControlledSeconds() / DurationEnemy->GetTotalControlledSeconds(), 1.0f);

	float PreviousFraction = 1.0f;
	const float DurationStepSeconds = StunBaseDurationSeconds / 4.0f;
	for (int32 Step = 0; Step < 3; ++Step)
	{
		DurationEnemy->TickControlledDuration(DurationStepSeconds);
		const float CurrentFraction = DurationEnemy->GetRemainingControlledSeconds() / DurationEnemy->GetTotalControlledSeconds();
		TestTrue(TEXT("Remaining/Total fraction should decrease monotonically as the duration advances"),
			CurrentFraction < PreviousFraction);
		PreviousFraction = CurrentFraction;
	}

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

	// (i1b) issue #224: stale-read contract on the Controlled -> Banked edge -
	// reading the duration accessors after TransitionToBanked() does not crash and
	// matches the same "stale read, guarded by state" contract
	// GetControllingAbility() documents. AlertControlled was never ticked, so
	// Remaining still equals Total in full; two consecutive reads returning the
	// same value demonstrate the read itself does not mutate state.
	TestEqual(TEXT("GetTotalControlledSeconds should reflect Sleep's base duration"),
		AlertControlled->GetTotalControlledSeconds(), AbilityData::Get(EAbilitySlot::Sleep).BaseDurationSeconds);
	TestEqual(TEXT("GetRemainingControlledSeconds should still equal the full duration after banking (no ticks elapsed)"),
		AlertControlled->GetRemainingControlledSeconds(), AlertControlled->GetTotalControlledSeconds());
	TestEqual(TEXT("Reading GetRemainingControlledSeconds again after banking should return the same stable value"),
		AlertControlled->GetRemainingControlledSeconds(), AlertControlled->GetTotalControlledSeconds());

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

	// (i2b) issue #224: stale-read contract after expiry - same "stale read,
	// guarded by state" contract GetControllingAbility() already documents.
	// Reading the duration accessors after OnEnemyControlledExpired does not crash,
	// Remaining reads 0.0 (TickControlledDuration clamps to zero before reverting),
	// Total retains its last-known value, and repeated reads are stable.
	TestEqual(TEXT("GetRemainingControlledSeconds should read 0.0 immediately after the duration expires"),
		ExpiryEnemy->GetRemainingControlledSeconds(), 0.0f);
	TestEqual(TEXT("GetTotalControlledSeconds should retain its last-known value after expiry"),
		ExpiryEnemy->GetTotalControlledSeconds(), StunDurationSeconds);
	ExpiryEnemy->TickControlledDuration(5.0f); // no-op: CurrentState is Alert, not Controlled
	TestEqual(TEXT("GetRemainingControlledSeconds should remain stable on repeated reads after expiry"),
		ExpiryEnemy->GetRemainingControlledSeconds(), 0.0f);
	TestEqual(TEXT("GetTotalControlledSeconds should remain stable on repeated reads after expiry"),
		ExpiryEnemy->GetTotalControlledSeconds(), StunDurationSeconds);

	// (i2c) issue #224: TotalControlledSeconds must recompute on a second
	// ReceiveControl() call, not remain stuck on the first Controlled window's value -
	// the actual repeat-CC gameplay scenario (stunned, breaks free, rooted again).
	ExpiryEnemy->ReceiveControl(EAbilitySlot::Root); // Alert -> Controlled again, different ability
	const float RootDurationSeconds = AbilityData::Get(EAbilitySlot::Root).BaseDurationSeconds;
	TestEqual(TEXT("GetTotalControlledSeconds should recompute to the new ability's duration on re-entry"),
		ExpiryEnemy->GetTotalControlledSeconds(), RootDurationSeconds);
	TestEqual(TEXT("GetRemainingControlledSeconds should reset to the new total on re-entry"),
		ExpiryEnemy->GetRemainingControlledSeconds(), ExpiryEnemy->GetTotalControlledSeconds());

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

	// (j2) Sleep-flavour early wake (issue #257): being hit by a DIFFERENT ability
	// while still Controlled by Sleep (whose AbilityData flags
	// bWakesEarlyOnOtherAbilityHit) ends the Controlled window immediately, before its
	// full duration would naturally have elapsed - the same Controlled->Alert edge
	// (i2) exercises for natural expiry, just triggered early. Placed here, after (j)'s
	// check on the long-lived un-rooted AlertControlled actor rather than beside
	// (i2)/(i2c), for the same "no GC roots" reason the (v)-(y) Elite cases document at
	// the top of this file - keeping new un-rooted NewObject<>() allocations out of the
	// stretch AlertControlled must survive avoids a repeat of that same incidental-GC
	// risk.
	AEnemyBaseTestActor* WakeEarlyEnemy = NewObject<AEnemyBaseTestActor>();
	UEnemyControlledExpiredTestListener* WakeEarlyListener = NewObject<UEnemyControlledExpiredTestListener>();
	WakeEarlyEnemy->OnEnemyControlledExpired.AddDynamic(WakeEarlyListener, &UEnemyControlledExpiredTestListener::HandleEnemyControlledExpired);
	WakeEarlyEnemy->TickCheckDetection(ZeroDistanceLocation); // Idle -> Alert
	WakeEarlyEnemy->ReceiveControl(EAbilitySlot::Sleep); // Alert -> Controlled by Sleep
	TestEqual(TEXT("WakeEarlyEnemy should be Controlled after the initial Sleep cast"),
		static_cast<uint8>(WakeEarlyEnemy->GetEnemyState()), static_cast<uint8>(EEnemyState::Controlled));
	WakeEarlyEnemy->ReceiveControl(EAbilitySlot::Root); // different ability while still Controlled by Sleep
	TestEqual(TEXT("A Sleep-Controlled enemy hit by a different ability should wake to Alert, not stay Controlled or reach Banked"),
		static_cast<uint8>(WakeEarlyEnemy->GetEnemyState()), static_cast<uint8>(EEnemyState::Alert));
	TestEqual(TEXT("OnEnemyControlledExpired should fire exactly once on an early wake"),
		WakeEarlyListener->CallCount, 1);

	// (j3) Re-casting the SAME ability that's already controlling the enemy is still
	// a no-op, matching the base "no-op unless Alert/Attack" contract - there is no
	// real gameplay path today that re-casts an ability on its own already-Controlled
	// target, and the early-wake branch must not treat Ability == ControllingAbility
	// as a wake trigger.
	AEnemyBaseTestActor* SameAbilityRecastEnemy = NewObject<AEnemyBaseTestActor>();
	UEnemyControlledExpiredTestListener* SameAbilityRecastListener = NewObject<UEnemyControlledExpiredTestListener>();
	SameAbilityRecastEnemy->OnEnemyControlledExpired.AddDynamic(SameAbilityRecastListener, &UEnemyControlledExpiredTestListener::HandleEnemyControlledExpired);
	SameAbilityRecastEnemy->TickCheckDetection(ZeroDistanceLocation); // Idle -> Alert
	SameAbilityRecastEnemy->ReceiveControl(EAbilitySlot::Sleep); // Alert -> Controlled by Sleep
	SameAbilityRecastEnemy->ReceiveControl(EAbilitySlot::Sleep); // same ability again while still Controlled
	TestEqual(TEXT("Re-casting the same ability on an already-Controlled enemy should remain Controlled"),
		static_cast<uint8>(SameAbilityRecastEnemy->GetEnemyState()), static_cast<uint8>(EEnemyState::Controlled));
	TestEqual(TEXT("OnEnemyControlledExpired must not fire when the re-cast ability matches ControllingAbility"),
		SameAbilityRecastListener->CallCount, 0);

	// (j4) An enemy Controlled by a non-wake-flagged ability (e.g. Root, whose
	// AbilityData defaults bWakesEarlyOnOtherAbilityHit to false) hit by a DIFFERENT
	// ability is unaffected - confirms the bWakesEarlyOnOtherAbilityHit gate itself,
	// not just "any second hit on a Controlled enemy wakes it."
	AEnemyBaseTestActor* NonWakeFlaggedEnemy = NewObject<AEnemyBaseTestActor>();
	UEnemyControlledExpiredTestListener* NonWakeFlaggedListener = NewObject<UEnemyControlledExpiredTestListener>();
	NonWakeFlaggedEnemy->OnEnemyControlledExpired.AddDynamic(NonWakeFlaggedListener, &UEnemyControlledExpiredTestListener::HandleEnemyControlledExpired);
	NonWakeFlaggedEnemy->TickCheckDetection(ZeroDistanceLocation); // Idle -> Alert
	NonWakeFlaggedEnemy->ReceiveControl(EAbilitySlot::Root); // Alert -> Controlled by Root (not wake-flagged)
	NonWakeFlaggedEnemy->ReceiveControl(EAbilitySlot::Sleep); // different ability while still Controlled by Root
	TestEqual(TEXT("A Root-Controlled enemy hit by a different ability should remain Controlled (Root is not wake-flagged)"),
		static_cast<uint8>(NonWakeFlaggedEnemy->GetEnemyState()), static_cast<uint8>(EEnemyState::Controlled));
	TestEqual(TEXT("OnEnemyControlledExpired must not fire for a non-wake-flagged ControllingAbility"),
		NonWakeFlaggedListener->CallCount, 0);

	// (j5) issue #257 Acceptance Criteria: an uninterrupted Sleep-Controlled enemy
	// (no other ability cast while Controlled, so the new (j2) early-wake path never
	// triggers) still reaches Banked normally through TransitionToBanked(), same as
	// (h) does for a generic Controlled enemy - proving the early-wake addition did
	// not regress the ordinary banking/herd-chain path for Sleep specifically.
	// OnEnemyControlledExpired must never fire on this path (mirrors (i3)'s
	// "TransitionToBanked exit path never fires OnEnemyControlledExpired" contract),
	// confirming banking, not an early wake, is what ended the Controlled window.
	AEnemyBaseTestActor* UninterruptedSleepEnemy = NewObject<AEnemyBaseTestActor>();
	UEnemyBankedTestListener* UninterruptedSleepBankedListener = NewObject<UEnemyBankedTestListener>();
	UEnemyControlledExpiredTestListener* UninterruptedSleepExpiredListener = NewObject<UEnemyControlledExpiredTestListener>();
	UninterruptedSleepEnemy->OnEnemyBanked.AddDynamic(UninterruptedSleepBankedListener, &UEnemyBankedTestListener::HandleEnemyBanked);
	UninterruptedSleepEnemy->OnEnemyControlledExpired.AddDynamic(UninterruptedSleepExpiredListener, &UEnemyControlledExpiredTestListener::HandleEnemyControlledExpired);
	UninterruptedSleepEnemy->TickCheckDetection(ZeroDistanceLocation); // Idle -> Alert
	UninterruptedSleepEnemy->ReceiveControl(EAbilitySlot::Sleep); // Alert -> Controlled by Sleep, no other ability cast
	TestEqual(TEXT("GetControllingAbility should report Sleep with no interrupting cast"),
		static_cast<uint8>(UninterruptedSleepEnemy->GetControllingAbility()), static_cast<uint8>(EAbilitySlot::Sleep));
	UninterruptedSleepEnemy->TransitionToBanked();
	TestEqual(TEXT("An uninterrupted Sleep-Controlled enemy should reach Banked via TransitionToBanked"),
		static_cast<uint8>(UninterruptedSleepEnemy->GetEnemyState()), static_cast<uint8>(EEnemyState::Banked));
	TestEqual(TEXT("OnEnemyBanked should have fired exactly once for the uninterrupted Sleep enemy"),
		UninterruptedSleepBankedListener->CallCount, 1);
	TestEqual(TEXT("OnEnemyControlledExpired must never fire when an uninterrupted Sleep enemy is banked"),
		UninterruptedSleepExpiredListener->CallCount, 0);

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

	// (p3) TickChaseMovement while Controlled by Snare: unlike Stun above (p2), Snare's
	// bAllowsMovementWhileControlled=true lets chase continue at ControlledSpeedMultiplier
	// (0.5) - issue #254's "partial slow" claim, proven by actual distance moved.
	AEnemyBaseTestActor* SnaredChaser = NewObject<AEnemyBaseTestActor>();
	SnaredChaser->TickCheckDetection(FVector(1000.0f, 0.0f, 0.0f)); // Idle -> Alert (within DetectionRangeUnits, unlike (p)'s FarPlayerLocation)
	SnaredChaser->ReceiveControl(EAbilitySlot::Snare); // Alert -> Controlled
	const FVector SnaredStart = SnaredChaser->GetActorLocation();
	SnaredChaser->TickChaseMovement(FVector(1000.0f, 0.0f, 0.0f), 1.0f);
	const float SnaredDistanceMoved = FVector::Dist(SnaredChaser->GetActorLocation(), SnaredStart);
	TestEqual(TEXT("(p3) A Snared enemy should move at half its normal speed, not freeze"),
		SnaredDistanceMoved, SnaredChaser->GetEffectiveMovementSpeedUnitsPerSecond() * 0.5f);

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

	// (v-fear) TickFleeMovement (issue #253) moves a Fear-Controlled enemy away
	// from CasterLocation at the base default speed, advancing exactly
	// speed*DeltaSeconds in one tick - mirrors (q)'s toward-player shape, inverted.
	AEnemyBaseTestActor* FearedFleer = NewObject<AEnemyBaseTestActor>();
	FearedFleer->TickCheckDetection(ZeroDistanceLocation); // Idle -> Alert
	FearedFleer->ReceiveControl(EAbilitySlot::Fear); // Alert -> Controlled
	const FVector BeforeFlee = FearedFleer->GetActorLocation();
	const FVector CasterLocation(1000.0f, 0.0f, 0.0f);
	FearedFleer->TickFleeMovement(CasterLocation, 0.5f);
	const float FleeDistanceMoved = FVector::Dist(FearedFleer->GetActorLocation(), BeforeFlee);
	TestEqual(TEXT("(v-fear) A Feared enemy should flee at base default speed * DeltaSeconds"),
		FleeDistanceMoved, 600.0f * 0.5f);
	TestTrue(TEXT("(v-fear) A Feared enemy should move away from, not toward, the caster"),
		FVector::Dist(FearedFleer->GetActorLocation(), CasterLocation) > FVector::Dist(BeforeFlee, CasterLocation));

	// (w-fear) TickFleeMovement is a no-op for a Controlled enemy whose
	// ControllingAbility does NOT flag bFleesFromCasterWhileControlled (Stun) -
	// proves the flag-gate, not just the state-gate, mirrors (p2)'s shape.
	AEnemyBaseTestActor* StunnedNonFleer = NewObject<AEnemyBaseTestActor>();
	StunnedNonFleer->TickCheckDetection(ZeroDistanceLocation); // Idle -> Alert
	StunnedNonFleer->ReceiveControl(EAbilitySlot::Stun); // Alert -> Controlled
	const FVector BeforeStunnedFlee = StunnedNonFleer->GetActorLocation();
	StunnedNonFleer->TickFleeMovement(FVector(1000.0f, 0.0f, 0.0f), 1.0f);
	const float StunnedFleeDistanceMoved = FVector::Dist(StunnedNonFleer->GetActorLocation(), BeforeStunnedFlee);
	TestEqual(TEXT("(w-fear) A Stun-Controlled enemy should not flee - flag-gated, not state-gated"),
		StunnedFleeDistanceMoved, 0.0f);

	// (x-fear) TickFleeMovement is a no-op outside Controlled: Idle, Alert, Attack,
	// Banked - mirrors (p)/(r)/(r2)'s no-op-outside-the-relevant-state shape.
	AEnemyBaseTestActor* IdleFleer = NewObject<AEnemyBaseTestActor>();
	const FVector IdleFleeStart = IdleFleer->GetActorLocation();
	IdleFleer->TickFleeMovement(FVector(5000.0f, 0.0f, 0.0f), 1.0f);
	const float IdleFleeDistanceMoved = FVector::Dist(IdleFleer->GetActorLocation(), IdleFleeStart);
	TestEqual(TEXT("(x-fear) TickFleeMovement while Idle should not move the actor"),
		IdleFleeDistanceMoved, 0.0f);

	AEnemyBaseTestActor* AlertFleer = NewObject<AEnemyBaseTestActor>();
	AlertFleer->TickCheckDetection(FVector(1000.0f, 0.0f, 0.0f)); // Idle -> Alert
	const FVector AlertFleeStart = AlertFleer->GetActorLocation();
	AlertFleer->TickFleeMovement(FVector(1000.0f, 0.0f, 0.0f), 1.0f);
	const float AlertFleeDistanceMoved = FVector::Dist(AlertFleer->GetActorLocation(), AlertFleeStart);
	TestEqual(TEXT("(x-fear) TickFleeMovement while Alert should not move the actor"),
		AlertFleeDistanceMoved, 0.0f);

	AEnemyBaseTestActor* AttackFleer = NewObject<AEnemyBaseTestActor>();
	AttackFleer->TickCheckDetection(ZeroDistanceLocation); // Idle -> Alert
	AttackFleer->TickCheckDetection(ZeroDistanceLocation); // Alert -> Attack
	const FVector AttackFleeStart = AttackFleer->GetActorLocation();
	AttackFleer->TickFleeMovement(FVector(5000.0f, 0.0f, 0.0f), 1.0f);
	const float AttackFleeDistanceMoved = FVector::Dist(AttackFleer->GetActorLocation(), AttackFleeStart);
	TestEqual(TEXT("(x-fear) TickFleeMovement while Attack should not move the actor"),
		AttackFleeDistanceMoved, 0.0f);

	AEnemyBaseTestActor* BankedFleer = NewObject<AEnemyBaseTestActor>();
	BankedFleer->TickCheckDetection(ZeroDistanceLocation); // Idle -> Alert
	BankedFleer->ReceiveControl(EAbilitySlot::Fear); // Alert -> Controlled
	BankedFleer->TransitionToBanked(); // Controlled -> Banked
	const FVector BankedFleeStart = BankedFleer->GetActorLocation();
	BankedFleer->TickFleeMovement(FVector(5000.0f, 0.0f, 0.0f), 1.0f);
	const float BankedFleeDistanceMoved = FVector::Dist(BankedFleer->GetActorLocation(), BankedFleeStart);
	TestEqual(TEXT("(x-fear) TickFleeMovement while Banked should not move the actor"),
		BankedFleeDistanceMoved, 0.0f);

	// (y-fear) degenerate direction: a Feared enemy exactly coincident with
	// CasterLocation does not move - mirrors TickChaseMovement's own
	// KINDA_SMALL_NUMBER dead-zone guard, inverted.
	AEnemyBaseTestActor* CoincidentFleer = NewObject<AEnemyBaseTestActor>();
	CoincidentFleer->TickCheckDetection(ZeroDistanceLocation); // Idle -> Alert
	CoincidentFleer->ReceiveControl(EAbilitySlot::Fear); // Alert -> Controlled
	const FVector CoincidentLocation = CoincidentFleer->GetActorLocation();
	CoincidentFleer->TickFleeMovement(CoincidentLocation, 1.0f);
	const float CoincidentFleeDistanceMoved = FVector::Dist(CoincidentFleer->GetActorLocation(), CoincidentLocation);
	TestEqual(TEXT("(y-fear) A Feared enemy exactly coincident with the caster should not move"),
		CoincidentFleeDistanceMoved, 0.0f);

	// (z-fear) the real Tick() override wires TickFleeMovement into the per-frame
	// loop, mirroring case (t)'s TickChaseMovement wiring-through-Tick() shape.
	UWorld* FleeWorld = FAutomationEditorCommonUtils::CreateNewMap();
	if (TestNotNull(TEXT("CreateNewMap should return a valid World"), FleeWorld))
	{
		AEnemyBaseTestActor* TickedFleer = FleeWorld->SpawnActor<AEnemyBaseTestActor>();
		APawn* FleePlayerPawn = FleeWorld->SpawnActor<APawn>();
		if (TestNotNull(TEXT("(z-fear) AEnemyBaseTestActor should spawn into the test World"), TickedFleer)
			&& TestNotNull(TEXT("(z-fear) APawn should spawn into the test World"), FleePlayerPawn))
		{
			APlayerController* FleeController = FleeWorld->SpawnActor<APlayerController>();
			if (!TestNotNull(TEXT("(z-fear) Should be able to spawn a controller to possess the pawn"), FleeController))
			{
				return false;
			}
			FleeController->Possess(FleePlayerPawn);
			FleeWorld->AddController(FleeController);

			USceneComponent* FleePlayerPawnRoot = NewObject<USceneComponent>(FleePlayerPawn);
			FleePlayerPawnRoot->RegisterComponent();
			FleePlayerPawn->SetRootComponent(FleePlayerPawnRoot);
			FleePlayerPawn->SetActorLocation(FVector(1000.0f, 0.0f, 0.0f));

			TickedFleer->TickCheckDetection(FleePlayerPawn->GetActorLocation()); // Idle -> Alert
			TickedFleer->ReceiveControl(EAbilitySlot::Fear); // Alert -> Controlled
			const FVector BeforeFleeTick = TickedFleer->GetActorLocation();
			TickedFleer->Tick(0.1f);
			const float FleeTickDistanceMoved = FVector::Dist(TickedFleer->GetActorLocation(), BeforeFleeTick);
			TestEqual(TEXT("(z-fear) Tick() should move the feared enemy away from the player at base default speed * DeltaSeconds"),
				FleeTickDistanceMoved, 600.0f * 0.1f);
			TestTrue(TEXT("(z-fear) Tick() should move the feared enemy away from, not toward, the player"),
				FVector::Dist(TickedFleer->GetActorLocation(), FleePlayerPawn->GetActorLocation())
					> FVector::Dist(BeforeFleeTick, FleePlayerPawn->GetActorLocation()));

			// A second Tick() after the player moves further along the same ray the enemy
			// is already fleeing along should flee back toward the origin, away from the
			// player's NEW position - proves Tick() re-reads a live player location every
			// frame (per app-changelog/issue-253.md) rather than a cast-time snapshot. This
			// placement is deliberate, not arbitrary: moving the player to (0, 1000, 0)
			// would pass even if TickFleeMovement kept using the stale first-tick caster
			// location, since continuing in the original -X direction still happens to
			// increase distance from a point off the X axis. Placing the new player
			// position further out along the same -X ray the enemy already fled along
			// means live tracking (fleeing back toward +X, away from the new position)
			// and a frozen -X direction (still walking toward the new position) produce
			// opposite distance deltas, so only correct live-tracking passes.
			const FVector AfterFirstTick = TickedFleer->GetActorLocation();
			FleePlayerPawn->SetActorLocation(FVector(-2000.0f, 0.0f, 0.0f));
			TickedFleer->Tick(0.1f);
			const FVector AfterSecondTick = TickedFleer->GetActorLocation();
			TestTrue(TEXT("(z-fear) A second Tick() after the player moves should flee away from the player's NEW position, not a cast-time snapshot"),
				FVector::Dist(AfterSecondTick, FleePlayerPawn->GetActorLocation())
					> FVector::Dist(AfterFirstTick, FleePlayerPawn->GetActorLocation()));
		}
	}

	// (a-follow) TickFollowMovement (issue #214) moves a Stun-Controlled enemy toward
	// PlayerLocation at GetEffectiveFollowSpeedUnitsPerSecond() * DeltaSeconds while
	// farther than FollowDistanceUnits + speed*dt - mirrors (q)'s toward-player shape.
	AEnemyBaseTestActor* FollowingEnemy = NewObject<AEnemyBaseTestActor>();
	FollowingEnemy->TickCheckDetection(ZeroDistanceLocation); // Idle -> Alert
	FollowingEnemy->ReceiveControl(EAbilitySlot::Stun); // Alert -> Controlled
	const FVector BeforeFollow = FollowingEnemy->GetActorLocation();
	const FVector FarFollowPlayerLocation(2000.0f, 0.0f, 0.0f);
	FollowingEnemy->TickFollowMovement(FarFollowPlayerLocation, 1.0f);
	const float FollowDistanceMoved = FVector::Dist(FollowingEnemy->GetActorLocation(), BeforeFollow);
	TestEqual(TEXT("(a-follow) A Stun-Controlled enemy should follow at the effective follow speed * DeltaSeconds"),
		FollowDistanceMoved, FollowingEnemy->GetEffectiveFollowSpeedUnitsPerSecond() * 1.0f);
	TestTrue(TEXT("(a-follow) Following should move the enemy toward, not away from, the player"),
		FVector::Dist(FollowingEnemy->GetActorLocation(), FarFollowPlayerLocation) < FVector::Dist(BeforeFollow, FarFollowPlayerLocation));

	// (root-follow) Root-Controlled enemies DO follow (issue #214 review follow-up) -
	// locks in docs/prd-herd-mechanic.md's operator design decision, which applies to
	// every Controlled enemy with no per-ability carve-out. Root sets neither
	// bAllowsMovementWhileControlled nor bFleesFromCasterWhileControlled, so it falls
	// through TickFollowMovement's gate exactly like Stun/Sleep - mirrors (a-follow)'s
	// shape with EAbilitySlot::Root instead of Stun.
	AEnemyBaseTestActor* RootFollower = NewObject<AEnemyBaseTestActor>();
	RootFollower->TickCheckDetection(ZeroDistanceLocation); // Idle -> Alert
	RootFollower->ReceiveControl(EAbilitySlot::Root); // Alert -> Controlled
	const FVector BeforeRootFollow = RootFollower->GetActorLocation();
	const FVector FarRootFollowPlayerLocation(2000.0f, 0.0f, 0.0f);
	RootFollower->TickFollowMovement(FarRootFollowPlayerLocation, 1.0f);
	const float RootFollowDistanceMoved = FVector::Dist(RootFollower->GetActorLocation(), BeforeRootFollow);
	TestEqual(TEXT("(root-follow) A Root-Controlled enemy should follow at the effective follow speed * DeltaSeconds"),
		RootFollowDistanceMoved, RootFollower->GetEffectiveFollowSpeedUnitsPerSecond() * 1.0f);
	TestTrue(TEXT("(root-follow) Following should move the enemy toward, not away from, the player"),
		FVector::Dist(RootFollower->GetActorLocation(), FarRootFollowPlayerLocation) < FVector::Dist(BeforeRootFollow, FarRootFollowPlayerLocation));

	// (b-follow) overshoot clamp: a player closer than FollowDistanceUnits + speed*dt
	// is approached but the move clamps exactly to FollowDistanceUnits short, never
	// stacking on the player - mirrors (s)'s overshoot-clamp shape, target distance is
	// FollowDistanceUnits, not 0.
	AEnemyBaseTestActor* OvershootFollower = NewObject<AEnemyBaseTestActor>();
	OvershootFollower->TickCheckDetection(ZeroDistanceLocation); // Idle -> Alert
	OvershootFollower->ReceiveControl(EAbilitySlot::Stun); // Alert -> Controlled
	const FVector OvershootPlayerLocation(OvershootFollower->FollowDistanceUnits + 50.0f, 0.0f, 0.0f);
	OvershootFollower->TickFollowMovement(OvershootPlayerLocation, 1.0f); // would move speed*1.0 units - only 50 units of headroom exist before the stop-short gap
	const float OvershootResidualDistance = FVector::Dist(OvershootFollower->GetActorLocation(), OvershootPlayerLocation);
	TestEqual(TEXT("(b-follow) Follow clamps to stop exactly FollowDistanceUnits short of the player, never stacking"),
		OvershootResidualDistance, OvershootFollower->FollowDistanceUnits);

	// (b2-follow) already within FollowDistanceUnits: zero further movement.
	AEnemyBaseTestActor* AlreadyCloseFollower = NewObject<AEnemyBaseTestActor>();
	AlreadyCloseFollower->TickCheckDetection(ZeroDistanceLocation); // Idle -> Alert
	AlreadyCloseFollower->ReceiveControl(EAbilitySlot::Stun); // Alert -> Controlled
	const FVector AlreadyClosePlayerLocation(AlreadyCloseFollower->FollowDistanceUnits - 10.0f, 0.0f, 0.0f);
	const FVector BeforeAlreadyClose = AlreadyCloseFollower->GetActorLocation();
	AlreadyCloseFollower->TickFollowMovement(AlreadyClosePlayerLocation, 1.0f);
	const float AlreadyCloseDistanceMoved = FVector::Dist(AlreadyCloseFollower->GetActorLocation(), BeforeAlreadyClose);
	TestEqual(TEXT("(b2-follow) A follower already within FollowDistanceUnits should not move"),
		AlreadyCloseDistanceMoved, 0.0f);

	// (c-follow) TickFollowMovement is a no-op outside Controlled: Idle, Alert,
	// Attack, Banked - mirrors (p)/(r)/(r2)/(x-fear)'s no-op-outside-the-relevant-
	// state shape.
	AEnemyBaseTestActor* IdleFollower = NewObject<AEnemyBaseTestActor>();
	const FVector IdleFollowStart = IdleFollower->GetActorLocation();
	IdleFollower->TickFollowMovement(FVector(5000.0f, 0.0f, 0.0f), 1.0f);
	const float IdleFollowDistanceMoved = FVector::Dist(IdleFollower->GetActorLocation(), IdleFollowStart);
	TestEqual(TEXT("(c-follow) TickFollowMovement while Idle should not move the actor"),
		IdleFollowDistanceMoved, 0.0f);

	AEnemyBaseTestActor* AlertFollower = NewObject<AEnemyBaseTestActor>();
	AlertFollower->TickCheckDetection(FVector(1000.0f, 0.0f, 0.0f)); // Idle -> Alert
	const FVector AlertFollowStart = AlertFollower->GetActorLocation();
	AlertFollower->TickFollowMovement(FVector(1000.0f, 0.0f, 0.0f), 1.0f);
	const float AlertFollowDistanceMoved = FVector::Dist(AlertFollower->GetActorLocation(), AlertFollowStart);
	TestEqual(TEXT("(c-follow) TickFollowMovement while Alert should not move the actor"),
		AlertFollowDistanceMoved, 0.0f);

	AEnemyBaseTestActor* AttackFollower = NewObject<AEnemyBaseTestActor>();
	AttackFollower->TickCheckDetection(ZeroDistanceLocation); // Idle -> Alert
	AttackFollower->TickCheckDetection(ZeroDistanceLocation); // Alert -> Attack
	const FVector AttackFollowStart = AttackFollower->GetActorLocation();
	AttackFollower->TickFollowMovement(FVector(5000.0f, 0.0f, 0.0f), 1.0f);
	const float AttackFollowDistanceMoved = FVector::Dist(AttackFollower->GetActorLocation(), AttackFollowStart);
	TestEqual(TEXT("(c-follow) TickFollowMovement while Attack should not move the actor"),
		AttackFollowDistanceMoved, 0.0f);

	AEnemyBaseTestActor* BankedFollower = NewObject<AEnemyBaseTestActor>();
	BankedFollower->TickCheckDetection(ZeroDistanceLocation); // Idle -> Alert
	BankedFollower->ReceiveControl(EAbilitySlot::Stun); // Alert -> Controlled
	BankedFollower->TransitionToBanked(); // Controlled -> Banked
	const FVector BankedFollowStart = BankedFollower->GetActorLocation();
	BankedFollower->TickFollowMovement(FVector(5000.0f, 0.0f, 0.0f), 1.0f);
	const float BankedFollowDistanceMoved = FVector::Dist(BankedFollower->GetActorLocation(), BankedFollowStart);
	TestEqual(TEXT("(c-follow) TickFollowMovement while Banked should not move the actor"),
		BankedFollowDistanceMoved, 0.0f);

	// (d-follow) TickFollowMovement is a no-op for Snare-Controlled enemies -
	// bAllowsMovementWhileControlled already claims this tick's movement via
	// TickChaseMovement (see (p3)); this proves the gate on TickFollowMovement
	// itself, not just that TickChaseMovement still works.
	AEnemyBaseTestActor* SnaredNonFollower = NewObject<AEnemyBaseTestActor>();
	SnaredNonFollower->TickCheckDetection(FVector(1000.0f, 0.0f, 0.0f)); // Idle -> Alert
	SnaredNonFollower->ReceiveControl(EAbilitySlot::Snare); // Alert -> Controlled
	const FVector BeforeSnaredFollow = SnaredNonFollower->GetActorLocation();
	SnaredNonFollower->TickFollowMovement(FVector(1000.0f, 0.0f, 0.0f), 1.0f);
	const float SnaredFollowDistanceMoved = FVector::Dist(SnaredNonFollower->GetActorLocation(), BeforeSnaredFollow);
	TestEqual(TEXT("(d-follow) A Snare-Controlled enemy should not be moved by TickFollowMovement - TickChaseMovement already owns this tick"),
		SnaredFollowDistanceMoved, 0.0f);

	// (e-follow) TickFollowMovement is a no-op for Fear-Controlled enemies -
	// bFleesFromCasterWhileControlled already claims this tick's movement via
	// TickFleeMovement (see (v-fear)); proves the gate on TickFollowMovement itself.
	AEnemyBaseTestActor* FearedNonFollower = NewObject<AEnemyBaseTestActor>();
	FearedNonFollower->TickCheckDetection(ZeroDistanceLocation); // Idle -> Alert
	FearedNonFollower->ReceiveControl(EAbilitySlot::Fear); // Alert -> Controlled
	const FVector BeforeFearedFollow = FearedNonFollower->GetActorLocation();
	FearedNonFollower->TickFollowMovement(FVector(1000.0f, 0.0f, 0.0f), 1.0f);
	const float FearedFollowDistanceMoved = FVector::Dist(FearedNonFollower->GetActorLocation(), BeforeFearedFollow);
	TestEqual(TEXT("(e-follow) A Fear-Controlled enemy should not be moved by TickFollowMovement - TickFleeMovement already owns this tick"),
		FearedFollowDistanceMoved, 0.0f);

	// (f-follow) duration-reversion mid-follow: once the Controlled-duration expires
	// and CurrentState reverts to Alert (mirrors (i2)'s expiry pattern), a further
	// TickFollowMovement call is a no-op - the existing state-gate alone halts follow,
	// with zero new transition logic.
	AEnemyBaseTestActor* ExpiringFollower = NewObject<AEnemyBaseTestActor>();
	ExpiringFollower->TickCheckDetection(ZeroDistanceLocation); // Idle -> Alert
	ExpiringFollower->ReceiveControl(EAbilitySlot::Stun); // Alert -> Controlled
	ExpiringFollower->TickFollowMovement(FVector(2000.0f, 0.0f, 0.0f), 1.0f); // moves partway
	const float StunDurationSecondsForFollow = AbilityData::Get(EAbilitySlot::Stun).BaseDurationSeconds;
	ExpiringFollower->TickControlledDuration(StunDurationSecondsForFollow + 1.0f); // forces Controlled -> Alert
	TestEqual(TEXT("(f-follow) precondition: duration expiry reverted the enemy to Alert"),
		static_cast<uint8>(ExpiringFollower->GetEnemyState()), static_cast<uint8>(EEnemyState::Alert));
	const FVector BeforeExpiredFollow = ExpiringFollower->GetActorLocation();
	ExpiringFollower->TickFollowMovement(FVector(2000.0f, 0.0f, 0.0f), 1.0f);
	const float ExpiredFollowDistanceMoved = FVector::Dist(ExpiringFollower->GetActorLocation(), BeforeExpiredFollow);
	TestEqual(TEXT("(f-follow) TickFollowMovement should not move the actor once Controlled-duration expiry reverts it to Alert"),
		ExpiredFollowDistanceMoved, 0.0f);

	// (g-follow) elite interaction: SetIsElite(true) multiplies
	// GetEffectiveFollowSpeedUnitsPerSecond() by EliteMovementSpeedMultiplier, and the
	// actual distance moved by TickFollowMovement matches - mirrors (v)-(x)'s elite-
	// multiplier shape.
	AEnemyBaseTestActor* EliteFollower = NewObject<AEnemyBaseTestActor>();
	EliteFollower->SetIsElite(true);
	TestEqual(TEXT("(g-follow) Effective follow speed should be multiplied while Elite"),
		EliteFollower->GetEffectiveFollowSpeedUnitsPerSecond(),
		EliteFollower->FollowSpeedUnitsPerSecond * EliteFollower->EliteMovementSpeedMultiplier);
	EliteFollower->TickCheckDetection(ZeroDistanceLocation); // Idle -> Alert
	EliteFollower->ReceiveControl(EAbilitySlot::Stun); // Alert -> Controlled
	const FVector BeforeEliteFollow = EliteFollower->GetActorLocation();
	EliteFollower->TickFollowMovement(FVector(5000.0f, 0.0f, 0.0f), 1.0f);
	const float EliteFollowDistanceMoved = FVector::Dist(EliteFollower->GetActorLocation(), BeforeEliteFollow);
	TestEqual(TEXT("(g-follow) TickFollowMovement should move an Elite enemy at its multiplied effective follow speed"),
		EliteFollowDistanceMoved, EliteFollower->GetEffectiveFollowSpeedUnitsPerSecond() * 1.0f);

	// (h-follow) the real Tick() override wires TickFollowMovement into the per-frame
	// loop, mirroring case (t)'s TickChaseMovement wiring-through-Tick() shape.
	UWorld* FollowWorld = FAutomationEditorCommonUtils::CreateNewMap();
	if (TestNotNull(TEXT("CreateNewMap should return a valid World"), FollowWorld))
	{
		AEnemyBaseTestActor* TickedFollower = FollowWorld->SpawnActor<AEnemyBaseTestActor>();
		APawn* FollowPlayerPawn = FollowWorld->SpawnActor<APawn>();
		if (TestNotNull(TEXT("(h-follow) AEnemyBaseTestActor should spawn into the test World"), TickedFollower)
			&& TestNotNull(TEXT("(h-follow) APawn should spawn into the test World"), FollowPlayerPawn))
		{
			APlayerController* FollowController = FollowWorld->SpawnActor<APlayerController>();
			if (!TestNotNull(TEXT("(h-follow) Should be able to spawn a controller to possess the pawn"), FollowController))
			{
				return false;
			}
			FollowController->Possess(FollowPlayerPawn);
			FollowWorld->AddController(FollowController);

			USceneComponent* FollowPlayerPawnRoot = NewObject<USceneComponent>(FollowPlayerPawn);
			FollowPlayerPawnRoot->RegisterComponent();
			FollowPlayerPawn->SetRootComponent(FollowPlayerPawnRoot);
			FollowPlayerPawn->SetActorLocation(FVector(1000.0f, 0.0f, 0.0f)); // within DetectionRangeUnits so TickCheckDetection below actually reaches Alert

			TickedFollower->TickCheckDetection(FollowPlayerPawn->GetActorLocation()); // Idle -> Alert
			TickedFollower->ReceiveControl(EAbilitySlot::Stun); // Alert -> Controlled
			const FVector BeforeFollowTick = TickedFollower->GetActorLocation();
			TickedFollower->Tick(0.1f);
			const float FollowTickDistanceMoved = FVector::Dist(TickedFollower->GetActorLocation(), BeforeFollowTick);
			TestEqual(TEXT("(h-follow) Tick() should move the Controlled enemy toward the player at the effective follow speed * DeltaSeconds"),
				FollowTickDistanceMoved, TickedFollower->GetEffectiveFollowSpeedUnitsPerSecond() * 0.1f);
		}
	}

	// (i-follow) the real Tick() wiring order (TickChaseMovement -> TickFollowMovement
	// -> TickFleeMovement) can't double-apply for a Snare-Controlled enemy - mirrors
	// (h-follow)'s scaffold, swapping Stun for Snare, and asserts total per-frame
	// displacement matches TickChaseMovement's own half-speed distance exactly (i.e.
	// TickFollowMovement's gate held through the real Tick() path, not just in
	// isolation like (d-follow) already proves).
	UWorld* SnareTickWorld = FAutomationEditorCommonUtils::CreateNewMap();
	if (TestNotNull(TEXT("CreateNewMap should return a valid World"), SnareTickWorld))
	{
		AEnemyBaseTestActor* TickedSnaredFollower = SnareTickWorld->SpawnActor<AEnemyBaseTestActor>();
		APawn* SnareTickPlayerPawn = SnareTickWorld->SpawnActor<APawn>();
		if (TestNotNull(TEXT("(i-follow) AEnemyBaseTestActor should spawn into the test World"), TickedSnaredFollower)
			&& TestNotNull(TEXT("(i-follow) APawn should spawn into the test World"), SnareTickPlayerPawn))
		{
			APlayerController* SnareTickController = SnareTickWorld->SpawnActor<APlayerController>();
			if (!TestNotNull(TEXT("(i-follow) Should be able to spawn a controller to possess the pawn"), SnareTickController))
			{
				return false;
			}
			SnareTickController->Possess(SnareTickPlayerPawn);
			SnareTickWorld->AddController(SnareTickController);

			USceneComponent* SnareTickPlayerPawnRoot = NewObject<USceneComponent>(SnareTickPlayerPawn);
			SnareTickPlayerPawnRoot->RegisterComponent();
			SnareTickPlayerPawn->SetRootComponent(SnareTickPlayerPawnRoot);
			SnareTickPlayerPawn->SetActorLocation(FVector(1000.0f, 0.0f, 0.0f)); // within DetectionRangeUnits, matches (h-follow)/(t)

			TickedSnaredFollower->TickCheckDetection(SnareTickPlayerPawn->GetActorLocation()); // Idle -> Alert
			TickedSnaredFollower->ReceiveControl(EAbilitySlot::Snare); // Alert -> Controlled
			const FVector BeforeSnareTick = TickedSnaredFollower->GetActorLocation();
			TickedSnaredFollower->Tick(0.1f);
			const float SnareTickDistanceMoved = FVector::Dist(TickedSnaredFollower->GetActorLocation(), BeforeSnareTick);
			TestEqual(TEXT("(i-follow) Tick() should move a Snare-Controlled enemy at exactly TickChaseMovement's half-speed distance, with zero additional displacement from TickFollowMovement"),
				SnareTickDistanceMoved,
				TickedSnaredFollower->GetEffectiveMovementSpeedUnitsPerSecond() * TickedSnaredFollower->GetControlledSpeedMultiplier() * 0.1f);
		}
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
