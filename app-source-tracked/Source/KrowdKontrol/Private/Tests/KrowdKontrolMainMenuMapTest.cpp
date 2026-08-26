// Confirms issue #323's full wiring end-to-end: the project's GameDefaultMap ini
// setting resolves to /Game/Maps/L_MainMenu (AC #2), the map loads and its
// WorldSettings override resolves to AMainMenuGameMode (so AMainMenuPlayerController
// ::BeginPlay() actually shows UMainMenuWidget on launch - issue #324's already-built
// wiring), the map contains no gameplay actors (AC #1: no AEnemyBase, no ATargetZone,
// no pawn carrying UPlayerEnergyComponent), and the Editor Startup Map is left
// unchanged at L_Level01 (AC #3) - all four are only verifiable against the real
// .umap/.ini content, which is excluded from app-source-tracked/ per D-009, so this
// is the only in-diff, verifiable proof any of this actually landed correctly.
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
    TestTrue(TEXT("L_MainMenu's WorldSettings should override GameMode to AMainMenuGameMode"),
        WorldSettings->DefaultGameMode == AMainMenuGameMode::StaticClass());

    bool bHasEnemy = false;
    for (TActorIterator<AEnemyBase> It(World); It; ++It) { bHasEnemy = true; break; }
    TestFalse(TEXT("L_MainMenu should contain no enemy actors (AC #1)"), bHasEnemy);

    bool bHasTargetZone = false;
    for (TActorIterator<ATargetZone> It(World); It; ++It) { bHasTargetZone = true; break; }
    TestFalse(TEXT("L_MainMenu should contain no target zone actors (AC #1)"), bHasTargetZone);

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

#endif // WITH_DEV_AUTOMATION_TESTS
