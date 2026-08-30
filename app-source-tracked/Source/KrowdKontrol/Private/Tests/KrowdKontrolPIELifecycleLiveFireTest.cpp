// Adds KrowdKontrol.PIE.LifecycleLiveFire (PRD docs/prd-functional-pie-tests.md REQ-3
// item 1, issue #234's acceptance test) - the third and final REQ-3 scenario test in
// the KrowdKontrol.PIE.* group (issue #236) that KrowdKontrolPIESessionTest.cpp stood
// up.
//
// Every KrowdKontrol.Unit.* lifecycle test drives OnWorldBeginPlay()/
// RefreshLevelClearState() by direct call in a CreateNewMap() world, so no automated
// test tier has ever exercised the real PIE begin-play -> tick -> OnLevelClear ->
// save-to-disk chain end-to-end. Issue #234 proved this gap live (all six enemies
// banked in real play, OnLevelClear never fired, no save file) - since fixed (PR #288:
// IsTickableWhenPaused() override + diagnostic logging). #234 is already merged by the
// time this test lands, so this test is expected to PASS on first write, not merely to
// be authored red - it pins that fix (and the whole chain around it) against
// regression, it does not reproduce the original bug.
//
// This test opens L_Level01 in a real PIE session, asserts HasLevelBegun() on the real
// ULevelLifecycleSubsystem, binds a ULevelLifecycleTestListener to the live
// subsystem's OnLevelClear delegate, drives every enemy in the level to Banked
// room-by-room (real player-pawn teleport -> real Tick()-driven detection ->
// ReceiveControl() -> swept teleport onto the enemy's type-matched ATargetZone -> real
// physics overlap), then asserts OnLevelClear fires and
// Saved/SaveGames/KrowdKontrol_LevelClearTimes.sav exists with a recorded time. No
// lifecycle method (OnWorldBeginPlay, RefreshLevelClearState, TickCheckDetection,
// TickChaseMovement) is ever called directly - every state change is reached only
// through the real engine tick during the PIE session.
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the other
// KrowdKontrol.* automation tests.
//
// Also pins issue #330's REQ-4 write-through path (PRD "Crowd Mastery Persistence"):
// the final assertion block additionally confirms the real deposit chain
// (UCrowdMasterySubsystem::HandleLevelClear -> UCrowdMasteryTotalSubsystem::
// DepositRunMastery) raises GetAccumulatedTotal() above 0 in this same live PIE
// session, not just in the Unit-tier NewObject<>() tests. Because this test's drive
// loop calls ReceiveControl() directly rather than through a real
// UAbilityCastComponent::ApplyAbility(), it also calls
// UCrowdMasterySubsystem::SampleControlledCount() explicitly right after
// ReceiveControl() (see FKrowdKontrolDriveAllEnemiesToBankedCommand::Update()) -
// without that, the OnAbilityCastApplied broadcast a real cast would trigger never
// fires, RunningMaxControlledCount stays stuck at 0, and DepositRunMastery(0) would
// make this assertion vacuous regardless of REQ-4's own correctness.

#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "UObject/StrongObjectPtr.h"
#include "LevelLifecycleSubsystem.h"
#include "LevelClearTimeSubsystem.h"
#include "LevelClearTimeSaveGame.h"
#include "CrowdMasteryTotalSubsystem.h"
#include "CrowdMasterySubsystem.h"
#include "Tests/LevelLifecycleTestListener.h"
#include "EnemyBase.h"
#include "RoomActor.h"
#include "TargetZone.h"
#include "EnemyTypeIndicatorComponent.h"
#include "AbilitySlot.h"
#include "Kismet/GameplayStatics.h"

#if WITH_DEV_AUTOMATION_TESTS

static ULevelLifecycleSubsystem* GetPIELifecycleSubsystem(UWorld* World)
{
	return World ? World->GetSubsystem<ULevelLifecycleSubsystem>() : nullptr;
}

DEFINE_LATENT_AUTOMATION_COMMAND_TWO_PARAMETER(FKrowdKontrolAssertLevelBeginFiredAndBindListenerCommand, FAutomationTestBase*, Test, TSharedRef<TStrongObjectPtr<ULevelLifecycleTestListener>>, OutListener);

