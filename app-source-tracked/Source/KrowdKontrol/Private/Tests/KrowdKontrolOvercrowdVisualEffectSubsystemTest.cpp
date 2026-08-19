// Confirms UOvercrowdVisualEffectSubsystem (issue #20) toggles
// EOvercrowdVisualDistortionState between Clear and Distorted exactly in sync with
// UOvercrowdDetectionComponent::OnPanicOverloadStateChanged - activating on Active,
// deactivating on Inactive, never double-firing, binding to exactly one
// UOvercrowdDetectionComponent found in the world, and driving a real
// UCameraModifier_OvercrowdDistortion added to the player's PlayerCameraManager.
//
// OvercrowdDetectionComponent.h documents CurrentState as one-directional in its own
// scope today (Inactive -> Active only; Active -> Inactive recovery is deferred to a
// separate, later issue - see that header's comment). This test proves the Distorted ->
// Clear path by broadcasting Inactive directly on the component's own public
// OnPanicOverloadStateChanged delegate (a plain UPROPERTY(BlueprintAssignable), callable
// with no friend access) rather than driving a real recovery transition, since no code
// path that produces one exists yet - same documented scope choice
// KrowdKontrolOvercrowdAudioSubsystemTest.cpp's precedent makes.
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the other
// KrowdKontrol.Unit.* tests.

