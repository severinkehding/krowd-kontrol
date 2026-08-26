// Confirms the boss-checkpoint acceptance criteria of issue #173 (PRD "Run Lifecycle
// & Progression Signals" REQ-4 boss-checkpoint sub-requirement) that is genuinely
// testable in-process: ULevelLifecycleSubsystem::RefreshBossCheckpointState() actually
// latches HasReachedBossCheckpoint() from a real ABossBase state transition (not
// simulated via friend access), AKrowdKontrolPlayerController::ComputeRestartOptions()
// returns an empty string until that latch fires and "BossCheckpoint" once it has (with
// the returned string proven to round-trip through FURL::HasOption() the same way the
// reader checks it), and ApplyBossCheckpointIfRequested() teleports the pawn to the
// boss's location only when that option is present on the world's URL - while
// confirming the already-tested issue #172 restart-triggering behavior
// (bRestartRequested flipping true via a real OnLevelFailed fire) is unaffected either
// way.
//
// Only the actual UGameplayStatics::OpenLevel() reload itself is NOT asserted here -
// same CreateNewMap()-World-is-not-a-game-world limitation KrowdKontrolLevelRestartTest.cpp
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
#include "BossBaseTestActor.h"
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

		ABossBaseTestActor* Boss = World->SpawnActor<ABossBaseTestActor>();
		if (!TestNotNull(TEXT("ABossBaseTestActor should spawn into the test World"), Boss))
		{
			return false;
		}

		LifecycleSubsystem->RefreshBossCheckpointState();
		TestFalse(TEXT("HasReachedBossCheckpoint should stay false while the boss is still Idle"),
			LifecycleSubsystem->HasReachedBossCheckpoint());

		Boss->AdvanceToArmed(); // Idle -> Armed
		LifecycleSubsystem->RefreshBossCheckpointState();
		TestTrue(TEXT("HasReachedBossCheckpoint should latch once a real boss leaves Idle"),
			LifecycleSubsystem->HasReachedBossCheckpoint());

		UPlayerEnergyComponent* Energy = Pawn->FindComponentByClass<UPlayerEnergyComponent>();
		if (!TestNotNull(TEXT("Pawn should have a PlayerEnergyComponent"), Energy))
		{
			return false;
		}

		TestEqual(TEXT("ComputeRestartOptions should report BossCheckpoint once the checkpoint is latched"),
			Controller->ComputeRestartOptions(), FString(TEXT("BossCheckpoint")));

		// Writer/reader round-trip: prove ComputeRestartOptions()'s output is actually
		// read back the same way ApplyBossCheckpointIfRequested() reads it via
		// FURL::HasOption(), not just that both sides independently match a hardcoded
		// literal. Built via the default constructor + AddOption() (the same primitive
		// FURL's own text parser calls per "?option" token) rather than parsing a full
		// "Map?Options" string - a text-parsing FURL constructor resolves the map
		// segment against the asset registry and resets the whole URL (wiping Op) if
		// that name isn't a real package, which a placeholder map name here isn't.
		const FString OptionsString = Controller->ComputeRestartOptions();
		FURL SimulatedReloadURL;
		SimulatedReloadURL.AddOption(*OptionsString);
		TestTrue(TEXT("ComputeRestartOptions()'s output should round-trip through FURL::HasOption the same way ApplyBossCheckpointIfRequested reads it"),
			SimulatedReloadURL.HasOption(TEXT("BossCheckpoint")));

		Energy->CurrentEnergy = 5.0f;
		Energy->ApplyContactDamage(10.0f, nullptr);

		TestTrue(TEXT("bRestartRequested should flip true after a real OnLevelFailed fire"),
			Controller->WasRestartRequested());
		TestEqual(TEXT("ComputeRestartOptions should still report BossCheckpoint after the fail"),
			Controller->ComputeRestartOptions(), FString(TEXT("BossCheckpoint")));
	}

	// --- Case C: ApplyBossCheckpointIfRequested() actually teleports the pawn to the
	// boss's location when the URL option is present, and is a no-op when it isn't -
	// exercised directly against the function, no real OpenLevel() involved.
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

		const FVector StartLocation(100.0f, 0.0f, 0.0f);
		Pawn->SetActorLocation(StartLocation);

		const FVector BossLocation(500.0f, 250.0f, 0.0f);
		ABossBaseTestActor* Boss = World->SpawnActor<ABossBaseTestActor>();
		if (!TestNotNull(TEXT("ABossBaseTestActor should spawn"), Boss))
		{
			return false;
		}
		Boss->SetActorLocation(BossLocation);

		// Option absent - must be a no-op.
		Controller->ApplyBossCheckpointIfRequested(Pawn);
		TestEqual(TEXT("Pawn should not move when the BossCheckpoint option is absent"),
			Pawn->GetActorLocation(), StartLocation);

		// Option present - pawn should teleport to the boss's location.
		World->URL.Op.Add(TEXT("BossCheckpoint"));
		Controller->ApplyBossCheckpointIfRequested(Pawn);
		TestEqual(TEXT("Pawn should teleport to the boss's location when BossCheckpoint is set"),
			Pawn->GetActorLocation(), BossLocation);
	}

	// --- Case D: BeginPlay()'s already-possessed branch actually applies the teleport
	// through the real production call site (not a direct call, unlike Case C) -
	// mirrors KrowdKontrolCrowdMasteryBeginPlayWiringTest.cpp's precedent for exactly
	// this "handler tested directly but never through its real wiring" gap shape.
	{
		UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
		if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
		{
			return false;
		}
		World->InitializeActorsForPlay(FURL());

		const FVector BossLocation(500.0f, 250.0f, 0.0f);
		ABossBaseTestActor* Boss = World->SpawnActor<ABossBaseTestActor>();
		if (!TestNotNull(TEXT("ABossBaseTestActor should spawn"), Boss))
		{
			return false;
		}
		Boss->SetActorLocation(BossLocation);

		AFlatCamera3DPrototypePawn* Pawn = World->SpawnActor<AFlatCamera3DPrototypePawn>();
		if (!TestNotNull(TEXT("Pawn should spawn"), Pawn))
		{
			return false;
		}
		Pawn->SetActorLocation(FVector(100.0f, 0.0f, 0.0f));

		// Simulate a post-restart world: the option is already on the URL before the
		// controller ever runs BeginPlay, same as a real OpenLevel() reload would set it.
		World->URL.Op.Add(TEXT("BossCheckpoint"));

		AKrowdKontrolPlayerController* Controller = World->SpawnActor<AKrowdKontrolPlayerController>();
		if (!TestNotNull(TEXT("Controller should spawn"), Controller))
		{
			return false;
		}

		// Possess BEFORE DispatchBeginPlay so GetPawn() is non-null inside BeginPlay,
		// exercising its already-possessed branch - the real production ordering when
		// AutoPossessPlayer runs ahead of BeginPlay.
		Controller->Possess(Pawn);
		Controller->Player = NewObject<ULocalPlayer>(GEngine);
		Controller->SetAsLocalPlayerController();
		Controller->DispatchBeginPlay();

		TestEqual(TEXT("BeginPlay's production wiring should teleport the pawn to the boss location"),
			Pawn->GetActorLocation(), BossLocation);
	}

	// --- Case E: OnPossess()'s call site applies the teleport when possession happens
	// after BeginPlay() has already run - the opposite ordering from Case D.
	{
		UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
		if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
		{
			return false;
		}
		World->InitializeActorsForPlay(FURL());
		World->URL.Op.Add(TEXT("BossCheckpoint"));

		const FVector BossLocation(500.0f, 250.0f, 0.0f);
		ABossBaseTestActor* Boss = World->SpawnActor<ABossBaseTestActor>();
		if (!TestNotNull(TEXT("ABossBaseTestActor should spawn"), Boss))
		{
			return false;
		}
		Boss->SetActorLocation(BossLocation);

		AFlatCamera3DPrototypePawn* Pawn = World->SpawnActor<AFlatCamera3DPrototypePawn>();
		if (!TestNotNull(TEXT("Pawn should spawn"), Pawn))
		{
			return false;
		}
		Pawn->SetActorLocation(FVector(100.0f, 0.0f, 0.0f));

		AKrowdKontrolPlayerController* Controller = World->SpawnActor<AKrowdKontrolPlayerController>();
		if (!TestNotNull(TEXT("Controller should spawn"), Controller))
		{
			return false;
		}
		Controller->Player = NewObject<ULocalPlayer>(GEngine);
		Controller->SetAsLocalPlayerController();
		Controller->DispatchBeginPlay(); // Unpossessed - BeginPlay's already-possessed branch no-ops.

		Controller->Possess(Pawn); // OnPossess()'s own call site must apply it.

		TestEqual(TEXT("OnPossess's production wiring should teleport the pawn to the boss location"),
			Pawn->GetActorLocation(), BossLocation);
	}

	// --- Case F: issue #342 - a fresh voluntary rerun after a boss checkpoint has
	// latched must ignore the checkpoint (empty reload options) and must not flip
	// bRestartRequested - that invariant is documented as defeat-path only. The
	// defeat-mode (bFreshRun=false, the default) restart must be completely unaffected
	// by this fix, so this case checks both modes against the same latched checkpoint.
	{
		UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
		if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
		{
			return false;
		}
		World->InitializeActorsForPlay(FURL());

		AKrowdKontrolPlayerController* Controller = World->SpawnActor<AKrowdKontrolPlayerController>();
		if (!TestNotNull(TEXT("Controller should spawn"), Controller))
		{
			return false;
		}
		Controller->Player = NewObject<ULocalPlayer>(GEngine);
		Controller->SetAsLocalPlayerController();
		Controller->DispatchBeginPlay();

		ULevelLifecycleSubsystem* LifecycleSubsystem = World->GetSubsystem<ULevelLifecycleSubsystem>();
		if (!TestNotNull(TEXT("UWorld should auto-instantiate ULevelLifecycleSubsystem"), LifecycleSubsystem))
		{
			return false;
		}

		ABossBaseTestActor* Boss = World->SpawnActor<ABossBaseTestActor>();
		if (!TestNotNull(TEXT("ABossBaseTestActor should spawn into the test World"), Boss))
		{
			return false;
		}
		Boss->AdvanceToArmed(); // Idle -> Armed
		LifecycleSubsystem->RefreshBossCheckpointState();
		TestTrue(TEXT("HasReachedBossCheckpoint should latch once a real boss leaves Idle"),
			LifecycleSubsystem->HasReachedBossCheckpoint());
		TestEqual(TEXT("Sanity: ComputeRestartOptions should report BossCheckpoint with the latch set"),
			Controller->ComputeRestartOptions(), FString(TEXT("BossCheckpoint")));

		TestFalse(TEXT("bRestartRequested should start false"), Controller->WasRestartRequested());

		Controller->RequestLevelRestart(/*bFreshRun=*/true);
		TestFalse(TEXT("A fresh-run restart must not flip bRestartRequested - that invariant is defeat-path only"),
			Controller->WasRestartRequested());
		TestTrue(TEXT("A fresh-run restart's mode should be recorded as fresh"),
			Controller->WasFreshRunRequested());

		Controller->RequestLevelRestart(); // bFreshRun defaults to false - the defeat path.
		TestTrue(TEXT("A defeat-mode restart should still flip bRestartRequested even with the checkpoint latched"),
			Controller->WasRestartRequested());
		TestFalse(TEXT("A defeat-mode restart's mode should be recorded as not fresh"),
			Controller->WasFreshRunRequested());
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
