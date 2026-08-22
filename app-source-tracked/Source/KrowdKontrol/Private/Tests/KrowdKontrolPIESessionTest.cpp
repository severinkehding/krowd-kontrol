// Stands up the KrowdKontrol.PIE.* test group (PRD docs/prd-functional-pie-tests.md
// REQ-1, issue #236) - a tier distinct from Smoke/Unit whose tests drive a real
// in-editor PIE session (real begin-play, real subsystem ticks, real PIE map-name
// mangling) instead of the CreateNewMap()/LoadMap() worlds every existing
// KrowdKontrol.Unit.* test uses, which never start play. Every assertion below runs
// against a live PIE UWorld reached via AutomationOpenMap's own latent commands - no
// lifecycle method (OnWorldBeginPlay, RefreshLevelClearState, or equivalent) is ever
// called directly.
//
// This issue does not add any of the PRD's three scenario tests (REQ-3, tracked
// separately) - it proves the mechanism works with one minimal test that opens
// L_Level01, starts a real PIE session, asserts the PIE-mangled map name
// (UEDPIE_0_...) is present, pumps a few frames, and ends the session cleanly.
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the other
// KrowdKontrol.* automation tests.

#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"

#if WITH_DEV_AUTOMATION_TESTS

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FKrowdKontrolAssertPIESessionActiveCommand, FAutomationTestBase*, Test);

bool FKrowdKontrolAssertPIESessionActiveCommand::Update()
{
	UWorld* PIEWorld = AutomationCommon::GetAnyGameWorld();
	if (Test->TestNotNull(TEXT("A live PIE world should exist"), PIEWorld))
	{
		// UEDPIE_0_ mangling only appears once a real PIE session is running - the
		// same prefix issue #223's fix (AKrowdKontrolPlayerController::
		// StripPIEPrefixFromMapName) strips. Its presence here is the proof that this
		// is a genuine PIE world, not an editor/CreateNewMap() world.
		Test->TestTrue(TEXT("The PIE world's map name should carry the UEDPIE_ mangling prefix that only a real PIE session produces (issue #223)"),
			PIEWorld->GetMapName().StartsWith(TEXT("UEDPIE_")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolPIESessionStartsTest,
	"KrowdKontrol.PIE.RealSessionStartsAndEndsCleanly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolPIESessionStartsTest::RunTest(const FString& Parameters)
{
	// AutomationOpenMap, run via EditorContext automation (this project's
	// UnrealEditor-Cmd.exe path), delegates to UEditorEngine::AutomationLoadMap,
	// which loads the map and queues FStartPIEForAutomationCommand - the real
	// EPlaySessionWorldType::PlayInEditor session starter, which itself blocks (as a
	// latent command) until GameState->HasMatchStarted() is true. No manual
	// FStartPIECommand wiring is needed here.
	AutomationOpenMap(TEXT("/Game/Maps/L_Level01"));
	ADD_LATENT_AUTOMATION_COMMAND(FKrowdKontrolAssertPIESessionActiveCommand(this));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForEngineFramesCommand(5));
	ADD_LATENT_AUTOMATION_COMMAND(FKrowdKontrolAssertPIESessionActiveCommand(this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