#include "Misc/AutomationTest.h"
#include "OvercrowdVisualEffectSubsystem.h"
#include "CameraModifier_OvercrowdDistortion.h"
#include "OvercrowdDistortionStateTestListener.h"
#include "OvercrowdDetectionComponent.h"
#include "EnemyBaseTestActor.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Camera/CameraTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolOvercrowdVisualEffectSubsystemTest,
	"KrowdKontrol.Unit.OvercrowdVisualEffectSubsystem",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolOvercrowdVisualEffectSubsystemTest::RunTest(const FString& Parameters)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
	{
		return false;
	}

	UOvercrowdVisualEffectSubsystem* VisualSubsystem = World->GetSubsystem<UOvercrowdVisualEffectSubsystem>();
	if (!TestNotNull(TEXT("UWorld should auto-instantiate UOvercrowdVisualEffectSubsystem"), VisualSubsystem))
	{
		return false;
	}

	UOvercrowdDistortionStateTestListener* Listener = NewObject<UOvercrowdDistortionStateTestListener>();
	VisualSubsystem->OnOvercrowdVisualDistortionStateChanged.AddDynamic(Listener, &UOvercrowdDistortionStateTestListener::HandleOvercrowdVisualDistortionStateChanged);

	// (a) default state, before any pawn/component/controller exists or any bind is attempted.
	TestEqual(TEXT("Default visual distortion state should be Clear"),
		static_cast<uint8>(VisualSubsystem->GetVisualDistortionState()), static_cast<uint8>(EOvercrowdVisualDistortionState::Clear));

	// (b) both binds fail gracefully (return false, no crash) with nothing in the world yet.
	TestFalse(TEXT("TryBindOvercrowdComponent should fail gracefully with no UOvercrowdDetectionComponent in the world"),
		VisualSubsystem->TryBindOvercrowdComponent());
	TestFalse(TEXT("TryBindCameraManager should fail gracefully with no PlayerController/PlayerCameraManager in the world"),
		VisualSubsystem->TryBindCameraManager());

	// Bootstrap: CreateNewMap() worlds never run InitializeActorsForPlay/BeginPlay, so
	// AController::PostInitializeComponents() never fires - which means
	// APlayerController::SpawnPlayerCameraManager() (itself called from
	// PostInitializeComponents) never runs either, leaving PlayerCameraManager null
	// unless spawned explicitly here (see this issue's plan Test Bootstrap Gotcha
	// section, and KrowdKontrolEnemyBaseTest.cpp:398-428 for the World->AddController()
	// half of this same gotcha).
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

	// (c) camera-manager binding now succeeds exactly once, producing a real modifier.
	TestTrue(TEXT("TryBindCameraManager should succeed once a PlayerCameraManager exists"),
		VisualSubsystem->TryBindCameraManager());
	UCameraModifier_OvercrowdDistortion* DistortionModifier = VisualSubsystem->DistortionModifier;
	if (!TestNotNull(TEXT("TryBindCameraManager should have created DistortionModifier"), DistortionModifier))
	{
		return false;
	}
	TestEqual(TEXT("EaseInSeconds should be copied onto the modifier"), DistortionModifier->EaseInSeconds, VisualSubsystem->DistortionEaseInSeconds);
	TestEqual(TEXT("EaseOutSeconds should be copied onto the modifier"), DistortionModifier->EaseOutSeconds, VisualSubsystem->DistortionEaseOutSeconds);
	TestEqual(TEXT("EaseExponent should be copied onto the modifier"), DistortionModifier->EaseExponent, VisualSubsystem->DistortionEaseExponent);
	TestEqual(TEXT("MaxSceneFringeIntensity should be copied onto the modifier"), DistortionModifier->MaxSceneFringeIntensity, VisualSubsystem->MaxSceneFringeIntensity);
	TestEqual(TEXT("MaxVignetteIntensity should be copied onto the modifier"), DistortionModifier->MaxVignetteIntensity, VisualSubsystem->MaxVignetteIntensity);

	// (d) a second bind attempt is idempotent - must not add a second modifier.
	TestTrue(TEXT("A second TryBindCameraManager call should stay bound (idempotent)"),
		VisualSubsystem->TryBindCameraManager());

	UOvercrowdDetectionComponent* OvercrowdComponent = NewObject<UOvercrowdDetectionComponent>(PlayerPawn);
	OvercrowdComponent->RegisterComponent();

	// (e) overcrowd-component binding now succeeds exactly once.
	TestTrue(TEXT("TryBindOvercrowdComponent should succeed once a UOvercrowdDetectionComponent exists"),
		VisualSubsystem->TryBindOvercrowdComponent());

	// (f) a second bind attempt is idempotent - must not double-subscribe the handler.
	TestTrue(TEXT("A second TryBindOvercrowdComponent call should stay bound (idempotent)"),
		VisualSubsystem->TryBindOvercrowdComponent());

	// (g) driving the real component to Active (enough hot-and-uncontrolled enemies,
	// sustained for the full duration - same shape as
	// KrowdKontrolOvercrowdAudioSubsystemTest.cpp) flips the visual subsystem to
	// Distorted, broadcasts exactly once, and engages the modifier.
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
	TestEqual(TEXT("Overcrowd going Active should flip visual distortion state to Distorted"),
		static_cast<uint8>(VisualSubsystem->GetVisualDistortionState()), static_cast<uint8>(EOvercrowdVisualDistortionState::Distorted));
	TestEqual(TEXT("OnOvercrowdVisualDistortionStateChanged should have fired exactly once"), Listener->CallCount, 1);
	TestEqual(TEXT("Broadcast should carry Distorted"),
		static_cast<uint8>(Listener->LastState), static_cast<uint8>(EOvercrowdVisualDistortionState::Distorted));
	TestTrue(TEXT("DistortionModifier should be engaged"), DistortionModifier->bIsEngaged);

	// (h) exercise the real ModifyCamera -> ModifyPostProcess -> alpha-convergence chain.
	// First a mid-ease step (half of EaseInSeconds, starting from CurrentAlpha == 0) proves
	// the eased curve shape itself - not just eventual convergence - matches
	// FMath::InterpEaseInOut(0, 1, RawProgress, EaseExponent), which a double-eased or
	// non-converging implementation would not satisfy at the midpoint.
	FMinimalViewInfo DummyPOV;
	DistortionModifier->ModifyCamera(DistortionModifier->EaseInSeconds * 0.5f, DummyPOV);
	const float ExpectedMidEaseAlpha = FMath::InterpEaseInOut(0.0f, 1.0f, 0.5f, DistortionModifier->EaseExponent);
	TestEqual(TEXT("CurrentAlpha at the ease midpoint should match the expected eased value"),
		DistortionModifier->GetCurrentAlpha(), ExpectedMidEaseAlpha, KINDA_SMALL_NUMBER);

	// Then a large DeltaTime (10 seconds) fully converges CurrentAlpha to 1.0 - proving the
	// eased convergence actually reaches its target given enough time, not just that the
	// target flag flipped.
	DistortionModifier->ModifyCamera(10.0f, DummyPOV);
	TestEqual(TEXT("CurrentAlpha should converge to 1.0 given a large enough DeltaTime"),
		DistortionModifier->GetCurrentAlpha(), 1.0f);

	// Directly exercise ModifyPostProcess (friend access) to assert the actual rendered
	// payload - override flags, intensity values, and blend weight - rather than only the
	// internal CurrentAlpha value, since CurrentAlpha converging correctly does not by
	// itself prove any of these five values were written correctly.
	float BlendWeight = 0.0f;
	FPostProcessSettings PPSettings;
	DistortionModifier->ModifyPostProcess(10.0f, BlendWeight, PPSettings);
	TestTrue(TEXT("SceneFringeIntensity override should be set"), PPSettings.bOverride_SceneFringeIntensity);
	TestEqual(TEXT("SceneFringeIntensity should equal MaxSceneFringeIntensity at full engagement"),
		PPSettings.SceneFringeIntensity, DistortionModifier->MaxSceneFringeIntensity);
	TestTrue(TEXT("VignetteIntensity override should be set"), PPSettings.bOverride_VignetteIntensity);
	TestEqual(TEXT("VignetteIntensity should equal MaxVignetteIntensity at full engagement"),
		PPSettings.VignetteIntensity, DistortionModifier->MaxVignetteIntensity);
	TestEqual(TEXT("PostProcessBlendWeight should equal CurrentAlpha"),
		BlendWeight, DistortionModifier->GetCurrentAlpha());

	// (i) a further real advance while already Active must not re-fire (the component
	// itself only broadcasts once, but this also proves SetVisualDistortionState's own
	// no-op guard holds).
	OvercrowdComponent->AdvancePanicOverloadState(1.0f);
	TestEqual(TEXT("A further advance while already Active should not re-fire the distortion delegate"), Listener->CallCount, 1);

	// (j) simulate a future recovery event: OvercrowdDetectionComponent.h documents
	// Active -> Inactive as out of scope for issue #16 (a separate, later issue), so no
	// real code path produces this broadcast today. Broadcasting Inactive directly on the
	// component's own public delegate (no friend access needed) proves the subsystem's
	// handler correctly clears the moment such a transition exists.
	OvercrowdComponent->OnPanicOverloadStateChanged.Broadcast(EPanicOverloadState::Inactive);
	TestEqual(TEXT("A simulated Inactive broadcast should clear the visual distortion state immediately"),
		static_cast<uint8>(VisualSubsystem->GetVisualDistortionState()), static_cast<uint8>(EOvercrowdVisualDistortionState::Clear));
	TestEqual(TEXT("The clear should broadcast exactly once more"), Listener->CallCount, 2);
	TestEqual(TEXT("Second broadcast should carry Clear"),
		static_cast<uint8>(Listener->LastState), static_cast<uint8>(EOvercrowdVisualDistortionState::Clear));
	TestFalse(TEXT("DistortionModifier should be disengaged"), DistortionModifier->bIsEngaged);

	// Ease-out convergence: drive the disengaged modifier with a large DeltaTime (10s
	// against a 1.5s default EaseOutSeconds) and assert CurrentAlpha actually decays back
	// to 0.0 - the ease-out direction is never otherwise exercised through
	// ModifyCamera/ModifyPostProcess, only the subsystem-level bIsEngaged flag above.
	DistortionModifier->ModifyCamera(10.0f, DummyPOV);
	TestEqual(TEXT("CurrentAlpha should converge back to 0.0 after disengagement given enough time"),
		DistortionModifier->GetCurrentAlpha(), 0.0f);

	// (k) a redundant Inactive broadcast must not re-fire (no-op guard).
	OvercrowdComponent->OnPanicOverloadStateChanged.Broadcast(EPanicOverloadState::Inactive);
	TestEqual(TEXT("A redundant Inactive broadcast should not re-fire the distortion delegate"), Listener->CallCount, 2);

	return true;
}

