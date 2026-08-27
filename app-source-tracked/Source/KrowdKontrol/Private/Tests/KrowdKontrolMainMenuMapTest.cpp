// Confirms issue #323's full wiring end-to-end: the project's GameDefaultMap ini
// setting resolves to /Game/Maps/L_MainMenu (AC #2), and the map's WorldSettings
// override resolves to AMainMenuGameMode (AC #4) - the class that, per issue #324's
// already-built wiring, is what makes AMainMenuPlayerController::BeginPlay() show
// UMainMenuWidget on launch, though that runtime behavior itself is not exercised
// here (no BeginPlay is dispatched - see the acceptance-criteria table in this
// issue's changelog for the still-manual verification step). The map contains no
// gameplay actors (AC #1: no AEnemyBase, no ATargetZone, no pawn carrying
// UPlayerEnergyComponent), and the Editor Startup Map is left unchanged at
// L_Level01 (AC #3) - all four checks are only verifiable against the real
// .umap/.ini content, which is excluded from app-source-tracked/ per D-009, so
// this is the only in-diff, verifiable proof any of this actually landed correctly.
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the other
// KrowdKontrol.Unit.* tests.

#include "Misc/AutomationTest.h"
#include "MainMenuGameMode.h"
#include "GameMapsSettings.h"
#include "EnemyBase.h"
#include "TargetZone.h"
#include "PlayerEnergyComponent.h"
#include "Tests/AutomationEditorCommon.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Editor.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FKrowdKontrolMainMenuMapTest,
    "KrowdKontrol.Unit.MainMenuMapWiring",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

template <typename T>
static bool HasActorOfClass(UWorld* World)
{
    for (TActorIterator<T> It(World); It; ++It)
    {
        return true;
    }
    return false;
}

bool FKrowdKontrolMainMenuMapTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("Project's GameDefaultMap should resolve to L_MainMenu"),
        UGameMapsSettings::GetGameDefaultMap(), FString(TEXT("/Game/Maps/L_MainMenu")));

#if WITH_EDITORONLY_DATA
    TestEqual(TEXT("EditorStartupMap should remain L_Level01 (issue #323 must not disturb it)"),
        GetDefault<UGameMapsSettings>()->EditorStartupMap.GetLongPackageName(),
        FString(TEXT("/Game/Maps/L_Level01")));
#endif

    FAutomationEditorCommonUtils::LoadMap(TEXT("/Game/Maps/L_MainMenu"));
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("L_MainMenu should load into a valid World"), World))
    {
        return false;
    }

    AWorldSettings* WorldSettings = World->GetWorldSettings();
    if (!TestNotNull(TEXT("L_MainMenu should have WorldSettings"), WorldSettings))
    {
        return false;
    }
    TestEqual(TEXT("L_MainMenu's WorldSettings should override GameMode to AMainMenuGameMode"),
        WorldSettings->DefaultGameMode, TSubclassOf<AGameModeBase>(AMainMenuGameMode::StaticClass()));

    TestFalse(TEXT("L_MainMenu should contain no enemy actors (AC #1)"), HasActorOfClass<AEnemyBase>(World));
    TestFalse(TEXT("L_MainMenu should contain no target zone actors (AC #1)"), HasActorOfClass<ATargetZone>(World));

    bool bHasPlayerControlledPawn = false;
    for (TActorIterator<APawn> It(World); It; ++It)
    {
        if (It->FindComponentByClass<UPlayerEnergyComponent>())
        {
            bHasPlayerControlledPawn = true;
            break;
        }
    }
    TestFalse(TEXT("L_MainMenu should contain no player-controlled robot pawn (AC #1)"), bHasPlayerControlledPawn);

    return true;
}

// Pass-1 review follow-up: AC #3 ("existing gameplay maps continue to work with PIE
// exactly as before") was only asserted via EditorStartupMap's static ini value
// above, not by actually loading a gameplay level after this PR's GameDefaultMap
// change. This doesn't re-run KrowdKontrolLevel01Test.cpp's full structural checks
// (room/door/enemy counts) - that coverage already exists and isn't this PR's to
// duplicate - it only proves L_Level01 still loads into a valid World, which is the
// part this PR's own ini edit could plausibly have broken.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FKrowdKontrolMainMenuDoesNotBreakExistingLevelLoadTest,
    "KrowdKontrol.Unit.MainMenuMapDoesNotBreakExistingLevelLoad",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolMainMenuDoesNotBreakExistingLevelLoadTest::RunTest(const FString& Parameters)
{
    FAutomationEditorCommonUtils::LoadMap(TEXT("/Game/Maps/L_Level01"));
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    return TestNotNull(TEXT("L_Level01 should still load into a valid World after this PR's GameDefaultMap change (AC #3)"), World);
}

#endif // WITH_DEV_AUTOMATION_TESTS