bool FKrowdKontrolAssertLevelBeginFiredAndBindListenerCommand::Update()
{
	UWorld* PIEWorld = AutomationCommon::GetAnyGameWorld();
	ULevelLifecycleSubsystem* Subsystem = GetPIELifecycleSubsystem(PIEWorld);
	if (Test->TestNotNull(TEXT("A live PIE world should exist"), PIEWorld)
		&& Test->TestNotNull(TEXT("The real PIE world should carry a ULevelLifecycleSubsystem"), Subsystem))
	{
		Test->TestTrue(TEXT("Level-begin should have fired via the real PIE begin-play tick"), Subsystem->HasLevelBegun());

		OutListener->Reset(NewObject<ULevelLifecycleTestListener>());
		Subsystem->OnLevelClear.AddDynamic(OutListener->Get(), &ULevelLifecycleTestListener::HandleLevelClear);
	}
	return true;
}

// State-machine latent command, not a flat list of ADD_LATENT_AUTOMATION_COMMAND calls:
// ARoomActor::Tick() only advances ONE room's first-entry countdown at a time, keyed on
// FindNearestRoom(PlayerLocation, AllRooms) by room-actor-origin distance
// (RoomActor.cpp:155-197, 376-391) - the player pawn must be teleported to each room in
// turn and that room's ~3.0s RoomActivationCountdownSeconds countdown must fully elapse
// (IsRoomActivated() becomes permanently true) before that room's owned enemies can
// leave Idle. This spans many frames and rooms, so it has to be one stateful command.
class FKrowdKontrolDriveAllEnemiesToBankedCommand : public IAutomationLatentCommand
{
public:
	explicit FKrowdKontrolDriveAllEnemiesToBankedCommand(FAutomationTestBase* InTest)
		: Test(InTest)
		, DriveStartTime(FPlatformTime::Seconds())
	{
	}

	virtual bool Update() override;

private:
	enum class ERoomPhase : uint8
	{
		MovePlayerToRoom,
		WaitForRoomActivated,
		DriveRoomEnemies,
		Done
	};

	enum class EEnemyPhase : uint8
	{
		WaitForDetection,
		ReceiveControl,
		TeleportOntoZone,
		AssertBanked
	};

	FAutomationTestBase* Test;
	double DriveStartTime;
	bool bTopologyResolved = false;
	// Diagnosis seam (2026-08-30 art-pass gate failure): if the PIE world is torn
	// down and replaced mid-drive (the player-defeat level restart is the only
	// in-game path that does this), every cached Room/Zone pointer above dangles
	// and the drive silently hangs until the 60s timeout. Detect it and fail with
	// the actual reason instead.
	TWeakObjectPtr<UWorld> ObservedWorld;
	TArray<ARoomActor*> Rooms;
	TMap<EEnemyType, ATargetZone*> ZoneByType;
	int32 CurrentRoomIndex = 0;
	int32 EnemyIndexInRoom = 0;
	ERoomPhase RoomPhase = ERoomPhase::MovePlayerToRoom;
	EEnemyPhase EnemyPhase = EEnemyPhase::WaitForDetection;
};

