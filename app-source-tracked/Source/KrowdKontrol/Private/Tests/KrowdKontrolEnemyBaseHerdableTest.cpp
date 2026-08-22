// Confirms AEnemyBase (issue #211) implements IHerdable correctly against its own
// real state machine, not a test-only fixture: IsControlled() tracks CurrentState ==
// Controlled, and GetHerdColourTag() reports the ColourTag of whichever ability is
// currently controlling it (AbilityData::Get(ControllingAbility).ColourTag), per
// EnemyBase.cpp's new IsControlled()/GetHerdColourTag() overrides.
//
// Uses AEnemyBaseTestActor (Private/Tests/EnemyBaseTestActor.h), the existing
// concrete test-only subclass of the UCLASS(Abstract) AEnemyBase, and drives it
// through Idle->Alert via the private TickCheckDetection (friend-granted below,
// mirroring KrowdKontrolEnemyBaseTest.cpp's own approach) before calling the public
// ReceiveControl()/TransitionToBanked().
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the other
// KrowdKontrol.Unit.* tests.

#include "Misc/AutomationTest.h"
#include "EnemyBase.h"
#include "EnemyBaseTestActor.h"
#include "Herdable.h"
#include "AbilitySlot.h"
#include "AbilityData.h"
#include "ReservedGameplayColours.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolEnemyBaseHerdableTest,
	"KrowdKontrol.Unit.EnemyBaseHerdable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolEnemyBaseHerdableTest::RunTest(const FString& Parameters)
{
	static const FVector ZeroDistanceLocation = FVector::ZeroVector;

	AEnemyBaseTestActor* Enemy = NewObject<AEnemyBaseTestActor>();
	if (!TestNotNull(TEXT("AEnemyBaseTestActor should construct"), Enemy))
	{
		return false;
	}

	TestTrue(TEXT("AEnemyBase should implement UHerdable"), Enemy->Implements<UHerdable>());

	IHerdable* Herdable = Cast<IHerdable>(Enemy);
	if (!TestNotNull(TEXT("AEnemyBase should be castable to IHerdable"), Herdable))
	{
		return false;
	}

	TestFalse(TEXT("A freshly-constructed enemy (Idle) should not be IsControlled()"),
		Herdable->IsControlled());

	Enemy->ReceiveControl(EAbilitySlot::Root); // no-op from Idle - state guard smoke check
	TestFalse(TEXT("ReceiveControl from Idle should be a no-op"), Herdable->IsControlled());

	// Drive Idle -> Alert via the friend-granted private TickCheckDetection, then
	// ReceiveControl (public) into Controlled - same shape KrowdKontrolEnemyBaseTest.cpp
	// already uses to reach Controlled.
	Enemy->TickCheckDetection(ZeroDistanceLocation); // Idle -> Alert
	Enemy->ReceiveControl(EAbilitySlot::Snare); // Alert -> Controlled
	TestTrue(TEXT("IsControlled should report true once Controlled by Snare"), Herdable->IsControlled());
	TestEqual(TEXT("GetHerdColourTag should report Snare's ColourTag (Purple) while Controlled"),
		Herdable->GetHerdColourTag(), ReservedGameplayColours::GetPurpleTag());

	Enemy->TransitionToBanked(); // Controlled -> Banked
	TestFalse(TEXT("IsControlled should report false once Banked"), Herdable->IsControlled());
	TestEqual(TEXT("GetHerdColourTag should still report the last controlling ability's tag after Banked (stale-read contract)"),
		Herdable->GetHerdColourTag(), ReservedGameplayColours::GetPurpleTag());

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
