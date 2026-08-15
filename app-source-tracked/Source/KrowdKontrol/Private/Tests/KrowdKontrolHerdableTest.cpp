// Confirms IHerdable's minimal data contract (issue #79, PRD 01 REQ-2): a
// test-only actor implementing the interface reports IsControlled() /
// GetHerdColourTag() correctly as its underlying state is toggled, and is
// queryable both via Implements<UHerdable>() and Cast<IHerdable>(). No CC
// effect, enemy AI, or colour-rendering logic is exercised here - this is a
// contract test, not a gameplay test.
//
// Uses NewObject rather than spawning into a UWorld: both accessors are pure
// with no world dependency, same rationale as KrowdKontrolThreatStateTest.cpp.
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the
// other KrowdKontrol.Unit.* tests.

#include "Misc/AutomationTest.h"
#include "Herdable.h"
#include "HerdableTestActor.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolHerdableTest,
	"KrowdKontrol.Unit.Herdable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolHerdableTest::RunTest(const FString& Parameters)
{
	AHerdableTestActor* Actor = NewObject<AHerdableTestActor>();
	if (!TestNotNull(TEXT("AHerdableTestActor should construct"), Actor))
	{
		return false;
	}

	TestTrue(TEXT("AHerdableTestActor should implement UHerdable"), Actor->Implements<UHerdable>());

	IHerdable* Interface = Cast<IHerdable>(Actor);
	if (!TestNotNull(TEXT("AHerdableTestActor should be castable to IHerdable"), Interface))
	{
		return false;
	}

	TestFalse(TEXT("Default IsControlled should be false"), Actor->IsControlled());
	TestEqual(TEXT("Default GetHerdColourTag should be NAME_None"), Actor->GetHerdColourTag(), FName(NAME_None));

	Actor->SetControlled(true);
	Actor->SetHerdColourTag(FName(TEXT("Purple")));
	TestTrue(TEXT("IsControlled should report true after toggling"), Actor->IsControlled());
	TestEqual(TEXT("GetHerdColourTag should report Purple after toggling"), Actor->GetHerdColourTag(), FName(TEXT("Purple")));
	TestTrue(TEXT("Interface pointer should see the same controlled state as the concrete type"), Interface->IsControlled());
	TestEqual(TEXT("Interface pointer should see the same colour tag as the concrete type"), Interface->GetHerdColourTag(), FName(TEXT("Purple")));

	Actor->SetControlled(false);
	TestFalse(TEXT("IsControlled should report false after toggling back"), Actor->IsControlled());

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
