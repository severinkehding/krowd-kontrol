// Confirms ABossBase (issue #44, PRD 04) guarantees: (1) the state machine
// cannot reach a "defeated" state other than Banked - no path skips a state,
// no path reaches Banked from anywhere but Vulnerable, and nothing ever leaves
// Banked once reached - and (2) the shield/split/enrage flags are toggleable
// and observable via their subclass-overridable hooks, including a guard
// against redundant same-value sets.
//
// Uses NewObject rather than spawning into a UWorld: ABossBase never calls
// GetWorld()/SpawnActor, same rationale as KrowdKontrolThreatStateTest.cpp.
//
// "No new enemy type, ability, or colour is introduced" (the issue's 4th
// acceptance criterion) is satisfied structurally, not by a runtime assertion -
// this file adds no enum value, ability, or colour reference anywhere.
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the
// other KrowdKontrol.Unit.* tests.

#include "Misc/AutomationTest.h"
#include "BossBase.h"
#include "BossBaseTestActor.h"
#include "BossBankedTestListener.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolBossBaseTest,
	"KrowdKontrol.Unit.BossBase",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolBossBaseTest::RunTest(const FString& Parameters)
{
	ABossBaseTestActor* Boss = NewObject<ABossBaseTestActor>();
	if (!TestNotNull(TEXT("ABossBaseTestActor should construct"), Boss))
	{
		return false;
	}

	UBossBankedTestListener* Listener = NewObject<UBossBankedTestListener>();
	Boss->OnBossBanked.AddDynamic(Listener, &UBossBankedTestListener::HandleBossBanked);

	// (a) default state.
	TestEqual(TEXT("Default boss state should be Idle"),
		static_cast<uint8>(Boss->GetBossState()), static_cast<uint8>(EBossState::Idle));

	// (b) no skipping steps - calling a transition method from the wrong
	// predecessor state must be a no-op and must not corrupt state.
	Boss->AdvanceToVulnerable();
	TestEqual(TEXT("AdvanceToVulnerable from Idle should be a no-op"),
		static_cast<uint8>(Boss->GetBossState()), static_cast<uint8>(EBossState::Idle));
	Boss->TransitionToBanked();
	TestEqual(TEXT("TransitionToBanked from Idle should be a no-op"),
		static_cast<uint8>(Boss->GetBossState()), static_cast<uint8>(EBossState::Idle));
	TestEqual(TEXT("OnBossBanked should not have fired yet"), Listener->CallCount, 0);

	// (c) full valid progression.
	Boss->AdvanceToArmed();
	TestEqual(TEXT("AdvanceToArmed from Idle should move to Armed"),
		static_cast<uint8>(Boss->GetBossState()), static_cast<uint8>(EBossState::Armed));

	Boss->AdvanceToArmed();
	TestEqual(TEXT("Repeated AdvanceToArmed should not double-advance"),
		static_cast<uint8>(Boss->GetBossState()), static_cast<uint8>(EBossState::Armed));

	Boss->AdvanceToVulnerable();
	TestEqual(TEXT("AdvanceToVulnerable from Armed should move to Vulnerable"),
		static_cast<uint8>(Boss->GetBossState()), static_cast<uint8>(EBossState::Vulnerable));

	Boss->TransitionToBanked();
	TestEqual(TEXT("TransitionToBanked from Vulnerable should move to Banked"),
		static_cast<uint8>(Boss->GetBossState()), static_cast<uint8>(EBossState::Banked));
	TestEqual(TEXT("OnBossBanked should have fired exactly once"), Listener->CallCount, 1);

	// (d) terminal/idempotent - once in Banked, nothing (including the very
	// methods that got it there) can move it anywhere else, and the delegate
	// never re-fires.
	Boss->AdvanceToArmed();
	TestEqual(TEXT("AdvanceToArmed after Banked should be a no-op"),
		static_cast<uint8>(Boss->GetBossState()), static_cast<uint8>(EBossState::Banked));
	Boss->AdvanceToVulnerable();
	TestEqual(TEXT("AdvanceToVulnerable after Banked should be a no-op"),
		static_cast<uint8>(Boss->GetBossState()), static_cast<uint8>(EBossState::Banked));
	Boss->TransitionToBanked();
	TestEqual(TEXT("TransitionToBanked after Banked should be a no-op"),
		static_cast<uint8>(Boss->GetBossState()), static_cast<uint8>(EBossState::Banked));
	TestEqual(TEXT("OnBossBanked should still have fired exactly once"), Listener->CallCount, 1);

	// (e) actor is pacified, not destroyed - no code path in ABossBase ever
	// calls Destroy().
	TestFalse(TEXT("Boss actor should not be destroyed by reaching Banked"), Boss->IsActorBeingDestroyed());

	// (f) shield/split/enrage toggleable and observable, independently.
	TestFalse(TEXT("Default HasShield should be false"), Boss->HasShield());
	Boss->SetHasShield(true);
	TestTrue(TEXT("HasShield should be true after SetHasShield(true)"), Boss->HasShield());
	TestEqual(TEXT("OnShieldChanged should have fired once"), Boss->ShieldChangedCallCount, 1);
	Boss->SetHasShield(true);
	TestEqual(TEXT("OnShieldChanged should not fire on a redundant same-value set"),
		Boss->ShieldChangedCallCount, 1);
	Boss->SetHasShield(false);
	TestFalse(TEXT("HasShield should be false after SetHasShield(false)"), Boss->HasShield());
	TestEqual(TEXT("OnShieldChanged should have fired twice"), Boss->ShieldChangedCallCount, 2);

	TestFalse(TEXT("Default IsSplit should be false"), Boss->IsSplit());
	Boss->SetIsSplit(true);
	TestTrue(TEXT("IsSplit should be true after SetIsSplit(true)"), Boss->IsSplit());
	TestEqual(TEXT("OnSplitChanged should have fired once"), Boss->SplitChangedCallCount, 1);
	Boss->SetIsSplit(true);
	TestEqual(TEXT("OnSplitChanged should not fire on a redundant same-value set"),
		Boss->SplitChangedCallCount, 1);
	Boss->SetIsSplit(false);
	TestFalse(TEXT("IsSplit should be false after SetIsSplit(false)"), Boss->IsSplit());
	TestEqual(TEXT("OnSplitChanged should have fired twice"), Boss->SplitChangedCallCount, 2);

	TestFalse(TEXT("Default IsEnraged should be false"), Boss->IsEnraged());
	Boss->SetIsEnraged(true);
	TestTrue(TEXT("IsEnraged should be true after SetIsEnraged(true)"), Boss->IsEnraged());
	TestEqual(TEXT("OnEnrageChanged should have fired once"), Boss->EnrageChangedCallCount, 1);
	Boss->SetIsEnraged(true);
	TestEqual(TEXT("OnEnrageChanged should not fire on a redundant same-value set"),
		Boss->EnrageChangedCallCount, 1);
	Boss->SetIsEnraged(false);
	TestFalse(TEXT("IsEnraged should be false after SetIsEnraged(false)"), Boss->IsEnraged());
	TestEqual(TEXT("OnEnrageChanged should have fired twice"), Boss->EnrageChangedCallCount, 2);

	// Confirm the three flags are independent - toggling one never moved another's
	// call count beyond what its own assertions above already account for.
	TestEqual(TEXT("Shield hook call count should be unaffected by split/enrage toggling"),
		Boss->ShieldChangedCallCount, 2);
	TestEqual(TEXT("Split hook call count should be unaffected by shield/enrage toggling"),
		Boss->SplitChangedCallCount, 2);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
