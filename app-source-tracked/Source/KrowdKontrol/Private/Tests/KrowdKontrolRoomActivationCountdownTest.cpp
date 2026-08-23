// Confirms issue #245 (PRD "Room Encounter Flow" REQ-3): on first entry into an
// un-cleared room, ARoomActor runs a visible 3-second "3"->"2"->"1" countdown via
// UOnScreenPromptWidget, holding its owned enemies Idle for the full duration
// (extending AEnemyBase::IsPlayerInOwningRoom()'s issue #244 gate with
// ARoomActor::IsActivationPending()), then activates exactly once - a permanent
// latch that never re-triggers on re-entry or on an already-cleared room.
//
// Each case uses its own FAutomationEditorCommonUtils::CreateNewMap() World, per
// this module's established per-scenario isolation convention (see
// KrowdKontrolAbilityUnlockPromptComponentTest.cpp). Drives CheckFirstEntry/
// AdvanceCountdown/TickCheckDetection directly via friend grants, never a real
// per-frame Tick() loop - same rationale KrowdKontrolEnemyRoomDetectionGateTest.cpp
// documents for its own room-gate test.
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the other
// KrowdKontrol.Unit.* tests.

#include "Misc/AutomationTest.h"
#include "RoomActor.h"
#include "EnemyBaseTestActor.h"
#include "KrowdKontrolPlayerController.h"
#include "OnScreenPromptWidget.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Engine.h"
#include "GameFramework/Pawn.h"
#include "Components/SceneComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolRoomActivationCountdownTest,
	"KrowdKontrol.Unit.RoomActivationCountdown",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

namespace KrowdKontrolRoomActivationCountdownTest
{
	// Spawns a controller-backed AKrowdKontrolPlayerController with a real, live
	// OnScreenPromptWidgetInstance - duplicated verbatim from
	// KrowdKontrolAbilityUnlockPromptComponentTest.cpp's SpawnControllerWithPromptWidget()
	// (this codebase duplicates this helper per test file rather than sharing it).
	AKrowdKontrolPlayerController* SpawnControllerWithPromptWidget(UWorld* World)
	{
		AKrowdKontrolPlayerController* Controller = World->SpawnActor<AKrowdKontrolPlayerController>();
		if (!Controller)
		{
			return nullptr;
		}
		Controller->Player = NewObject<ULocalPlayer>(GEngine);
		Controller->SetAsLocalPlayerController();
		World->AddController(Controller);
		Controller->DispatchBeginPlay();
		return Controller;
	}
}

