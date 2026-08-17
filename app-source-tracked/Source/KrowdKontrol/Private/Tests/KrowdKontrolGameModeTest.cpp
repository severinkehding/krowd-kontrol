// Confirms AKrowdKontrolGameMode (issue #132) actually points PlayerControllerClass at
// AKrowdKontrolPlayerController - the one line making the whole HUD-wiring feature
// reachable when GlobalDefaultGameMode selects this class. A reverted/typo'd assignment
// would silently fall back to the engine's default APlayerController with no HUD at all,
// and nothing else in the suite would catch it.

#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "KrowdKontrolGameMode.h"
#include "KrowdKontrolPlayerController.h"

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

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
