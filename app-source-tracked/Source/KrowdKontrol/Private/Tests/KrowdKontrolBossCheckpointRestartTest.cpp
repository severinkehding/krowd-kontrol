// Confirms the boss-checkpoint acceptance criteria of issue #173 (PRD "Run Lifecycle
// & Progression Signals" REQ-4 boss-checkpoint sub-requirement) that is genuinely
// testable in-process: AKrowdKontrolPlayerController::ComputeRestartOptions() returns
// an empty string when this world's ULevelLifecycleSubsystem has not latched
// HasReachedBossCheckpoint(), and "BossCheckpoint" once it has - while confirming the
// already-tested issue #172 restart-triggering behavior (bRestartRequested flipping
// true via a real OnLevelFailed fire) is unaffected either way.
//
// The actual UGameplayStatics::OpenLevel() reload and the reloaded world's
// ApplyBossCheckpointIfRequested() teleport are NOT asserted here - same
// CreateNewMap()-World-is-not-a-game-world limitation KrowdKontrolLevelRestartTest.cpp
// documents for issue #172's own reload. Verified manually in PIE instead (see this
// issue's PR body).
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the other
// KrowdKontrol.Unit.* tests.

#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "KrowdKontrolPlayerController.h"
#include "LevelLifecycleSubsystem.h"
#include "LevelClearTimeSubsystem.h"
#include "LevelFailComponent.h"
#include "FlatCamera3DPrototypePawn.h"
#include "PlayerEnergyComponent.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolBossCheckpointRestartTest,
	"KrowdKontrol.Unit.BossCheckpointRestart",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolBossCheckpointRestartTest::RunTest(const FString& Parameters)
{
	// --- Case A: no boss checkpoint reached - restart should target empty options.
	{
		UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
		if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
		{
			return false;
		}

		World->InitializeActorsForPlay(FURL());

		AFlatCamera3DPrototypePawn* Pawn = World->SpawnActor<AFlatCamera3DPrototypePawn>();
		if (!TestNotNull(TEXT("Pawn should spawn"), Pawn))
		{
			return false;
		}

		AKrowdKontrolPlayerController* Controller = World->SpawnActor<AKrowdKontrolPlayerController>();
		if (!TestNotNull(TEXT("Controller should spawn"), Controller))
		{
			return false;
		}

		Controller->Possess(Pawn);
		Controller->Player = NewObject<ULocalPlayer>(GEngine);
		Controller->SetAsLocalPlayerController();
		Controller->DispatchBeginPlay();

		UGameInstance* GameInstanceOuter = NewObject<UGameInstance>();
		ULevelClearTimeSubsystem* Subsystem = NewObject<ULevelClearTimeSubsystem>(GameInstanceOuter);
		Controller->CachedLevelClearTimeSubsystem = Subsystem;

		UPlayerEnergyComponent* Energy = Pawn->FindComponentByClass<UPlayerEnergyComponent>();
		if (!TestNotNull(TEXT("Pawn should have a PlayerEnergyComponent"), Energy))
		{
			return false;
		}

		TestTrue(TEXT("ComputeRestartOptions should be empty before any boss checkpoint is reached"),
			Controller->ComputeRestartOptions().IsEmpty());

		Energy->CurrentEnergy = 5.0f;
		Energy->ApplyContactDamage(10.0f, nullptr);

		TestTrue(TEXT("bRestartRequested should flip true after a real OnLevelFailed fire"),
			Controller->WasRestartRequested());
		TestTrue(TEXT("ComputeRestartOptions should still be empty - no boss in this level"),
			Controller->ComputeRestartOptions().IsEmpty());
	}

	// --- Case B: boss checkpoint reached - restart should target "BossCheckpoint".
	{
		UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
		if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
		{
			return false;
		}

		World->InitializeActorsForPlay(FURL());

		AFlatCamera3DPrototypePawn* Pawn = World->SpawnActor<AFlatCamera3DPrototypePawn>();
		if (!TestNotNull(TEXT("Pawn should spawn"), Pawn))
		{
			return false;
		}

		AKrowdKontrolPlayerController* Controller = World->SpawnActor<AKrowdKontrolPlayerController>();
		if (!TestNotNull(TEXT("Controller should spawn"), Controller))
		{
			return false;
		}

		Controller->Possess(Pawn);
		Controller->Player = NewObject<ULocalPlayer>(GEngine);
		Controller->SetAsLocalPlayerController();
		Controller->DispatchBeginPlay();

		UGameInstance* GameInstanceOuter = NewObject<UGameInstance>();
		ULevelClearTimeSubsystem* ClearTimeSubsystem = NewObject<ULevelClearTimeSubsystem>(GameInstanceOuter);
		Controller->CachedLevelClearTimeSubsystem = ClearTimeSubsystem;

		ULevelLifecycleSubsystem* LifecycleSubsystem = World->GetSubsystem<ULevelLifecycleSubsystem>();
		if (!TestNotNull(TEXT("UWorld should auto-instantiate ULevelLifecycleSubsystem"), LifecycleSubsystem))
		{
			return false;
		}
		LifecycleSubsystem->bHasReachedBossCheckpoint = true; // friend access - simulate "boss reached"

		UPlayerEnergyComponent* Energy = Pawn->FindComponentByClass<UPlayerEnergyComponent>();
		if (!TestNotNull(TEXT("Pawn should have a PlayerEnergyComponent"), Energy))
		{
			return false;
		}

		TestEqual(TEXT("ComputeRestartOptions should report BossCheckpoint once the checkpoint is latched"),
			Controller->ComputeRestartOptions(), FString(TEXT("BossCheckpoint")));

		Energy->CurrentEnergy = 5.0f;
		Energy->ApplyContactDamage(10.0f, nullptr);

		TestTrue(TEXT("bRestartRequested should flip true after a real OnLevelFailed fire"),
			Controller->WasRestartRequested());
		TestEqual(TEXT("ComputeRestartOptions should still report BossCheckpoint after the fail"),
			Controller->ComputeRestartOptions(), FString(TEXT("BossCheckpoint")));
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