bool FKrowdKontrolRoomActivationCountdownTest::RunTest(const FString& Parameters)
{
	using namespace KrowdKontrolRoomActivationCountdownTest;

	// (1)/(2)/(3): countdown starts on first entry only, owned enemies stay Idle for
	// the full duration regardless of proximity, and the room activates (gate opens)
	// at expiry.
	{
		UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
		if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
		{
			return false;
		}

		const FVector RoomLocation(0.f, 0.f, 0.f);
		ARoomActor* OwnRoom = World->SpawnActor<ARoomActor>(ARoomActor::StaticClass(), FTransform(RoomLocation));
		if (!TestNotNull(TEXT("OwnRoom should spawn into the test World"), OwnRoom))
		{
			return false;
		}

		AEnemyBaseTestActor* GatedEnemy = World->SpawnActor<AEnemyBaseTestActor>(AEnemyBaseTestActor::StaticClass(), FTransform(RoomLocation));
		if (!TestNotNull(TEXT("GatedEnemy should spawn into the test World"), GatedEnemy))
		{
			return false;
		}
		OwnRoom->AddOwnedEnemy(GatedEnemy);

		// (0) Before any CheckFirstEntry call, an un-cleared room's countdown has never
		// started - IsActivationPending() must be false (the PR's own documented deviation
		// from the plan: keyed on bCountdownActive, not "not yet activated" more broadly) so
		// TickCheckDetection's proximity-only gate still opens normally for callers that never
		// drive a real Tick() loop (every pre-#245 Automation test).
		TestFalse(TEXT("IsActivationPending should be false before CheckFirstEntry has ever run"),
			OwnRoom->IsActivationPending());

		// (1) First entry starts the countdown at the configured default.
		OwnRoom->CheckFirstEntry(RoomLocation);
		TestTrue(TEXT("CheckFirstEntry should start the countdown on first entry"), OwnRoom->IsCountdownActive());
		TestEqual(TEXT("Countdown should start at RoomActivationCountdownSeconds"),
			OwnRoom->GetRemainingCountdownSeconds(), OwnRoom->RoomActivationCountdownSeconds);

		// Re-entry while already counting down must not restart it.
		OwnRoom->CheckFirstEntry(RoomLocation);
		TestTrue(TEXT("Countdown should still be active after a second CheckFirstEntry call"), OwnRoom->IsCountdownActive());
		TestEqual(TEXT("A second CheckFirstEntry call must not restart the countdown"),
			OwnRoom->GetRemainingCountdownSeconds(), OwnRoom->RoomActivationCountdownSeconds);

		// (2) Owned enemy stays Idle even at zero distance from the player.
		GatedEnemy->TickCheckDetection(RoomLocation);
		TestEqual(TEXT("Owned enemy should stay Idle while the countdown is active, even at zero distance"),
			static_cast<uint8>(GatedEnemy->GetEnemyState()), static_cast<uint8>(EEnemyState::Idle));

		OwnRoom->AdvanceCountdown(1.5f);
		TestTrue(TEXT("Countdown should still be active partway through"), OwnRoom->IsCountdownActive());

		GatedEnemy->TickCheckDetection(RoomLocation);
		TestEqual(TEXT("Owned enemy should still be Idle partway through the countdown"),
			static_cast<uint8>(GatedEnemy->GetEnemyState()), static_cast<uint8>(EEnemyState::Idle));

		// (3) Crossing zero activates the room and opens the gate.
		OwnRoom->AdvanceCountdown(1.6f);
		TestFalse(TEXT("Countdown should no longer be active once it reaches zero"), OwnRoom->IsCountdownActive());
		TestTrue(TEXT("Room should be activated once the countdown reaches zero"), OwnRoom->IsRoomActivated());

		GatedEnemy->TickCheckDetection(RoomLocation);
		TestEqual(TEXT("Owned enemy should reach Alert once the room activates"),
			static_cast<uint8>(GatedEnemy->GetEnemyState()), static_cast<uint8>(EEnemyState::Alert));

		// (4a) No re-trigger after activation.
		OwnRoom->CheckFirstEntry(RoomLocation);
		TestFalse(TEXT("CheckFirstEntry must not restart the countdown once the room is activated"), OwnRoom->IsCountdownActive());
		TestEqual(TEXT("Remaining countdown should stay at zero after an already-activated room's CheckFirstEntry"),
			OwnRoom->GetRemainingCountdownSeconds(), 0.0f);
	}

	// (4b) A vacuously-cleared room (no owned enemies) never starts a countdown.
	{
		UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
		if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
		{
			return false;
		}

		const FVector RoomLocation(0.f, 0.f, 0.f);
		ARoomActor* ClearedRoom = World->SpawnActor<ARoomActor>(ARoomActor::StaticClass(), FTransform(RoomLocation));
		if (!TestNotNull(TEXT("ClearedRoom should spawn into the test World"), ClearedRoom))
		{
			return false;
		}

		ClearedRoom->CheckFirstEntry(RoomLocation);
		TestFalse(TEXT("A room with no owned enemies should never start a countdown"), ClearedRoom->IsCountdownActive());
	}

	// (5) Widget text assertion: "3" -> "2" -> "1" across the countdown.
	{
		UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
		if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
		{
			return false;
		}

		AKrowdKontrolPlayerController* Controller = SpawnControllerWithPromptWidget(World);
		if (!TestNotNull(TEXT("Controller should spawn"), Controller) ||
			!TestNotNull(TEXT("Controller should own a live OnScreenPromptWidgetInstance"), ToRawPtr(Controller->OnScreenPromptWidgetInstance)))
		{
			return false;
		}

		const FVector RoomLocation(0.f, 0.f, 0.f);
		ARoomActor* OwnRoom = World->SpawnActor<ARoomActor>(ARoomActor::StaticClass(), FTransform(RoomLocation));
		AEnemyBaseTestActor* GatedEnemy = World->SpawnActor<AEnemyBaseTestActor>(AEnemyBaseTestActor::StaticClass(), FTransform(RoomLocation));
		if (!TestNotNull(TEXT("OwnRoom should spawn into the test World"), OwnRoom) ||
			!TestNotNull(TEXT("GatedEnemy should spawn into the test World"), GatedEnemy))
		{
			return false;
		}
		OwnRoom->AddOwnedEnemy(GatedEnemy);

		OwnRoom->CheckFirstEntry(RoomLocation);
		TestEqual(TEXT("Prompt should show '3' on countdown start"),
			Controller->OnScreenPromptWidgetInstance->GetPromptDisplayText().ToString(), FString(TEXT("3")));

		OwnRoom->AdvanceCountdown(1.1f);
		TestEqual(TEXT("Prompt should show '2' after crossing the first boundary"),
			Controller->OnScreenPromptWidgetInstance->GetPromptDisplayText().ToString(), FString(TEXT("2")));

		OwnRoom->AdvanceCountdown(1.0f);
		TestEqual(TEXT("Prompt should show '1' after crossing the second boundary"),
			Controller->OnScreenPromptWidgetInstance->GetPromptDisplayText().ToString(), FString(TEXT("1")));
	}

	// (6) RoomActivationCountdownSeconds is configurable per-instance - a non-default
	// value must drive both the starting countdown and the activation timing, not a
	// hardcoded 3.0s.
	{
		UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
		if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
		{
			return false;
		}

		const FVector RoomLocation(0.f, 0.f, 0.f);
		ARoomActor* OwnRoom = World->SpawnActor<ARoomActor>(ARoomActor::StaticClass(), FTransform(RoomLocation));
		if (!TestNotNull(TEXT("OwnRoom should spawn into the test World"), OwnRoom))
		{
			return false;
		}

		AEnemyBaseTestActor* GatedEnemy = World->SpawnActor<AEnemyBaseTestActor>(AEnemyBaseTestActor::StaticClass(), FTransform(RoomLocation));
		if (!TestNotNull(TEXT("GatedEnemy should spawn into the test World"), GatedEnemy))
		{
			return false;
		}
		OwnRoom->AddOwnedEnemy(GatedEnemy);

		OwnRoom->RoomActivationCountdownSeconds = 2.0f;
		OwnRoom->CheckFirstEntry(RoomLocation);
		TestEqual(TEXT("Countdown should start at the configured (non-default) RoomActivationCountdownSeconds"),
			OwnRoom->GetRemainingCountdownSeconds(), 2.0f);

		OwnRoom->AdvanceCountdown(2.0f);
		TestTrue(TEXT("Room should activate after the configured (non-default) countdown duration elapses"),
			OwnRoom->IsRoomActivated());
	}

	// (7) CheckFirstEntry only starts the countdown for the room the player actually
	// resolves nearest to - proves the FindNearestRoom guard discriminates, not just exists.
	{
		UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
		if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
		{
			return false;
		}

		// Same 3000cm spacing as KrowdKontrolEnemyRoomDetectionGateTest.cpp.
		ARoomActor* NearRoom = World->SpawnActor<ARoomActor>(ARoomActor::StaticClass(), FTransform(FVector(0.f, 0.f, 0.f)));
		ARoomActor* FarRoom = World->SpawnActor<ARoomActor>(ARoomActor::StaticClass(), FTransform(FVector(3000.f, 0.f, 0.f)));
		if (!TestNotNull(TEXT("NearRoom should spawn into the test World"), NearRoom) ||
			!TestNotNull(TEXT("FarRoom should spawn into the test World"), FarRoom))
		{
			return false;
		}

		AEnemyBaseTestActor* NearEnemy = World->SpawnActor<AEnemyBaseTestActor>(AEnemyBaseTestActor::StaticClass(), FTransform(FVector(0.f, 0.f, 0.f)));
		AEnemyBaseTestActor* FarEnemy = World->SpawnActor<AEnemyBaseTestActor>(AEnemyBaseTestActor::StaticClass(), FTransform(FVector(3000.f, 0.f, 0.f)));
		if (!TestNotNull(TEXT("NearEnemy should spawn into the test World"), NearEnemy) ||
			!TestNotNull(TEXT("FarEnemy should spawn into the test World"), FarEnemy))
		{
			return false;
		}
		NearRoom->AddOwnedEnemy(NearEnemy);
		FarRoom->AddOwnedEnemy(FarEnemy);

		// Player at (1300,0,0) resolves nearest to NearRoom (|1300-0|=1300 < |1300-3000|=1700).
		// Calling CheckFirstEntry on FarRoom with this same location must not start its countdown.
		FarRoom->CheckFirstEntry(FVector(1300.f, 0.f, 0.f));
		TestFalse(TEXT("CheckFirstEntry must not start the countdown on a room the player isn't nearest to"),
			FarRoom->IsCountdownActive());

		NearRoom->CheckFirstEntry(FVector(1300.f, 0.f, 0.f));
		TestTrue(TEXT("CheckFirstEntry should start the countdown on the room the player actually resolves nearest to"),
			NearRoom->IsCountdownActive());
	}

	// (8) End-to-end Tick() orchestration (issue #290 pass-1 E2E finding): drives
	// the real ARoomActor::Tick()/AEnemyBase::Tick() overrides directly across
	// several simulated frames with a real possessed player pawn, instead of
	// calling CheckFirstEntry/AdvanceCountdown/TickCheckDetection independently
	// like every scenario above - proves the two per-frame Tick() loops actually
	// hold the enemy Idle together in practice, which is what a live PIE session
	// experiences and what the isolated per-function scenarios above cannot catch
	// (they missed both the Tick()-ordering double-consumption bug and the
	// throttled-poll race window that let enemies engage while the on-screen
	// countdown still read "3").
	{
		UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
		if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
		{
			return false;
		}

		const FVector RoomLocation(0.f, 0.f, 0.f);
		ARoomActor* OwnRoom = World->SpawnActor<ARoomActor>(ARoomActor::StaticClass(), FTransform(RoomLocation));
		AEnemyBaseTestActor* GatedEnemy = World->SpawnActor<AEnemyBaseTestActor>(AEnemyBaseTestActor::StaticClass(), FTransform(RoomLocation));
		if (!TestNotNull(TEXT("OwnRoom should spawn into the test World"), OwnRoom) ||
			!TestNotNull(TEXT("GatedEnemy should spawn into the test World"), GatedEnemy))
		{
			return false;
		}
		OwnRoom->AddOwnedEnemy(GatedEnemy);

		// Same possessed-pawn setup KrowdKontrolEnemyBaseTest.cpp's own real-Tick()
		// cases (t)/(u) use: UGameplayStatics::GetPlayerPawn (what both Tick()
		// overrides actually call) resolves through World::PlayerControllerList,
		// which needs an explicit AddController() here since CreateNewMap()'s
		// editor world never runs AController::PostInitializeComponents.
		// Placed 500 units off the enemy - within DetectionRangeUnits (1500, so
		// the Idle->Alert gate is the one under test) but outside the base
		// GetAttackRangeUnits() AEnemyBaseTestActor inherits unmodified (0.0f,
		// i.e. only an exact-location overlap would advance further to Attack) -
		// zero distance here would let the enemy race on past Alert into Attack
		// once the gate opens, which is a real subsequent transition this test
		// isn't about and would make the Alert assertion below flaky.
		const FVector PlayerLocation(500.f, 0.f, 0.f);
		APawn* PlayerPawn = World->SpawnActor<APawn>(APawn::StaticClass(), FTransform(PlayerLocation));
		APlayerController* Controller = World->SpawnActor<APlayerController>();
		if (!TestNotNull(TEXT("PlayerPawn should spawn into the test World"), PlayerPawn) ||
			!TestNotNull(TEXT("Should be able to spawn a controller to possess the pawn"), Controller))
		{
			return false;
		}
		Controller->Possess(PlayerPawn);
		World->AddController(Controller);
		USceneComponent* PlayerPawnRoot = NewObject<USceneComponent>(PlayerPawn);
		PlayerPawnRoot->RegisterComponent();
		PlayerPawn->SetRootComponent(PlayerPawnRoot);
		PlayerPawn->SetActorLocation(PlayerLocation);

		// Several simulated frames, well under RoomActivationCountdownSeconds's
		// 3.0s default, well within detection range of the player - both real
		// Tick() overrides run every frame in-engine (ARoomActor's own
		// first-entry poll used to be throttled to 0.25s; that mismatch against
		// AEnemyBase's per-frame Tick() was the root cause of issue #290's
		// live-session failure), so this drives them the same way: every
		// simulated frame.
		for (int32 Frame = 0; Frame < 10; ++Frame)
		{
			OwnRoom->Tick(0.05f);
			GatedEnemy->Tick(0.05f);
		}
		TestTrue(TEXT("Countdown should already be active after a handful of early frames"), OwnRoom->IsCountdownActive());
		TestEqual(TEXT("Owned enemy must stay Idle through the early countdown window, even well within detection range"),
			static_cast<uint8>(GatedEnemy->GetEnemyState()), static_cast<uint8>(EEnemyState::Idle));

		// Drive the remaining countdown to completion via the real Tick() loop.
		for (int32 Frame = 0; Frame < 60; ++Frame)
		{
			OwnRoom->Tick(0.05f);
			GatedEnemy->Tick(0.05f);
		}
		TestTrue(TEXT("Room should be activated once the full countdown elapses via real Tick() calls"), OwnRoom->IsRoomActivated());
		TestEqual(TEXT("Owned enemy should reach Alert once the room activates, via the real per-frame Tick() loop"),
			static_cast<uint8>(GatedEnemy->GetEnemyState()), static_cast<uint8>(EEnemyState::Alert));
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
