// Confirms UAbilityLockoutComponent (issue #178, PRD "Punishment System" REQ-2) locks
// the most recently cast ability (falling back to Stun if nothing has been cast yet
// this run) for a fixed duration on HandlePunishmentTriggered(), that the lockout
// expires and is castable again after exactly LockoutDurationSeconds, that each
// ability slot's lockout tracks independently, that OnAbilityLockoutChanged fires
// exactly once per true state transition (not on every intermediate AdvanceLockouts()
// call, and not a second time if HandlePunishmentTriggered() re-triggers a slot
// already locked), and that a large AdvanceLockouts() delta clamps remaining time to 0.
//
// Uses a bare NewObject(), no UWorld needed: HandleAbilityCastApplied,
// HandlePunishmentTriggered, and AdvanceLockouts call neither GetWorld() nor
// GetOwner(), mirroring KrowdKontrolAbilityCooldownTest.cpp.
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the other
// KrowdKontrol.Unit.* tests.

#include "Misc/AutomationTest.h"
#include "AbilityLockoutComponent.h"
#include "AbilityLockoutChangedTestListener.h"
#include "HAL/IConsoleManager.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolAbilityLockoutComponentTest,
	"KrowdKontrol.Unit.AbilityLockoutComponent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolAbilityLockoutComponentTest::RunTest(const FString& Parameters)
{
	// (a) No ability cast yet this run: HandlePunishmentTriggered() alone locks Stun
	// (the fallback), leaving every other slot unlocked.
	{
		UAbilityLockoutComponent* Component = NewObject<UAbilityLockoutComponent>();
		if (!TestNotNull(TEXT("UAbilityLockoutComponent should construct"), Component))
		{
			return false;
		}

		Component->HandlePunishmentTriggered();
		TestTrue(TEXT("Stun should be locked as the no-cast-yet fallback"), Component->IsAbilityLocked(EAbilitySlot::Stun));
		TestEqual(TEXT("Stun remaining should equal the default lockout duration"),
			Component->GetRemainingLockoutSeconds(EAbilitySlot::Stun), UAbilityLockoutComponent::DefaultLockoutDurationSeconds);
		TestFalse(TEXT("Sleep should not be locked"), Component->IsAbilityLocked(EAbilitySlot::Sleep));
		TestFalse(TEXT("Root should not be locked"), Component->IsAbilityLocked(EAbilitySlot::Root));
		TestFalse(TEXT("Fear should not be locked"), Component->IsAbilityLocked(EAbilitySlot::Fear));
		TestFalse(TEXT("Snare should not be locked"), Component->IsAbilityLocked(EAbilitySlot::Snare));
	}

	// (b) A cast recorded via HandleAbilityCastApplied changes which slot gets locked.
	{
		UAbilityLockoutComponent* Component = NewObject<UAbilityLockoutComponent>();
		if (!TestNotNull(TEXT("UAbilityLockoutComponent should construct"), Component))
		{
			return false;
		}

		Component->HandleAbilityCastApplied(EAbilitySlot::Root, nullptr);
		Component->HandlePunishmentTriggered();
		TestTrue(TEXT("Root should be locked - the most recently cast ability"), Component->IsAbilityLocked(EAbilitySlot::Root));
		TestFalse(TEXT("Stun should not be locked once a real cast has happened"), Component->IsAbilityLocked(EAbilitySlot::Stun));
	}

	// (c) Expiry timing: locked one tick short of the configured duration, unlocked
	// exactly at it.
	{
		UAbilityLockoutComponent* Component = NewObject<UAbilityLockoutComponent>();
		if (!TestNotNull(TEXT("UAbilityLockoutComponent should construct"), Component))
		{
			return false;
		}

		Component->HandlePunishmentTriggered();
		Component->AdvanceLockouts(UAbilityLockoutComponent::DefaultLockoutDurationSeconds - 1.0f);
		TestTrue(TEXT("Stun should still be locked one second short of the duration"), Component->IsAbilityLocked(EAbilitySlot::Stun));
		Component->AdvanceLockouts(1.0f);
		TestFalse(TEXT("Stun should be unlocked exactly at the configured duration"), Component->IsAbilityLocked(EAbilitySlot::Stun));
		TestEqual(TEXT("Stun remaining should be exactly 0 after expiry"), Component->GetRemainingLockoutSeconds(EAbilitySlot::Stun), 0.0f);
	}

	// (d) Independence: locking one slot leaves every other slot's state untouched,
	// and AdvanceLockouts only decrements slots actually locked.
	{
		UAbilityLockoutComponent* Component = NewObject<UAbilityLockoutComponent>();
		if (!TestNotNull(TEXT("UAbilityLockoutComponent should construct"), Component))
		{
			return false;
		}

		Component->HandleAbilityCastApplied(EAbilitySlot::Sleep, nullptr);
		Component->HandlePunishmentTriggered();
		Component->AdvanceLockouts(1.0f);
		TestEqual(TEXT("Sleep remaining should decrement by the advanced delta"),
			Component->GetRemainingLockoutSeconds(EAbilitySlot::Sleep), UAbilityLockoutComponent::DefaultLockoutDurationSeconds - 1.0f);
		TestEqual(TEXT("Stun remaining should stay 0 - it was never locked"), Component->GetRemainingLockoutSeconds(EAbilitySlot::Stun), 0.0f);
		TestFalse(TEXT("Root should not be locked"), Component->IsAbilityLocked(EAbilitySlot::Root));
		TestFalse(TEXT("Fear should not be locked"), Component->IsAbilityLocked(EAbilitySlot::Fear));
		TestFalse(TEXT("Snare should not be locked"), Component->IsAbilityLocked(EAbilitySlot::Snare));
	}

	// (e) OnAbilityLockoutChanged fires exactly once with (Ability, true) on the
	// trigger, exactly once with (Ability, false) on expiry - not on every
	// intermediate AdvanceLockouts() call while still locked, and not again if
	// HandlePunishmentTriggered() is called a second time while the same slot is
	// already locked (timer refreshes silently).
	{
		UAbilityLockoutComponent* Component = NewObject<UAbilityLockoutComponent>();
		if (!TestNotNull(TEXT("UAbilityLockoutComponent should construct"), Component))
		{
			return false;
		}

		UAbilityLockoutChangedTestListener* Listener = NewObject<UAbilityLockoutChangedTestListener>();
		Component->OnAbilityLockoutChanged.AddDynamic(Listener, &UAbilityLockoutChangedTestListener::HandleAbilityLockoutChanged);

		Component->HandlePunishmentTriggered();
		TestEqual(TEXT("OnAbilityLockoutChanged should fire exactly once on the trigger"), Listener->CallCount, 1);
		TestEqual(TEXT("The broadcast should carry the locked ability"),
			static_cast<uint8>(Listener->LastAbility), static_cast<uint8>(EAbilitySlot::Stun));
		TestTrue(TEXT("The broadcast should carry bLocked=true"), Listener->LastLocked);

		// Re-trigger while already locked: timer refreshes, no duplicate broadcast.
		Component->HandlePunishmentTriggered();
		TestEqual(TEXT("A repeated trigger while already locked should not re-broadcast"), Listener->CallCount, 1);

		// Intermediate AdvanceLockouts calls while still locked must not broadcast.
		Component->AdvanceLockouts(1.0f);
		Component->AdvanceLockouts(1.0f);
		TestEqual(TEXT("Intermediate AdvanceLockouts calls while still locked should not broadcast"), Listener->CallCount, 1);

		// Expiry broadcasts exactly once.
		Component->AdvanceLockouts(UAbilityLockoutComponent::DefaultLockoutDurationSeconds);
		TestEqual(TEXT("OnAbilityLockoutChanged should fire exactly once more on expiry"), Listener->CallCount, 2);
		TestFalse(TEXT("The expiry broadcast should carry bLocked=false"), Listener->LastLocked);

		// Further AdvanceLockouts calls on an already-expired slot must not re-broadcast.
		Component->AdvanceLockouts(1.0f);
		TestEqual(TEXT("AdvanceLockouts on an already-expired slot should not re-broadcast"), Listener->CallCount, 2);
	}

	// (f) A large AdvanceLockouts delta clamps remaining time to 0, never negative.
	{
		UAbilityLockoutComponent* Component = NewObject<UAbilityLockoutComponent>();
		if (!TestNotNull(TEXT("UAbilityLockoutComponent should construct"), Component))
		{
			return false;
		}

		Component->HandlePunishmentTriggered();
		Component->AdvanceLockouts(1000.0f);
		TestEqual(TEXT("A large AdvanceLockouts should clamp remaining time to 0, not negative"),
			Component->GetRemainingLockoutSeconds(EAbilitySlot::Stun), 0.0f);
		TestFalse(TEXT("Stun should no longer be locked after the large advance"), Component->IsAbilityLocked(EAbilitySlot::Stun));
	}

	// (g) Two different slots can be locked concurrently by two separate trigger events,
	// and each expires independently of the other - the reason this component uses a
	// TArray<float> per slot rather than a single scalar (see this component's own
	// class-comment rationale).
	{
		UAbilityLockoutComponent* Component = NewObject<UAbilityLockoutComponent>();
		if (!TestNotNull(TEXT("UAbilityLockoutComponent should construct"), Component))
		{
			return false;
		}

		Component->HandleAbilityCastApplied(EAbilitySlot::Root, nullptr);
		Component->HandlePunishmentTriggered();
		Component->AdvanceLockouts(3.0f);

		Component->HandleAbilityCastApplied(EAbilitySlot::Sleep, nullptr);
		Component->HandlePunishmentTriggered();

		TestTrue(TEXT("Root should still be locked while Sleep starts its own lockout"), Component->IsAbilityLocked(EAbilitySlot::Root));
		TestTrue(TEXT("Sleep should be locked"), Component->IsAbilityLocked(EAbilitySlot::Sleep));
		TestEqual(TEXT("Root remaining should reflect only its own elapsed time"),
			Component->GetRemainingLockoutSeconds(EAbilitySlot::Root), UAbilityLockoutComponent::DefaultLockoutDurationSeconds - 3.0f);
		TestEqual(TEXT("Sleep remaining should be a fresh full duration"),
			Component->GetRemainingLockoutSeconds(EAbilitySlot::Sleep), UAbilityLockoutComponent::DefaultLockoutDurationSeconds);

		Component->AdvanceLockouts(UAbilityLockoutComponent::DefaultLockoutDurationSeconds - 3.0f);
		TestFalse(TEXT("Root should expire on its own original schedule"), Component->IsAbilityLocked(EAbilitySlot::Root));
		TestTrue(TEXT("Sleep should still be locked - it started later"), Component->IsAbilityLocked(EAbilitySlot::Sleep));
	}

	// (h) EndAllLockouts() clears every currently-locked slot immediately, broadcasting
	// OnAbilityLockoutChanged(Slot, false) exactly once per slot that actually transitions
	// - not for slots that were never locked, and not just the first slot found. A second
	// call on an already-clear component must not re-broadcast.
	{
		UAbilityLockoutComponent* Component = NewObject<UAbilityLockoutComponent>();
		if (!TestNotNull(TEXT("UAbilityLockoutComponent should construct"), Component))
		{
			return false;
		}

		UAbilityLockoutChangedTestListener* Listener = NewObject<UAbilityLockoutChangedTestListener>();
		Component->OnAbilityLockoutChanged.AddDynamic(Listener, &UAbilityLockoutChangedTestListener::HandleAbilityLockoutChanged);

		Component->HandleAbilityCastApplied(EAbilitySlot::Root, nullptr);
		Component->HandlePunishmentTriggered();
		Component->HandleAbilityCastApplied(EAbilitySlot::Sleep, nullptr);
		Component->HandlePunishmentTriggered();
		Listener->CallCount = 0; // reset after setup broadcasts

		Component->EndAllLockouts();
		TestFalse(TEXT("Root should be unlocked after EndAllLockouts"), Component->IsAbilityLocked(EAbilitySlot::Root));
		TestFalse(TEXT("Sleep should be unlocked after EndAllLockouts"), Component->IsAbilityLocked(EAbilitySlot::Sleep));
		TestEqual(TEXT("EndAllLockouts should broadcast exactly once per slot that actually transitioned"), Listener->CallCount, 2);

		Component->EndAllLockouts();
		TestEqual(TEXT("EndAllLockouts on an already-clear component should not broadcast"), Listener->CallCount, 2);
	}

	// (i) kk.Punishment.LockoutEnabled=0 prevents HandlePunishmentTriggered from
	// locking anything; restoring the CVar to 1 (its default) immediately allows
	// normal activation again. The CVar is process-wide state shared by every
	// KrowdKontrol.Unit.* test running in the same Automation Framework pass, so
	// this always ends by leaving it at 1, regardless of which assertion above
	// fails first.
	{
		IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("kk.Punishment.LockoutEnabled"));
		if (!TestNotNull(TEXT("kk.Punishment.LockoutEnabled CVar should be registered"), CVar))
		{
			return false;
		}

		UAbilityLockoutComponent* Component = NewObject<UAbilityLockoutComponent>();
		if (!TestNotNull(TEXT("UAbilityLockoutComponent should construct"), Component))
		{
			return false;
		}

		CVar->Set(0);
		Component->HandlePunishmentTriggered();
		TestFalse(TEXT("Stun should not be locked while kk.Punishment.LockoutEnabled is 0"),
			Component->IsAbilityLocked(EAbilitySlot::Stun));

		CVar->Set(1);
		Component->HandlePunishmentTriggered();
		TestTrue(TEXT("Stun should lock normally once kk.Punishment.LockoutEnabled is restored to 1"),
			Component->IsAbilityLocked(EAbilitySlot::Stun));
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
