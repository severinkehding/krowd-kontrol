// Direct closure of issue #20's final rescoping comment: proves UOvercrowdAudioSubsystem
// (issue #38) and UOvercrowdVisualEffectSubsystem (issue #20) engage/disengage together
// off the exact same UOvercrowdDetectionComponent::OnPanicOverloadStateChanged
// trigger/recovery events - one real activation, one simulated recovery, both subsystems
// asserted in the same test off the same calls.
//
// Does not re-test either subsystem's own internals (covered by
// KrowdKontrolOvercrowdAudioSubsystemTest.cpp and
// KrowdKontrolOvercrowdVisualEffectSubsystemTest.cpp respectively) - only that they stay
// in sync with each other.
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the other
// KrowdKontrol.Unit.* tests.

#include "Misc/AutomationTest.h"
#include "OvercrowdAudioSubsystem.h"
#include "OvercrowdVisualEffectSubsystem.h"
#include "CameraModifier_OvercrowdDistortion.h"
#include "OvercrowdDistortionStateTestListener.h"
#include "OvercrowdDetectionComponent.h"
#include "EnemyBaseTestActor.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolOvercrowdAudioVisualSyncTest,
	"KrowdKontrol.Unit.OvercrowdAudioVisualSync",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolOvercrowdAudioVisualSyncTest::RunTest(const FString& Parameters)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
	{
		return false;
	}

	UOvercrowdAudioSubsystem* AudioSubsystem = World->GetSubsystem<UOvercrowdAudioSubsystem>();
	UOvercrowdVisualEffectSubsystem* VisualSubsystem = World->GetSubsystem<UOvercrowdVisualEffectSubsystem>();
	if (!TestNotNull(TEXT("UWorld should auto-instantiate UOvercrowdAudioSubsystem"), AudioSubsystem)
		|| !TestNotNull(TEXT("UWorld should auto-instantiate UOvercrowdVisualEffectSubsystem"), VisualSubsystem))
	{
		return false;
	}

	// Bootstrap: CreateNewMap() worlds never run InitializeActorsForPlay/BeginPlay, so
	// AController::PostInitializeComponents() never fires - which means
	// APlayerController::SpawnPlayerCameraManager() never runs either, leaving
	// PlayerCameraManager null unless spawned explicitly here (see this issue's plan
	// Test Bootstrap Gotcha section, and KrowdKontrolEnemyBaseTest.cpp:398-428 for the
	// World->AddController() half of this same gotcha).
	APawn* PlayerPawn = World->SpawnActor<APawn>();
	if (!TestNotNull(TEXT("APawn should spawn into the test World"), PlayerPawn))
	{
		return false;
	}
	APlayerController* Controller = World->SpawnActor<APlayerController>();
	if (!TestNotNull(TEXT("APlayerController should spawn into the test World"), Controller))
	{
		return false;
	}
	Controller->Possess(PlayerPawn);
	World->AddController(Controller);
	Controller->SpawnPlayerCameraManager();

	UOvercrowdDetectionComponent* OvercrowdComponent = NewObject<UOvercrowdDetectionComponent>(PlayerPawn);
	OvercrowdComponent->RegisterComponent();

	if (!TestTrue(TEXT("AudioSubsystem should bind to the UOvercrowdDetectionComponent"), AudioSubsystem->TryBindOvercrowdComponent())
		|| !TestTrue(TEXT("VisualSubsystem should bind to the UOvercrowdDetectionComponent"), VisualSubsystem->TryBindOvercrowdComponent())
		|| !TestTrue(TEXT("VisualSubsystem should bind to the PlayerCameraManager"), VisualSubsystem->TryBindCameraManager()))
	{
		return false;
	}

	// One real activation - enough hot-and-uncontrolled enemies, sustained for the full
	// duration - drives OvercrowdComponent to Active via exactly one
	// AdvancePanicOverloadState call. Both subsystems must flip off that single call.
	for (int32 Index = 0; Index < OvercrowdComponent->OvercrowdCrowdThreshold; ++Index)
	{
		AEnemyBaseTestActor* Enemy = World->SpawnActor<AEnemyBaseTestActor>();
		if (!TestNotNull(TEXT("AEnemyBaseTestActor should spawn"), Enemy))
		{
			return false;
		}
		Enemy->TickCheckDetection(FVector::ZeroVector); // Idle -> Alert
	}
	OvercrowdComponent->AdvancePanicOverloadState(OvercrowdComponent->OvercrowdUncontrolledDurationSeconds + 10.0f);
	TestEqual(TEXT("Real Overcrowd activation should flip the panic overload component to Active"),
		static_cast<uint8>(OvercrowdComponent->GetPanicOverloadState()), static_cast<uint8>(EPanicOverloadState::Active));
	TestEqual(TEXT("One AdvancePanicOverloadState call should flip audio to Muffled"),
		static_cast<uint8>(AudioSubsystem->GetMuffleState()), static_cast<uint8>(EOvercrowdAudioMuffleState::Muffled));
	TestEqual(TEXT("The same call should flip visual distortion to Distorted"),
		static_cast<uint8>(VisualSubsystem->GetVisualDistortionState()), static_cast<uint8>(EOvercrowdVisualDistortionState::Distorted));

	// One simulated recovery broadcast - same documented precedent as both subsystems'
	// own tests (Active -> Inactive is out of UOvercrowdDetectionComponent's own scope
	// today, issue #18) - must clear both subsystems together.
	OvercrowdComponent->OnPanicOverloadStateChanged.Broadcast(EPanicOverloadState::Inactive);
	TestEqual(TEXT("The simulated recovery broadcast should clear audio to Clear"),
		static_cast<uint8>(AudioSubsystem->GetMuffleState()), static_cast<uint8>(EOvercrowdAudioMuffleState::Clear));
	TestEqual(TEXT("The same broadcast should clear visual distortion to Clear"),
		static_cast<uint8>(VisualSubsystem->GetVisualDistortionState()), static_cast<uint8>(EOvercrowdVisualDistortionState::Clear));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
