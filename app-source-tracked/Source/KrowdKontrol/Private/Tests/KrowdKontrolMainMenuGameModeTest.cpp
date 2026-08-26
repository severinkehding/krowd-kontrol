// Confirms AMainMenuGameMode (issue #324) points PlayerControllerClass at
// AMainMenuPlayerController - mirrors KrowdKontrolGameModeTest.cpp's
// FKrowdKontrolGameModeTest shape. Unlike that test, this one does NOT assert
// against UGameMapsSettings::GetGlobalDefaultGameMode() - AMainMenuGameMode is a
// per-map (WorldSettings) override, never the project's global default, so there is
// no ini-level assertion to make here; the WorldSettings assignment itself lives in
// a .umap binary and is outside app-source-tracked's diff (same documented gap as
// KrowdKontrolGameModeTest.cpp's own per-level-override test).

#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "MainMenuGameMode.h"
#include "MainMenuPlayerController.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolMainMenuGameModeTest,
	"KrowdKontrol.Unit.MainMenuGameModeSetsPlayerController",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolMainMenuGameModeTest::RunTest(const FString& Parameters)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
	{
		return false;
	}

	AMainMenuGameMode* GameMode = World->SpawnActor<AMainMenuGameMode>();
	if (!TestNotNull(TEXT("GameMode should spawn"), GameMode))
	{
		return false;
	}

	TestEqual(TEXT("GameMode should point PlayerControllerClass at AMainMenuPlayerController"),
		GameMode->PlayerControllerClass, TSubclassOf<APlayerController>(AMainMenuPlayerController::StaticClass()));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
