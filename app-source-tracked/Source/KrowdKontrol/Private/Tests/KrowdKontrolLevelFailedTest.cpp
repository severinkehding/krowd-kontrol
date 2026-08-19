// Confirms the acceptance criteria of issue #171 (PRD "Run Lifecycle & Progression
// Signals" REQ-3): when the possessed pawn's UPlayerEnergyComponent reaches 0 energy,
// ULevelFailComponent::OnLevelFailed fires exactly once, the pawn's input is disabled
// via AKrowdKontrolPlayerController::HandleLevelFailed, and the level's in-progress
// clear timer is discarded (never recorded as a best) via
// ULevelClearTimeSubsystem::DiscardLevelTimer. Also confirms a further hit at
// already-0 energy does not re-fire OnLevelFailed.
//
// GetGameInstance() is null in this project's CreateNewMap()-based Automation test
// worlds (see KrowdKontrolLevelClearTimeSubsystemTest.cpp's own rationale), so a
// directly-constructed ULevelClearTimeSubsystem is injected into the controller's
// private CachedLevelClearTimeSubsystem via the FKrowdKontrolLevelFailedTest
// friendship, mirroring FKrowdKontrolGizmoFirstContactComponentTest's identical
// precedent.
//
// ULevelFailComponent::OnLevelFailed is a dynamic multicast delegate broadcast
// straight to an AActor target (AKrowdKontrolPlayerController::HandleLevelFailed) -
// that silently no-ops unless World->AreActorsInitialized() is true, since
// AActor::ProcessEvent gates reflection-dispatched calls on it. Same underlying
// "actor not initialized for play" gotcha KrowdKontrolDualZoneBossTest.cpp and
// KrowdKontrolMusicSubsystemTest.cpp already document; fixed the same way:
// World->InitializeActorsForPlay(FURL()) up front, before spawning any actors.
// SetBegunPlay(true) is intentionally NOT called here (matching
// KrowdKontrolDualZoneBossTest.cpp's rationale) - this test drives BeginPlay
// explicitly via DispatchBeginPlay() after possession.
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the other
// KrowdKontrol.Unit.* tests.

#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "KrowdKontrolPlayerController.h"
#include "LevelClearTimeSubsystem.h"
#include "LevelFailComponent.h"
#include "LevelFailedTestListener.h"
#include "FlatCamera3DPrototypePawn.h"
#include "PlayerEnergyComponent.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolLevelFailedTest,
	"KrowdKontrol.Unit.LevelFailed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolLevelFailedTest::RunTest(const FString& Parameters)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
	{
		return false;
	}

	// Required for ULevelFailComponent::OnLevelFailed's dynamic-delegate-bound
	// AActor handler (AKrowdKontrolPlayerController::HandleLevelFailed) to actually
	// fire on Broadcast() - see the file comment above.
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

	// GetGameInstance() is null in this CreateNewMap() World - inject a
	// directly-constructed subsystem via friendship, mirroring
	// FKrowdKontrolGizmoFirstContactComponentTest's identical precedent.
	UGameInstance* GameInstanceOuter = NewObject<UGameInstance>();
	ULevelClearTimeSubsystem* Subsystem = NewObject<ULevelClearTimeSubsystem>(GameInstanceOuter);
	Controller->CachedLevelClearTimeSubsystem = Subsystem;

	const FName LevelID = FName(*World->GetMapName());
	Subsystem->StartLevelTimer(LevelID);

	UPlayerEnergyComponent* Energy = Pawn->FindComponentByClass<UPlayerEnergyComponent>();
	if (!TestNotNull(TEXT("Pawn should have a PlayerEnergyComponent"), Energy))
	{
		return false;
	}

	ULevelFailComponent* LevelFailComp = Pawn->FindComponentByClass<ULevelFailComponent>();
	if (!TestNotNull(TEXT("Pawn should have a LevelFailComponent"), LevelFailComp))
	{
		return false;
	}

	ULevelFailedTestListener* Listener = NewObject<ULevelFailedTestListener>();
	LevelFailComp->OnLevelFailed.AddDynamic(Listener, &ULevelFailedTestListener::HandleLevelFailed);

	TestTrue(TEXT("Pawn input should start enabled"), Pawn->InputEnabled());

	// Deterministic single-call floor to exactly 0, mirroring
	// KrowdKontrolPlayerEnergyComponentTest.cpp's own case (c).
	Energy->CurrentEnergy = 5.0f;
	Energy->ApplyContactDamage(10.0f, nullptr);

	TestEqual(TEXT("OnLevelFailed should fire exactly once"), Listener->CallCount, 1);
	TestFalse(TEXT("Pawn input should be disabled after level failure"), Pawn->InputEnabled());

	float OutBest = 0.0f;
	TestFalse(TEXT("No best time should be recorded - the failed run's timer must be discarded, not recorded"),
		Subsystem->GetBestClearTimeSeconds(LevelID, OutBest));

	// A further hit at already-0 energy must not re-fire OnLevelFailed - OnEnergyChanged
	// itself does not re-fire at the floor (see PlayerEnergyComponentTest case (d)).
	Energy->ApplyContactDamage(10.0f, nullptr);
	TestEqual(TEXT("A further hit at 0 energy should not re-fire OnLevelFailed"), Listener->CallCount, 1);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
