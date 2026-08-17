// Confirms AKrowdKontrolGameMode (issue #132) actually points PlayerControllerClass at
// AKrowdKontrolPlayerController - the one line making the whole HUD-wiring feature
// reachable when GlobalDefaultGameMode selects this class. A reverted/typo'd assignment
// would silently fall back to the engine's default APlayerController with no HUD at all,
// and nothing else in the suite would catch it.
//
// Also confirms the map-to-GameMode wiring itself (PR #133 validation feedback):
// Config/DefaultEngine.ini's GlobalDefaultGameMode setting is excluded from
// app-source-tracked (see CLAUDE.md's D-009), so reading it back via
// UGameMapsSettings::GetGlobalDefaultGameMode() here is the only in-diff, verifiable
// proof that it actually selects AKrowdKontrolGameMode, not just that the class itself
// is well-formed.

#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "KrowdKontrolGameMode.h"
#include "KrowdKontrolPlayerController.h"
#include "GameMapsSettings.h"
#include "PlaceholderTargetZoneActor.h"
#include "GameFramework/WorldSettings.h"
#include "Editor.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolGameModeTest,
	"KrowdKontrol.Unit.GameModeSetsPlayerController",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolGameModeTest::RunTest(const FString& Parameters)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
	{
		return false;
	}

	AKrowdKontrolGameMode* GameMode = World->SpawnActor<AKrowdKontrolGameMode>();
	if (!TestNotNull(TEXT("GameMode should spawn"), GameMode))
	{
		return false;
	}

	TestEqual(TEXT("GameMode should point PlayerControllerClass at AKrowdKontrolPlayerController"),
		GameMode->PlayerControllerClass, TSubclassOf<APlayerController>(AKrowdKontrolPlayerController::StaticClass()));

	TestEqual(TEXT("Project's GlobalDefaultGameMode should resolve to AKrowdKontrolGameMode"),
		UGameMapsSettings::GetGlobalDefaultGameMode(), AKrowdKontrolGameMode::StaticClass()->GetPathName());

	return true;
}

// Confirms neither playable level's own World Settings silently overrides the project's
// GlobalDefaultGameMode (checked above) away from AKrowdKontrolGameMode. A per-level
// override lives in the .umap binary, so it wouldn't show up in app-source-tracked at
// all - this is the only in-diff, verifiable proof both levels actually get the
// HUD-wiring GameMode at runtime, closing the gap PR #133's validation pass flagged as
// unverifiable from the diff.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolGameModeLevelOverrideTest,
	"KrowdKontrol.Unit.GameModeLevelsDoNotOverrideGameMode",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolGameModeLevelOverrideTest::RunTest(const FString& Parameters)
{
	const TCHAR* MapPaths[] = { TEXT("/Game/Maps/L_FlatCamera3DPrototype"), TEXT("/Game/Maps/L_Paper2DPrototype") };

	for (const TCHAR* MapPath : MapPaths)
	{
		FAutomationEditorCommonUtils::LoadMap(MapPath);

		UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
		if (!TestNotNull(*FString::Printf(TEXT("%s should load into a valid World"), MapPath), World))
		{
			continue;
		}

		AWorldSettings* WorldSettings = World->GetWorldSettings();
		if (!TestNotNull(*FString::Printf(TEXT("%s should have WorldSettings"), MapPath), WorldSettings))
		{
			continue;
		}

		// No override (None) means the level falls through to GlobalDefaultGameMode,
		// already proven correct above - only an explicit override to some other class
		// would defeat the HUD-wiring feature on this level.
		TestTrue(*FString::Printf(TEXT("%s should not override GameMode away from AKrowdKontrolGameMode"), MapPath),
			!WorldSettings->DefaultGameMode || WorldSettings->DefaultGameMode == AKrowdKontrolGameMode::StaticClass());
	}

	return true;
}

// Beacon hook (issue #132's third scoped deliverable): RefreshTargetZoneBeacons()
// must collect exactly the live APlaceholderTargetZoneActor instances - starting
// empty in a bare test map, tracking spawns, and pruning removals on re-refresh.
// The controller is spawned directly (not via GameMode possession flow) because this
// harness never drives World->BeginPlay(); RefreshTargetZoneBeacons() is public and
// world-driven, so it is testable standalone - same friend-free pattern as the
// GameMode tests above.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolControllerBeaconHookTest,
	"KrowdKontrol.Unit.ControllerBeaconHook",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolControllerBeaconHookTest::RunTest(const FString& Parameters)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
	{
		return false;
	}

	AKrowdKontrolPlayerController* Controller = World->SpawnActor<AKrowdKontrolPlayerController>();
	if (!TestNotNull(TEXT("AKrowdKontrolPlayerController should spawn"), Controller))
	{
		return false;
	}

	// (a) empty world: refresh finds nothing.
	TestEqual(TEXT("Refresh in a beacon-less world should find 0"), Controller->RefreshTargetZoneBeacons(), 0);
	TestEqual(TEXT("TargetZoneBeacons should be empty"), Controller->TargetZoneBeacons.Num(), 0);

	// (b) two spawned beacons are both collected.
	APlaceholderTargetZoneActor* BeaconA = World->SpawnActor<APlaceholderTargetZoneActor>();
	APlaceholderTargetZoneActor* BeaconB = World->SpawnActor<APlaceholderTargetZoneActor>();
	if (!TestNotNull(TEXT("First beacon should spawn"), BeaconA) ||
		!TestNotNull(TEXT("Second beacon should spawn"), BeaconB))
	{
		return false;
	}
	TestEqual(TEXT("Refresh should find both spawned beacons"), Controller->RefreshTargetZoneBeacons(), 2);
	TestTrue(TEXT("TargetZoneBeacons should contain the first beacon"), Controller->TargetZoneBeacons.Contains(BeaconA));
	TestTrue(TEXT("TargetZoneBeacons should contain the second beacon"), Controller->TargetZoneBeacons.Contains(BeaconB));

	// (c) removal is pruned on re-refresh (destroyed actors must not linger).
	World->DestroyActor(BeaconA);
	TestEqual(TEXT("Refresh after destroying one beacon should find 1"), Controller->RefreshTargetZoneBeacons(), 1);
	TestTrue(TEXT("The surviving beacon should still be tracked"), Controller->TargetZoneBeacons.Contains(BeaconB));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
