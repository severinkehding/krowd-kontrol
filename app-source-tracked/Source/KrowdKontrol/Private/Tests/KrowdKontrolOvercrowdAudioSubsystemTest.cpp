// Confirms UOvercrowdAudioSubsystem (issue #38, Audio & Music PRD REQ-3) toggles
// EOvercrowdAudioMuffleState between Clear and Muffled exactly in sync with
// UOvercrowdDetectionComponent::OnPanicOverloadStateChanged - activating on Active,
// deactivating on Inactive, never double-firing, and binding to exactly one
// UOvercrowdDetectionComponent found in the world.
//
// OvercrowdDetectionComponent.h documents CurrentState as one-directional in its own
// scope today (Inactive -> Active only; Active -> Inactive recovery is deferred to a
// separate, later issue - see that header's comment). This test proves the Muffled ->
// Clear path by broadcasting Inactive directly on the component's own public
// OnPanicOverloadStateChanged delegate (a plain UPROPERTY(BlueprintAssignable), callable
// with no friend access) rather than driving a real recovery transition, since no code
// path that produces one exists yet. That is a deliberate, documented scope choice (see
// this issue's plan NOT_BUILDING section), not a gap in this test.
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the other
// KrowdKontrol.Unit.* tests.

#include "Misc/AutomationTest.h"
#include "OvercrowdAudioSubsystem.h"
#include "OvercrowdMuffleStateTestListener.h"
#include "OvercrowdDetectionComponent.h"
#include "EnemyBaseTestActor.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolOvercrowdAudioSubsystemTest,
	"KrowdKontrol.Unit.OvercrowdAudioSubsystem",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolOvercrowdAudioSubsystemTest::RunTest(const FString& Parameters)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
	{
		return false;
	}

	UOvercrowdAudioSubsystem* AudioSubsystem = World->GetSubsystem<UOvercrowdAudioSubsystem>();
	if (!TestNotNull(TEXT("UWorld should auto-instantiate UOvercrowdAudioSubsystem"), AudioSubsystem))
	{
		return false;
	}

	UOvercrowdMuffleStateTestListener* Listener = NewObject<UOvercrowdMuffleStateTestListener>();
	AudioSubsystem->OnOvercrowdAudioMuffleStateChanged.AddDynamic(Listener, &UOvercrowdMuffleStateTestListener::HandleOvercrowdAudioMuffleStateChanged);

	// (a) default state, before any pawn/component exists or any bind is attempted.
	TestEqual(TEXT("Default muffle state should be Clear"),
		static_cast<uint8>(AudioSubsystem->GetMuffleState()), static_cast<uint8>(EOvercrowdAudioMuffleState::Clear));

	// (b) binding fails gracefully (returns false, no crash) with no pawn/component in the world yet.
	TestFalse(TEXT("TryBindOvercrowdComponent should fail gracefully with no UOvercrowdDetectionComponent in the world"),
		AudioSubsystem->TryBindOvercrowdComponent());

	APawn* PlayerPawn = World->SpawnActor<APawn>();
	if (!TestNotNull(TEXT("APawn should spawn into the test World"), PlayerPawn))
	{
		return false;
	}
	UOvercrowdDetectionComponent* OvercrowdComponent = NewObject<UOvercrowdDetectionComponent>(PlayerPawn);
	OvercrowdComponent->RegisterComponent();

	// (c) binding now succeeds exactly once.
	TestTrue(TEXT("TryBindOvercrowdComponent should succeed once a UOvercrowdDetectionComponent exists"),
		AudioSubsystem->TryBindOvercrowdComponent());

	// (d) a second bind attempt is idempotent - must not double-subscribe the handler.
	TestTrue(TEXT("A second TryBindOvercrowdComponent call should stay bound (idempotent)"),
		AudioSubsystem->TryBindOvercrowdComponent());

	// (e) driving the real component to Active (enough hot-and-uncontrolled enemies, sustained
	// for the full duration - same shape as KrowdKontrolOvercrowdDetectionComponentTest.cpp)
	// flips the audio subsystem to Muffled and broadcasts exactly once.
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
	TestEqual(TEXT("Overcrowd going Active should flip audio muffle state to Muffled"),
		static_cast<uint8>(AudioSubsystem->GetMuffleState()), static_cast<uint8>(EOvercrowdAudioMuffleState::Muffled));
	TestEqual(TEXT("OnOvercrowdAudioMuffleStateChanged should have fired exactly once"), Listener->CallCount, 1);
	TestEqual(TEXT("Broadcast should carry Muffled"),
		static_cast<uint8>(Listener->LastState), static_cast<uint8>(EOvercrowdAudioMuffleState::Muffled));

	// (f) a further real advance while already Active must not re-fire (the component itself
	// only broadcasts once, but this also proves SetMuffleState's own no-op guard holds).
	OvercrowdComponent->AdvancePanicOverloadState(1.0f);
	TestEqual(TEXT("A further advance while already Active should not re-fire the muffle delegate"), Listener->CallCount, 1);

	// (g) simulate a future recovery event: OvercrowdDetectionComponent.h documents Active ->
	// Inactive as out of scope for issue #16 (a separate, later issue), so no real code path
	// produces this broadcast today. Broadcasting Inactive directly on the component's own
	// public delegate (no friend access needed) proves the subsystem's handler correctly
	// clears the moment such a transition exists, per this plan's NOT_BUILDING scope note.
	OvercrowdComponent->OnPanicOverloadStateChanged.Broadcast(EPanicOverloadState::Inactive);
	TestEqual(TEXT("A simulated Inactive broadcast should clear the muffle state immediately"),
		static_cast<uint8>(AudioSubsystem->GetMuffleState()), static_cast<uint8>(EOvercrowdAudioMuffleState::Clear));
	TestEqual(TEXT("The clear should broadcast exactly once more"), Listener->CallCount, 2);
	TestEqual(TEXT("Second broadcast should carry Clear"),
		static_cast<uint8>(Listener->LastState), static_cast<uint8>(EOvercrowdAudioMuffleState::Clear));

	// (h) a redundant Inactive broadcast must not re-fire (no-op guard).
	OvercrowdComponent->OnPanicOverloadStateChanged.Broadcast(EPanicOverloadState::Inactive);
	TestEqual(TEXT("A redundant Inactive broadcast should not re-fire the muffle delegate"), Listener->CallCount, 2);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
