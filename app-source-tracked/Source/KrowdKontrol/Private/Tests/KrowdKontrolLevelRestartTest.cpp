// Confirms the acceptance criteria of issue #172 (PRD "Run Lifecycle & Progression
// Signals" REQ-4) that is genuinely testable in-process: a real
// ULevelFailComponent::OnLevelFailed firing (via a deterministic zero-energy hit)
// drives AKrowdKontrolPlayerController::HandleLevelFailed to call
// RequestLevelRestart(), which flips bRestartRequested (exposed read-only via
// WasRestartRequested()) from false to true.
//
// The actual UGameplayStatics::OpenLevel() map reload is NOT asserted here -
// RequestLevelRestart() only issues it when World->IsGameWorld() is true, and this
// test's CreateNewMap() World deliberately is not one (confirmed via research: a real
// map load hangs an in-process Automation run - see web-research.md). That guard, and
// the resulting "full energy / enemy reset" behavior a fresh OpenLevel produces, is
// verified manually in PIE and documented in this issue's PR body instead.
//
// GetGameInstance() is null in this project's CreateNewMap()-based Automation test
// worlds, so a directly-constructed ULevelClearTimeSubsystem is injected into the
// controller's private CachedLevelClearTimeSubsystem via the
// FKrowdKontrolLevelRestartTest friendship, mirroring
// KrowdKontrolLevelFailedTest.cpp's identical precedent - this keeps
// HandleLevelFailed's "no subsystem" warning out of this test entirely.
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the other
// KrowdKontrol.Unit.* tests.

#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "KrowdKontrolPlayerController.h"
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
	FKrowdKontrolLevelRestartTest,
	"KrowdKontrol.Unit.LevelRestart",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolLevelRestartTest::RunTest(const FString& Parameters)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
	{
		return false;
	}

	// Required for ULevelFailComponent::OnLevelFailed's dynamic-delegate-bound
	// AActor handler (AKrowdKontrolPlayerController::HandleLevelFailed) to actually
	// fire on Broadcast() - see KrowdKontrolLevelFailedTest.cpp's file comment.
	World->InitializeActorsForPlay(FURL());

	// A CreateNewMap() World is an Editor-type World, never a game world - this is
	// the precondition RequestLevelRestart()'s IsGameWorld() guard relies on to skip
	// the real OpenLevel() call here. Asserted up front so this test documents *why*
	// no assertion on the actual reload follows.
	TestFalse(TEXT("CreateNewMap() World should not be a game world"), World->IsGameWorld());

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
	// KrowdKontrolLevelFailedTest.cpp's identical precedent, so HandleLevelFailed's
	// "no subsystem" warning never fires here.
	UGameInstance* GameInstanceOuter = NewObject<UGameInstance>();
	ULevelClearTimeSubsystem* Subsystem = NewObject<ULevelClearTimeSubsystem>(GameInstanceOuter);
	Controller->CachedLevelClearTimeSubsystem = Subsystem;

	UPlayerEnergyComponent* Energy = Pawn->FindComponentByClass<UPlayerEnergyComponent>();
	if (!TestNotNull(TEXT("Pawn should have a PlayerEnergyComponent"), Energy))
	{
		return false;
	}

	TestFalse(TEXT("bRestartRequested should start false"), Controller->WasRestartRequested());

	// Deterministic single-call floor to exactly 0, mirroring
	// KrowdKontrolLevelFailedTest.cpp's own case - fires OnLevelFailed ->
	// HandleLevelFailed -> RequestLevelRestart.
	Energy->CurrentEnergy = 5.0f;
	Energy->ApplyContactDamage(10.0f, nullptr);

	TestTrue(TEXT("bRestartRequested should flip true after a real OnLevelFailed fire"),
		Controller->WasRestartRequested());

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
