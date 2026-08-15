// Confirms IThreatState's minimal data contract (issue #81, PRD 01 REQ-1): a
// test-only actor implementing the interface reports GetThreatState() as
// Idle/Hot correctly as its underlying state is toggled. No AI, animation, or
// HUD logic is exercised here - this is a contract test, not a gameplay test.
//
// Uses NewObject rather than spawning into a UWorld: GetThreatState() is a pure
// accessor with no world dependency, same rationale as
// KrowdKontrolPlaceholderCubeActorTest.cpp.
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the
// other KrowdKontrol.Unit.* tests.

#include "Misc/AutomationTest.h"
#include "ThreatState.h"
#include "ThreatStateTestActor.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolThreatStateTest,
	"KrowdKontrol.Unit.ThreatState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolThreatStateTest::RunTest(const FString& Parameters)
{
	AThreatStateTestActor* Actor = NewObject<AThreatStateTestActor>();
	if (!TestNotNull(TEXT("AThreatStateTestActor should construct"), Actor))
	{
		return false;
	}

	IThreatState* Interface = Cast<IThreatState>(Actor);
	if (!TestNotNull(TEXT("AThreatStateTestActor should be castable to IThreatState"), Interface))
	{
		return false;
	}

	TestEqual(TEXT("Default threat state should be Idle"),
		static_cast<uint8>(Actor->GetThreatState()), static_cast<uint8>(EThreatState::Idle));

	Actor->SetThreatState(EThreatState::Hot);
	TestEqual(TEXT("GetThreatState should report Hot after toggling"),
		static_cast<uint8>(Actor->GetThreatState()), static_cast<uint8>(EThreatState::Hot));
	TestEqual(TEXT("Interface pointer should see the same Hot state as the concrete type"),
		static_cast<uint8>(Interface->GetThreatState()), static_cast<uint8>(EThreatState::Hot));

	Actor->SetThreatState(EThreatState::Idle);
	TestEqual(TEXT("GetThreatState should report Idle after toggling back"),
		static_cast<uint8>(Actor->GetThreatState()), static_cast<uint8>(EThreatState::Idle));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
