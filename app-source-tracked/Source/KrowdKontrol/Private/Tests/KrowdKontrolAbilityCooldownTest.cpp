// Confirms UAbilityCooldownComponent (issue #71) blocks a recast on a slot still on
// cooldown and allows an immediate recast once that slot's cooldown expires - the two
// acceptance criteria PRD 02 REQ-5 requires - and that each of the 5 slots tracks
// independently.
//
// Uses a bare NewObject(), no UWorld needed: TryStartCooldown/AdvanceCooldowns call
// neither GetWorld() nor GetOwner(), mirroring KrowdKontrolPlayerEnergyComponentTest.
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the other
// KrowdKontrol.Unit.* tests.

#include "Misc/AutomationTest.h"
#include "AbilityCooldownComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolAbilityCooldownTest,
	"KrowdKontrol.Unit.AbilityCooldown",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolAbilityCooldownTest::RunTest(const FString& Parameters)
{
	UAbilityCooldownComponent* Component = NewObject<UAbilityCooldownComponent>();
	if (!TestNotNull(TEXT("UAbilityCooldownComponent should construct"), Component))
	{
		return false;
	}

	// (a) TryStartCooldown on a fresh component starts the cooldown.
	bool Started = Component->TryStartCooldown(EAbilitySlot::Stun);
	TestTrue(TEXT("First TryStartCooldown(Stun) should succeed"), Started);
	TestTrue(TEXT("Stun should be on cooldown after starting"), Component->IsOnCooldown(EAbilitySlot::Stun));
	TestEqual(TEXT("Stun remaining should equal the configured duration"),
		Component->GetRemainingCooldownSeconds(EAbilitySlot::Stun), UAbilityCooldownComponent::DefaultAbilityCooldownSeconds);

	// (b) A second TryStartCooldown while still on cooldown is blocked, no state change.
	Started = Component->TryStartCooldown(EAbilitySlot::Stun);
	TestFalse(TEXT("TryStartCooldown(Stun) mid-cooldown should be blocked"), Started);
	TestEqual(TEXT("Stun remaining should be unchanged by the blocked recast"),
		Component->GetRemainingCooldownSeconds(EAbilitySlot::Stun), UAbilityCooldownComponent::DefaultAbilityCooldownSeconds);

	// (c) Advancing exactly to the configured duration clears the cooldown, allowing an
	// immediate recast.
	Component->AdvanceCooldowns(UAbilityCooldownComponent::DefaultAbilityCooldownSeconds);
	TestFalse(TEXT("Stun should no longer be on cooldown after advancing past its duration"), Component->IsOnCooldown(EAbilitySlot::Stun));
	TestEqual(TEXT("Stun remaining should be exactly 0 after expiry"), Component->GetRemainingCooldownSeconds(EAbilitySlot::Stun), 0.0f);
	Started = Component->TryStartCooldown(EAbilitySlot::Stun);
	TestTrue(TEXT("TryStartCooldown(Stun) should succeed immediately after expiry"), Started);
	Component->AdvanceCooldowns(UAbilityCooldownComponent::DefaultAbilityCooldownSeconds);

	// (d) Independence: starting one slot's cooldown does not affect the others, and
	// advancing time only decrements slots actually on cooldown.
	Component->TryStartCooldown(EAbilitySlot::Sleep);
	TestFalse(TEXT("Stun should not be on cooldown from Sleep's cast"), Component->IsOnCooldown(EAbilitySlot::Stun));
	TestFalse(TEXT("Root should not be on cooldown from Sleep's cast"), Component->IsOnCooldown(EAbilitySlot::Root));
	TestFalse(TEXT("Fear should not be on cooldown from Sleep's cast"), Component->IsOnCooldown(EAbilitySlot::Fear));
	TestFalse(TEXT("Snare should not be on cooldown from Sleep's cast"), Component->IsOnCooldown(EAbilitySlot::Snare));
	Component->AdvanceCooldowns(1.0f);
	TestEqual(TEXT("Sleep remaining should decrement by the advanced delta"),
		Component->GetRemainingCooldownSeconds(EAbilitySlot::Sleep), UAbilityCooldownComponent::DefaultAbilityCooldownSeconds - 1.0f);
	TestEqual(TEXT("Stun remaining should stay 0 - it was never started"), Component->GetRemainingCooldownSeconds(EAbilitySlot::Stun), 0.0f);
	Component->AdvanceCooldowns(UAbilityCooldownComponent::DefaultAbilityCooldownSeconds);

	// (e) Per-slot configurability: a custom duration is honored, not hardcoded.
	Component->AbilityCooldownDurations[static_cast<int32>(EAbilitySlot::Root)] = 1.5f;
	Started = Component->TryStartCooldown(EAbilitySlot::Root);
	TestTrue(TEXT("TryStartCooldown(Root) should succeed with a custom duration configured"), Started);
	TestEqual(TEXT("Root remaining should equal its custom configured duration"), Component->GetRemainingCooldownSeconds(EAbilitySlot::Root), 1.5f);
	Component->AdvanceCooldowns(1.5f);

	// (f) A large delta clamps remaining time to 0, never negative.
	Component->TryStartCooldown(EAbilitySlot::Fear);
	Component->AdvanceCooldowns(100.0f);
	TestEqual(TEXT("A large AdvanceCooldowns should clamp remaining time to 0, not negative"),
		Component->GetRemainingCooldownSeconds(EAbilitySlot::Fear), 0.0f);
	TestFalse(TEXT("Fear should no longer be on cooldown after the large advance"), Component->IsOnCooldown(EAbilitySlot::Fear));

	// (g) Invalid/misconfigured AbilityCooldownDurations does not crash and falls back
	// to a safe non-negative duration.
	Component->AbilityCooldownDurations.SetNum(2); // shorter than NumAbilitySlots
	Started = Component->TryStartCooldown(EAbilitySlot::Snare);
	TestTrue(TEXT("TryStartCooldown(Snare) should still succeed with a too-short durations array"), Started);
	TestEqual(TEXT("Snare should fall back to the default duration when its configured entry is missing"),
		Component->GetRemainingCooldownSeconds(EAbilitySlot::Snare), UAbilityCooldownComponent::DefaultAbilityCooldownSeconds);
	Component->AdvanceCooldowns(UAbilityCooldownComponent::DefaultAbilityCooldownSeconds);

	Component->AbilityCooldownDurations.Init(UAbilityCooldownComponent::DefaultAbilityCooldownSeconds, UAbilityCooldownComponent::NumAbilitySlots);
	Component->AbilityCooldownDurations[static_cast<int32>(EAbilitySlot::Snare)] = -5.0f;
	Started = Component->TryStartCooldown(EAbilitySlot::Snare);
	TestTrue(TEXT("TryStartCooldown(Snare) should still succeed with a negative configured duration"), Started);
	TestEqual(TEXT("A negative configured duration should clamp to 0, never negative"), Component->GetRemainingCooldownSeconds(EAbilitySlot::Snare), 0.0f);
	TestFalse(TEXT("Snare should not read as on cooldown when its clamped duration is 0"), Component->IsOnCooldown(EAbilitySlot::Snare));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
