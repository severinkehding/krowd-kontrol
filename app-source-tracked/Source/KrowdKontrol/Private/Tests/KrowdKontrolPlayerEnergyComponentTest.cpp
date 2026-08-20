// Confirms UPlayerEnergyComponent (issue #78) reduces CurrentEnergy by the clamped
// amount, clamps RawAmount down to MaxDamagePerHit rather than applying it in full,
// and never lets CurrentEnergy go below 0 - the mechanism PRD 01's REQ-4 (energy only
// drops from enemy contact) and REQ-6 (capped per-hit damage) depend on.
//
// Uses a bare NewObject(), no UWorld needed: unlike the RoomEnemyBudgetController
// test, ApplyContactDamage() calls neither GetWorld() nor GetOwner(), so there is
// nothing here that requires a real editor world to spawn into.
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the other
// KrowdKontrol.Unit.* tests.

#include "Misc/AutomationTest.h"
#include "PlayerEnergyComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolPlayerEnergyComponentTest,
	"KrowdKontrol.Unit.PlayerEnergyComponent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolPlayerEnergyComponentTest::RunTest(const FString& Parameters)
{
	UPlayerEnergyComponent* Component = NewObject<UPlayerEnergyComponent>();
	if (!TestNotNull(TEXT("UPlayerEnergyComponent should construct"), Component))
	{
		return false;
	}

	Component->MaxEnergy = 100.0f;
	Component->MaxDamagePerHit = 10.0f;
	Component->CurrentEnergy = 100.0f;

	// (a) A raw amount within the cap reduces CurrentEnergy by exactly that amount.
	float Applied = Component->ApplyContactDamage(7.0f, nullptr);
	TestEqual(TEXT("Applied damage should equal the uncapped raw amount"), Applied, 7.0f);
	TestEqual(TEXT("CurrentEnergy should drop by the applied amount"), Component->CurrentEnergy, 93.0f);

	// (b) A raw amount above MaxDamagePerHit is clamped down, not applied in full.
	Applied = Component->ApplyContactDamage(50.0f, nullptr);
	TestEqual(TEXT("Applied damage should be clamped to MaxDamagePerHit"), Applied, 10.0f);
	TestEqual(TEXT("CurrentEnergy should drop by MaxDamagePerHit, not the raw amount"), Component->CurrentEnergy, 83.0f);

	// (c) CurrentEnergy never drops below 0, even with many hits.
	Component->CurrentEnergy = 5.0f;
	Applied = Component->ApplyContactDamage(10.0f, nullptr);
	TestEqual(TEXT("Applied damage should be limited by remaining energy, not MaxDamagePerHit alone"), Applied, 5.0f);
	TestEqual(TEXT("CurrentEnergy should floor at 0"), Component->CurrentEnergy, 0.0f);

	Applied = Component->ApplyContactDamage(10.0f, nullptr);
	TestEqual(TEXT("Further damage at 0 energy should apply nothing"), Applied, 0.0f);
	TestEqual(TEXT("CurrentEnergy should remain at 0"), Component->CurrentEnergy, 0.0f);

	// (d) A misconfigured negative MaxDamagePerHit must not flip ApplyContactDamage
	// into a heal - FMath::Clamp doesn't assume Min <= Max, so this only holds because
	// ApplyContactDamage clamps MaxDamagePerHit itself to a non-negative floor first.
	Component->MaxDamagePerHit = -5.0f;
	Component->CurrentEnergy = 50.0f;
	Applied = Component->ApplyContactDamage(3.0f, nullptr);
	TestEqual(TEXT("A negative MaxDamagePerHit should apply zero damage, never negative"), Applied, 0.0f);
	TestEqual(TEXT("CurrentEnergy should not increase from a negative MaxDamagePerHit"), Component->CurrentEnergy, 50.0f);

	// (e) A negative RawAmount must not heal the player - ApplyContactDamage clamps
	// RawAmount's lower bound to 0 before it ever reaches MaxDamagePerHit's own clamp,
	// so a negative raw amount applies zero damage rather than negative damage (which
	// would increase CurrentEnergy).
	Component->MaxDamagePerHit = 10.0f;
	Component->CurrentEnergy = 50.0f;
	Applied = Component->ApplyContactDamage(-20.0f, nullptr);
	TestEqual(TEXT("A negative RawAmount should apply zero damage, never negative"), Applied, 0.0f);
	TestEqual(TEXT("CurrentEnergy should not increase from a negative RawAmount"), Component->GetCurrentEnergy(), 50.0f);

	// (f) Construct-twice invariant (issue #172 follow-up): CurrentEnergy is seeded
	// only in the constructor, from MaxEnergy - so a freshly-constructed instance
	// always starts full regardless of any other instance's state (here, the drained
	// Component above). This is the invariant the level-restart flow's "full energy
	// for free on reload" claim depends on - a future persistence mechanism (e.g. a
	// GameInstanceSubsystem-cached energy value) breaking this would fail here.
	UPlayerEnergyComponent* FreshComponent = NewObject<UPlayerEnergyComponent>();
	if (TestNotNull(TEXT("A second UPlayerEnergyComponent should construct"), FreshComponent))
	{
		TestEqual(TEXT("A freshly constructed component should start at MaxEnergy"),
			FreshComponent->GetCurrentEnergy(), FreshComponent->MaxEnergy);
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
