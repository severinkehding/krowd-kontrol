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

	TestTrue(TEXT("Default threat state should be Idle"),
		Actor->GetThreatState() == EThreatState::Idle);

	Actor->SetThreatState(EThreatState::Hot);
	TestTrue(TEXT("GetThreatState should report Hot after toggling"),
		Actor->GetThreatState() == EThreatState::Hot);

	Actor->SetThreatState(EThreatState::Idle);
	TestTrue(TEXT("GetThreatState should report Idle after toggling back"),
		Actor->GetThreatState() == EThreatState::Idle);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