bool FKrowdKontrolDriveAllEnemiesToBankedCommand::Update()
{
	// Wall-clock timeout, not a frame count: a stuck detection/room-activation state
	// must fail loudly instead of hanging the automation run indefinitely.
	if (FPlatformTime::Seconds() - DriveStartTime > 60.0)
	{
		Test->AddError(TEXT("Timed out driving all enemies to Banked within 60 seconds"));
		return true;
	}

	UWorld* PIEWorld = AutomationCommon::GetAnyGameWorld();
	if (!PIEWorld)
	{
		return false;
	}
	if (bTopologyResolved && ObservedWorld.Get() != PIEWorld)
	{
		Test->AddError(TEXT("PIE world was replaced mid-drive (in-game level restart - i.e. the player pawn was defeated while the drive was still herding); cached room/zone topology is stale, aborting"));
		return true;
	}

	if (!bTopologyResolved)
	{
		ObservedWorld = PIEWorld;
		for (TActorIterator<ARoomActor> It(PIEWorld); It; ++It)
		{
			Rooms.Add(*It);
		}
		for (ARoomActor* Room : Rooms)
		{
			for (const FRoomTargetZone& Zone : Room->GetTargetZones())
			{
				if (!Zone.MarkerActor)
				{
					continue;
				}
				TArray<AActor*> Attached;
				Zone.MarkerActor->GetAttachedActors(Attached);
				for (AActor* AttachedActor : Attached)
				{
					if (ATargetZone* Candidate = Cast<ATargetZone>(AttachedActor))
					{
						// Level-wide by EnemyType, not scoped to CurrentRoom - if two rooms in a
						// future level both have a same-type pen, the later room's zone silently
						// wins for every enemy of that type across the whole level (harmless today:
						// ATargetZone::HandleZoneOverlap gates on type only, never room identity -
						// but this map is not "this room's zone," it's "a zone of this type
						// somewhere in the level").
						ZoneByType.Add(Zone.EnemyType, Candidate);
						break;
					}
				}
			}
		}
		bTopologyResolved = true;
		UE_LOG(LogTemp, Display, TEXT("LiveFireDrive: topology resolved - %d rooms, %d typed zones"), Rooms.Num(), ZoneByType.Num());
	}

	if (RoomPhase == ERoomPhase::Done || CurrentRoomIndex >= Rooms.Num())
	{
		return true;
	}

	ARoomActor* CurrentRoom = Rooms[CurrentRoomIndex];

	switch (RoomPhase)
	{
	case ERoomPhase::MovePlayerToRoom:
	{
		if (CurrentRoom->GetOwnedEnemies().Num() == 0)
		{
			// Nothing to drive in this room - skip it immediately rather than waiting
			// on an activation/detection sequence that will never matter.
			++CurrentRoomIndex;
			return false;
		}
		APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(PIEWorld, 0);
		if (!Test->TestNotNull(TEXT("A player pawn should exist to drive room-by-room detection"), PlayerPawn))
		{
			return true;
		}
		PlayerPawn->SetActorLocation(CurrentRoom->GetActorLocation(), /*bSweep=*/false);
		UE_LOG(LogTemp, Display, TEXT("LiveFireDrive: player moved to room %d ('%s'), waiting for activation"), CurrentRoomIndex, *CurrentRoom->GetActorLabel());
		RoomPhase = ERoomPhase::WaitForRoomActivated;
		return false;
	}
	case ERoomPhase::WaitForRoomActivated:
	{
		if (CurrentRoom->IsRoomActivated())
		{
			UE_LOG(LogTemp, Display, TEXT("LiveFireDrive: room %d activated, driving %d enemies"), CurrentRoomIndex, CurrentRoom->GetOwnedEnemies().Num());
			RoomPhase = ERoomPhase::DriveRoomEnemies;
		}
		return false;
	}
	case ERoomPhase::DriveRoomEnemies:
	{
		const TArray<TObjectPtr<AEnemyBase>>& OwnedEnemies = CurrentRoom->GetOwnedEnemies();
		if (EnemyIndexInRoom >= OwnedEnemies.Num())
		{
			++CurrentRoomIndex;
			EnemyIndexInRoom = 0;
			EnemyPhase = EEnemyPhase::WaitForDetection;
			RoomPhase = ERoomPhase::MovePlayerToRoom;
			return false;
		}

		AEnemyBase* Enemy = OwnedEnemies[EnemyIndexInRoom];
		switch (EnemyPhase)
		{
		case EEnemyPhase::WaitForDetection:
			// ReceiveControl() is a no-op unless CurrentState is Alert or Attack
			// (EnemyBase.cpp) - never call it before this real-tick-driven state is
			// reached.
			if (Enemy->GetEnemyState() == EEnemyState::Alert || Enemy->GetEnemyState() == EEnemyState::Attack)
			{
				UE_LOG(LogTemp, Display, TEXT("LiveFireDrive: enemy %d/'%s' detected (state=%d), issuing control"), EnemyIndexInRoom, *Enemy->GetActorLabel(), static_cast<int32>(Enemy->GetEnemyState()));
				EnemyPhase = EEnemyPhase::ReceiveControl;
			}
			return false;

		case EEnemyPhase::ReceiveControl:
			Enemy->ReceiveControl(EAbilitySlot::Stun);
			// This drives ReceiveControl() directly rather than through a real
			// UAbilityCastComponent::ApplyAbility() call, so the OnAbilityCastApplied
			// broadcast that would normally trigger UCrowdMasterySubsystem::
			// SampleControlledCount() (issue #174) never fires here - sample explicitly
			// so the Crowd Mastery running-max this issue's #330 persistence assertion
			// depends on reflects reality instead of staying stuck at 0. Uses the same
			// public, test-drivable entry point SampleControlledCount()'s own doc
			// comment already calls out for exactly this purpose.
			if (UCrowdMasterySubsystem* CrowdMasterySubsystem = PIEWorld->GetSubsystem<UCrowdMasterySubsystem>())
			{
				CrowdMasterySubsystem->SampleControlledCount();
			}
			EnemyPhase = EEnemyPhase::TeleportOntoZone;
			return false;

		case EEnemyPhase::TeleportOntoZone:
		{
			UE_LOG(LogTemp, Display, TEXT("LiveFireDrive: pre-sweep enemy %d/'%s' at %s (state=%d)"), EnemyIndexInRoom, *Enemy->GetActorLabel(), *Enemy->GetActorLocation().ToCompactString(), static_cast<int32>(Enemy->GetEnemyState()));
			// Resolve via the enemy's own type, never a fixed room->zone mapping - a
			// wrong-type zone silently no-ops the bank (ATargetZone::HandleZoneOverlap).
			const UEnemyTypeIndicatorComponent* TypeIndicator = Enemy->FindComponentByClass<UEnemyTypeIndicatorComponent>();
			ATargetZone* Zone = TypeIndicator ? ZoneByType.FindRef(TypeIndicator->EnemyType) : nullptr;
			if (!Test->TestNotNull(TEXT("Enemy should resolve a type-matched ATargetZone before being teleported onto it"), Zone))
			{
				return true;
			}
			// Banking is a real physics overlap event, not a distance/tick check - the
			// move must generate a begin-overlap against the zone. bSweep=false, NOT
			// true: a plain teleport still runs UpdateOverlaps() and broadcasts the
			// begin-overlap banking listens for, while a swept move dies entirely
			// (bMoved=false, enemy never leaves its spot) whenever any third blocking
			// actor sits on the straight line to the pen - live-diagnosed 2026-08-30:
			// L1 room 2's still-Idle BomberEnemy at (4800,500) sat exactly between the
			// driven bomber (4800,292) and its zone (4800,700), and which bomber the
			// drive picks first is registration-order luck that a level re-save can
			// (and did) reshuffle.
			const bool bMoved = Enemy->SetActorLocation(Zone->GetActorLocation(), /*bSweep=*/false);
			UE_LOG(LogTemp, Display, TEXT("LiveFireDrive: swept enemy %d/'%s' toward zone at %s - moved=%d, landed at %s"), EnemyIndexInRoom, *Enemy->GetActorLabel(), *Zone->GetActorLocation().ToCompactString(), bMoved ? 1 : 0, *Enemy->GetActorLocation().ToCompactString());
			EnemyPhase = EEnemyPhase::AssertBanked;
			return false;
		}

		case EEnemyPhase::AssertBanked:
			if (Enemy->GetEnemyState() == EEnemyState::Banked)
			{
				Test->TestEqual(TEXT("Enemy should reach Banked after a swept teleport onto its type-matched zone"),
					Enemy->GetEnemyState(), EEnemyState::Banked);
				UE_LOG(LogTemp, Display, TEXT("LiveFireDrive: enemy %d/'%s' banked"), EnemyIndexInRoom, *Enemy->GetActorLabel());
				++EnemyIndexInRoom;
				EnemyPhase = EEnemyPhase::WaitForDetection;
			}
			return false;
		}
		return false;
	}

	default:
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolPIELifecycleLiveFireTest,
	"KrowdKontrol.PIE.LifecycleLiveFire",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolPIELifecycleLiveFireTest::RunTest(const FString& Parameters)
{
	TSharedRef<TStrongObjectPtr<ULevelLifecycleTestListener>> Listener = MakeShared<TStrongObjectPtr<ULevelLifecycleTestListener>>();

	AutomationOpenMap(TEXT("/Game/Maps/L_Level01"));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForEngineFramesCommand(5));
	ADD_LATENT_AUTOMATION_COMMAND(FKrowdKontrolAssertLevelBeginFiredAndBindListenerCommand(this, Listener));
	ADD_LATENT_AUTOMATION_COMMAND(FKrowdKontrolDriveAllEnemiesToBankedCommand(this));
	ADD_LATENT_AUTOMATION_COMMAND(FUntilCommand(
		[this, Listener]() -> bool
		{
			return Listener->Get() && Listener->Get()->LevelClearCallCount > 0;
		},
		[this]() -> bool
		{
			AddError(TEXT("Timed out waiting for OnLevelClear to fire after all enemies reached Banked"));
			return true;
		},
		15.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FUntilCommand(
		[this]() -> bool
		{
			UWorld* PIEWorld = AutomationCommon::GetAnyGameWorld();
			if (!PIEWorld)
			{
				return false;
			}
			if (!TestTrue(TEXT("KrowdKontrol_LevelClearTimes.sav should exist after OnLevelClear"),
				UGameplayStatics::DoesSaveGameExist(ULevelClearTimeSubsystem::SaveSlotName, 0)))
			{
				return true;
			}
			UGameInstance* GameInstance = PIEWorld->GetGameInstance();
			ULevelClearTimeSubsystem* ClearTimeSubsystem = GameInstance ? GameInstance->GetSubsystem<ULevelClearTimeSubsystem>() : nullptr;
			if (!TestNotNull(TEXT("The real PIE GameInstance should carry a ULevelClearTimeSubsystem"), ClearTimeSubsystem))
			{
				return true;
			}
			// PIE-mangled map name (e.g. UEDPIE_0_L_Level01) is the real save-lookup
			// key ULevelLifecycleSubsystem::RefreshLevelClearState() uses - not
			// UWorld::RemovePIEPrefix(...), which is only for display assertions.
			float BestSeconds = 0.0f;
			const FName MapID(*PIEWorld->GetMapName());
			if (TestTrue(TEXT("GetBestClearTimeSeconds should find a recorded clear time for this PIE session's map"),
				ClearTimeSubsystem->GetBestClearTimeSeconds(MapID, BestSeconds)))
			{
				TestTrue(TEXT("The recorded clear time should be greater than zero"), BestSeconds > 0.0f);
			}
			UCrowdMasteryTotalSubsystem* MasteryTotalSubsystem = GameInstance ? GameInstance->GetSubsystem<UCrowdMasteryTotalSubsystem>() : nullptr;
			if (TestNotNull(TEXT("The real PIE GameInstance should carry a UCrowdMasteryTotalSubsystem"), MasteryTotalSubsystem))
			{
				// Real deposit-on-clear (UCrowdMasterySubsystem::HandleLevelClear) plus this
				// issue's write-through persistence (UCrowdMasteryTotalSubsystem::
				// PersistAccumulatedTotal, issue #330) - proves both REQ-1's accumulation and
				// REQ-4's save-to-disk actually fire in a real PIE session, not just in the
				// Unit-tier NewObject<>() tests, which never exercise the real deposit call
				// chain (GetGameInstance() is null in CreateNewMap() worlds).
				TestTrue(TEXT("The accumulated Crowd Mastery total should be greater than zero after a real level clear"),
					MasteryTotalSubsystem->GetAccumulatedTotal() > 0);

				// In-memory state alone doesn't prove REQ-4 (persistence) actually happened -
				// DepositRunMastery() only logs a warning and continues if SaveGameToSlot
				// fails, so confirm the write actually reached disk, same as the clear-time
				// check above does.
				if (USaveGame* MasteryLoaded = UGameplayStatics::LoadGameFromSlot(ULevelClearTimeSubsystem::SaveSlotName, 0))
				{
					if (ULevelClearTimeSaveGame* MasteryTyped = Cast<ULevelClearTimeSaveGame>(MasteryLoaded))
					{
						TestTrue(TEXT("AccumulatedCrowdMasteryTotal should be persisted to disk after a real level clear, not just held in memory"),
							MasteryTyped->AccumulatedCrowdMasteryTotal > 0);
					}
				}
			}
			return true;
		},
		[this]() -> bool
		{
			AddError(TEXT("Timed out waiting for a live PIE world to assert the level-clear-times save file against"));
			return true;
		},
		15.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