// Confirms SetVisualDistortionState's documented "logically succeeded, side effect
// silently skipped" fallback: if a Panic Overload transition arrives before
// TryBindCameraManager() has ever succeeded (DistortionModifier still null - e.g. very
// early in a level's first frame), CurrentState still flips and the change delegate
// still broadcasts, only the modifier's SetEngaged call is skipped. Mirrors
// OvercrowdAudioSubsystem::bHasWarnedMissingAudioDevice's equivalent contract.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolOvercrowdVisualEffectMissingCameraManagerFallbackTest,
	"KrowdKontrol.Unit.OvercrowdVisualEffectMissingCameraManagerFallback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolOvercrowdVisualEffectMissingCameraManagerFallbackTest::RunTest(const FString& Parameters)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
	{
		return false;
	}

	UOvercrowdVisualEffectSubsystem* VisualSubsystem = World->GetSubsystem<UOvercrowdVisualEffectSubsystem>();
	if (!TestNotNull(TEXT("UWorld should auto-instantiate UOvercrowdVisualEffectSubsystem"), VisualSubsystem))
	{
		return false;
	}

	UOvercrowdDistortionStateTestListener* Listener = NewObject<UOvercrowdDistortionStateTestListener>();
	VisualSubsystem->OnOvercrowdVisualDistortionStateChanged.AddDynamic(Listener, &UOvercrowdDistortionStateTestListener::HandleOvercrowdVisualDistortionStateChanged);

	APawn* PlayerPawn = World->SpawnActor<APawn>();
	if (!TestNotNull(TEXT("APawn should spawn into the test World"), PlayerPawn))
	{
		return false;
	}

	UOvercrowdDetectionComponent* OvercrowdComponent = NewObject<UOvercrowdDetectionComponent>(PlayerPawn);
	OvercrowdComponent->RegisterComponent();

	// Deliberately bind the overcrowd component but never call TryBindCameraManager() (and
	// never spawn a PlayerController/PlayerCameraManager) - DistortionModifier stays null,
	// exercising the fallback branch this test targets.
	TestTrue(TEXT("TryBindOvercrowdComponent should succeed with no PlayerCameraManager in the world"),
		VisualSubsystem->TryBindOvercrowdComponent());

	OvercrowdComponent->OnPanicOverloadStateChanged.Broadcast(EPanicOverloadState::Active);
	TestEqual(TEXT("CurrentState should still flip to Distorted even with no camera manager bound"),
		static_cast<uint8>(VisualSubsystem->GetVisualDistortionState()), static_cast<uint8>(EOvercrowdVisualDistortionState::Distorted));
	TestEqual(TEXT("The delegate should still broadcast despite the missing modifier"), Listener->CallCount, 1);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
