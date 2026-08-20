// Confirms UPunishmentManagerComponent (issue #177, PRD "Punishment System" REQ-1)
// re-broadcasts OnPunishmentTriggered exactly once per real contact-damage event
// observed via UPlayerEnergyComponent::OnEnergyChanged, that repeated real damage
// events each fire independently, and that a call to ApplyContactDamage which does
// not actually change CurrentEnergy (already at 0) does not spuriously trigger a
// punishment - the deliberate, documented edge case of this component's trigger
// semantics.
//
// Uses a bare NewObject(), no UWorld needed: neither ApplyContactDamage nor
// HandleEnergyChanged call GetWorld() or GetOwner(), mirroring
// KrowdKontrolPlayerEnergyComponentTest.cpp.
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the other
// KrowdKontrol.Unit.* tests.

#include "Misc/AutomationTest.h"
#include "PlayerEnergyComponent.h"
#include "PunishmentManagerComponent.h"
#include "PunishmentTriggeredTestListener.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolPunishmentManagerComponentTest,
	"KrowdKontrol.Unit.PunishmentManagerComponent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolPunishmentManagerComponentTest::RunTest(const FString& Parameters)
{
	UPlayerEnergyComponent* Energy = NewObject<UPlayerEnergyComponent>();
	if (!TestNotNull(TEXT("UPlayerEnergyComponent should construct"), Energy))
	{
		return false;
	}

	UPunishmentManagerComponent* Manager = NewObject<UPunishmentManagerComponent>();
	if (!TestNotNull(TEXT("UPunishmentManagerComponent should construct"), Manager))
	{
		return false;
	}

	Energy->MaxEnergy = 100.0f;
	Energy->MaxDamagePerHit = 10.0f;
	Energy->CurrentEnergy = 100.0f;

	Energy->OnEnergyChanged.AddDynamic(Manager, &UPunishmentManagerComponent::HandleEnergyChanged);

	UPunishmentTriggeredTestListener* Listener = NewObject<UPunishmentTriggeredTestListener>();
	if (!TestNotNull(TEXT("UPunishmentTriggeredTestListener should construct"), Listener))
	{
		return false;
	}
	Manager->OnPunishmentTriggered.AddDynamic(Listener, &UPunishmentTriggeredTestListener::HandlePunishmentTriggered);

	// (a) One real damage event fires the punishment trigger exactly once.
	Energy->ApplyContactDamage(7.0f, nullptr);
	TestEqual(TEXT("One real damage event should trigger the punishment signal exactly once"), Listener->CallCount, 1);

	// (b) A second, independent real damage event fires a second, independent broadcast.
	Energy->ApplyContactDamage(7.0f, nullptr);
	TestEqual(TEXT("A second real damage event should trigger a second, independent broadcast"), Listener->CallCount, 2);

	// (c) ApplyContactDamage with no actual energy change (already at 0) must not
	// spuriously trigger a punishment - the trigger is keyed to "energy actually
	// changed", not "method was called".
	Energy->CurrentEnergy = 0.0f;
	Energy->ApplyContactDamage(7.0f, nullptr);
	TestEqual(TEXT("A no-op ApplyContactDamage call (already at 0 energy) should not trigger a punishment"), Listener->CallCount, 2);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
