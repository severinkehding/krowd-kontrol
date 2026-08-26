// Confirms AMainMenuGameMode (issue #324) points PlayerControllerClass at
// AMainMenuPlayerController - mirrors KrowdKontrolGameModeTest.cpp's
// FKrowdKontrolGameModeTest shape. Unlike that test, this one does NOT assert
// against UGameMapsSettings::GetGlobalDefaultGameMode() - AMainMenuGameMode is a
// per-map (WorldSettings) override, never the project's global default, so there is
// no ini-level assertion to make here. The per-level WorldSettings override itself
// IS covered below (FKrowdKontrolMainMenuGameModeLevelOverrideTest), mirroring
// KrowdKontrolGameModeTest.cpp's own FKrowdKontrolGameModeLevelOverrideTest, which
// already proves this exact LoadMap()+GetWorldSettings()->DefaultGameMode pattern is
// usable from an automation test - there is no binary-diff limitation blocking it.

#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "MainMenuGameMode.h"
#include "MainMenuPlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "Editor.h"

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

// Confirms /Game/Maps/L_MainMenuTemp's WorldSettings actually overrides GameMode to
// AMainMenuGameMode - the real issue #323 hand-off mechanism this PR's changelog
// centers on, previously only checked by a one-off manual headless-pythonscript run.
// Mirrors KrowdKontrolGameModeTest.cpp's FKrowdKontrolGameModeLevelOverrideTest exactly.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolMainMenuGameModeLevelOverrideTest,
	"KrowdKontrol.Unit.MainMenuGameModeLevelOverride",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolMainMenuGameModeLevelOverrideTest::RunTest(const FString& Parameters)
{
	const TCHAR* MapPath = TEXT("/Game/Maps/L_MainMenuTemp");
	FAutomationEditorCommonUtils::LoadMap(MapPath);

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!TestNotNull(*FString::Printf(TEXT("%s should load into a valid World"), MapPath), World))
	{
		return false;
	}

	AWorldSettings* WorldSettings = World->GetWorldSettings();
	if (!TestNotNull(*FString::Printf(TEXT("%s should have WorldSettings"), MapPath), WorldSettings))
	{
		return false;
	}

	TestEqual(*FString::Printf(TEXT("%s should override GameMode to AMainMenuGameMode"), MapPath),
		WorldSettings->DefaultGameMode, TSubclassOf<AGameModeBase>(AMainMenuGameMode::StaticClass()));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
