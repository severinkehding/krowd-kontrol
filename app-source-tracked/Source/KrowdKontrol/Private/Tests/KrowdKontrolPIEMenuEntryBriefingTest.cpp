// Adds KrowdKontrol.PIE.MenuEntryBriefing (issue #356) - a regression test for the
// pre-level briefing card (issue #246/PR #272) no longer appearing when a level is
// entered via the main menu's real UGameplayStatics::OpenLevel() travel path
// (introduced by PR #346/issue #325), instead of a fresh PIE session booting
// directly into the level (EditorStartupMap=/Game/Maps/L_Level01).
//
// Investigation for #356 ruled out both the #235-style buffered-retry race and PR
// #309's AnyKey-dismiss input change against real engine source (World.cpp,
// UnrealEngine.cpp) - see docs/prd-mission-briefing-tracker.md and the issue's own
// investigation artifact. Neither mechanism differs between a fresh PIE load and an
// OpenLevel()-triggered travel. The leading, previously-unverified hypothesis is a
// content/asset gap: ULevelBriefingSubsystem::LevelBriefingTable is an
// EditDefaultsOnly reference set entirely outside Source/, and the real menu-click ->
// OpenLevel() -> OnLevelBegin path this bug reports was never exercised in a real PIE
// session before merge (PR #346 body flagged real click-through as untested).
//
// This test opens L_MainMenu (a genuine disk load, not the currently-open-editor-level
// PIE path), drives the real UMainMenuWidget::HandleLevelSelected() click handler
// (same real function a player's click reaches, per KrowdKontrolMainMenuLevelSelectTest.cpp's
// precedent for calling it directly since there is still no UMG click-simulation
// primitive in this environment), and asserts the destination AKrowdKontrolPlayerController's
// BriefingCardWidgetInstance is visible after the real OpenLevel() travel and
// OnLevelBegin fire. If this fails, the Output Log's presence/absence of
// ULevelBriefingSubsystem's "LevelBriefingTable is unset" / "no LevelBriefingTable row
// found for map" warnings (LevelBriefingSubsystem.cpp) distinguishes a content-authoring
// gap from a genuine code defect - see the investigation artifact's decision tree.
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the other
// KrowdKontrol.* automation tests.

#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"
#include "MainMenuPlayerController.h"
#include "MainMenuWidget.h"
#include "KrowdKontrolPlayerController.h"
#include "BriefingCardWidget.h"
#include "Kismet/GameplayStatics.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolPIEMenuEntryBriefingTest,
	"KrowdKontrol.PIE.MenuEntryBriefing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolPIEMenuEntryBriefingTest::RunTest(const FString& Parameters)
{
	// Genuine disk load of the menu map - NOT the currently-open-editor-level PIE
	// path (EditorStartupMap=L_Level01) - so this reproduces the same fresh-load
	// codepath a real player hits, per this issue's own working-vs-broken distinction.
	AutomationOpenMap(TEXT("/Game/Maps/L_MainMenu"));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForEngineFramesCommand(5));

	ADD_LATENT_AUTOMATION_COMMAND(FUntilCommand(
		[this]() -> bool
		{
			UWorld* PIEWorld = AutomationCommon::GetAnyGameWorld();
			AMainMenuPlayerController* MenuController = PIEWorld
				? Cast<AMainMenuPlayerController>(UGameplayStatics::GetPlayerController(PIEWorld, 0))
				: nullptr;
			if (!MenuController || !MenuController->MainMenuWidgetInstance)
			{
				return false;
			}
			// Real call through the real click handler (not a synthesized OpenLevel())
			// - exercises the exact function a player's click reaches, per this issue's
			// own reported path.
			MenuController->MainMenuWidgetInstance->HandleLevelSelected(TEXT("L_Level01"));
			return true;
		},
		[this]() -> bool { AddError(TEXT("Timed out waiting for the main menu to be ready")); return true; },
		10.0f));

	ADD_LATENT_AUTOMATION_COMMAND(FWaitForEngineFramesCommand(10));

	ADD_LATENT_AUTOMATION_COMMAND(FUntilCommand(
		[this]() -> bool
		{
			UWorld* PIEWorld = AutomationCommon::GetAnyGameWorld();
			AKrowdKontrolPlayerController* Controller = PIEWorld
				? Cast<AKrowdKontrolPlayerController>(UGameplayStatics::GetPlayerController(PIEWorld, 0))
				: nullptr;
			if (!Controller || !Controller->BriefingCardWidgetInstance)
			{
				return false;
			}
			TestTrue(TEXT("The briefing card should be visible after entering L_Level01 via the main menu"),
				Controller->BriefingCardWidgetInstance->IsBriefingVisible());
			return true;
		},
		[this]() -> bool { AddError(TEXT("Timed out waiting for L_Level01 to load after menu-driven OpenLevel()")); return true; },
		20.0f));

	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
