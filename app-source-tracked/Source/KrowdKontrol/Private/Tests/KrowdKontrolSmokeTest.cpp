// Proves the headless Automation Framework pipeline works end to end: written,
// discovered, run via UnrealEditor-Cmd.exe -ExecCmds="Automation RunTests ...",
// reported pass/fail in a form harness/run_ue_automation.sh can parse.
//
// Intentionally trivial — this is infrastructure, not gameplay coverage. Real
// gameplay unit tests (ability-matching logic, punishment triggers, etc. — see
// MISSION.md's Core Capabilities) get added under this same Tests/ directory as that
// code lands, named KrowdKontrol.Unit.* to distinguish them from this smoke group.
//
// #if-guarded so this compiles out of Shipping/packaged builds entirely — automation
// tests have no business in a build a player downloads.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolSmokeTest,
	"KrowdKontrol.Smoke.PipelineIsAlive",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::SmokeFilter)

bool FKrowdKontrolSmokeTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("The KrowdKontrol module's automation pipeline runs"), true);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
