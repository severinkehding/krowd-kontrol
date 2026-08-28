// Adds KrowdKontrol.PIE.PostContactAttackRecovery (issue #314) - the
// docs/prd-functional-pie-tests.md-mandated KrowdKontrol.PIE.* companion to issue
// #313's fix (PR #336, AEnemyBase::TickAttackDuration()): a timer-based,
// ability-independent Attack -> Alert exit. #313's own bundled regression coverage
// (KrowdKontrol.Unit.EnemyBase's (i5)/(i5b)/(i5d) cases) drives the transition
// entirely through the TickCheckDetection/TickAttackDuration friend hooks in a
// CreateNewMap()/NewObject world that never runs a real begin-play/tick/PIE-session
// path - per harness/README.md's own stated rule, a feature whose failure mode
// involves ticking needs a KrowdKontrol.PIE.* scenario too, since a fully green Unit
// suite has already shipped three live-only regressions past the gate this way
// (#199, #223, #234).
//
// This test opens L_Level01 in a real PIE session, resolves the first AEnemyBase in
// the level, teleports the real player pawn onto it so the real Tick() ->
// TickCheckDetection() proximity check (never a direct/friend call) drives
// Idle->Alert->Attack, then retreats the player pawn well outside detection range
// (mirroring issue #313's own "player retreats out of range" bug report) and asserts,
// purely through real per-frame ticking with ReceiveControl() never called, that the
// enemy's Attack state reverts to Alert on its own, OnEnemyAttackExpired fires exactly
// once, and the enemy visibly resumes chase movement toward the retreated player
// afterward. No lifecycle/friend method (TickCheckDetection, AdvanceToAttack,
// TickAttackDuration, GetAttackDurationSeconds) is ever called directly.
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the other
// KrowdKontrol.* automation tests.

#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "UObject/StrongObjectPtr.h"
#include "EnemyBase.h"
#include "RoomActor.h"
#include "Kismet/GameplayStatics.h"
#include "EnemyAttackExpiredTestListener.h"

#if WITH_DEV_AUTOMATION_TESTS

// State-machine latent command, not a flat list of ADD_LATENT_AUTOMATION_COMMAND
// calls: the enemy has to be driven through several real-tick-only waits in sequence
// (optional room-activation, Idle->Alert->Attack, retreat, Attack-duration expiry,
// chase-movement resumption), each of which can take several real frames.
class FKrowdKontrolDriveEnemyIntoAttackAndRecoverCommand : public IAutomationLatentCommand
{
public:
	FKrowdKontrolDriveEnemyIntoAttackAndRecoverCommand(FAutomationTestBase* InTest, TSharedRef<TStrongObjectPtr<UEnemyAttackExpiredTestListener>> InListener)
		: Test(InTest)
		, Listener(InListener)
		, DriveStartTime(FPlatformTime::Seconds())
	{
	}

	virtual bool Update() override;

private:
	enum class EPhase : uint8
	{
		ResolveTopology,
		WaitForRoomActivated,
		WaitForAttack,
		RetreatPlayer,
		WaitForAttackExpiry,
		WaitForMovementResumption,
		Done
	};

	FAutomationTestBase* Test;
	TSharedRef<TStrongObjectPtr<UEnemyAttackExpiredTestListener>> Listener;
	double DriveStartTime;
	double MovementCheckStartTime = 0.0;
	AEnemyBase* TargetEnemy = nullptr;
	ARoomActor* TargetRoom = nullptr;
	FVector EnemyLocationAtRecoveryStart = FVector::ZeroVector;
	EPhase Phase = EPhase::ResolveTopology;
};

bool FKrowdKontrolDriveEnemyIntoAttackAndRecoverCommand::Update()
{
	// Wall-clock timeout, not a frame count: a stuck detection/expiry state must fail
	// loudly instead of hanging the automation run indefinitely. 30s comfortably
	// covers the ~2.5s worst-case Attack-duration across all four concrete enemy
	// types (EnemyBase.h's GetAttackDurationSeconds() floor) plus the room-activation
	// countdown (~3s).
	if (FPlatformTime::Seconds() - DriveStartTime > 30.0)
	{
		Test->AddError(TEXT("Timed out driving the enemy into Attack and back to Alert within 30 seconds"));
		return true;
	}

	UWorld* PIEWorld = AutomationCommon::GetAnyGameWorld();
	if (!PIEWorld)
	{
		return false;
	}

	switch (Phase)
	{
	case EPhase::ResolveTopology:
	{
		for (TActorIterator<AEnemyBase> It(PIEWorld); It; ++It)
		{
			TargetEnemy = *It;
			break;
		}
		if (!Test->TestNotNull(TEXT("L_Level01 should have at least one AEnemyBase actor to drive through Attack recovery"), TargetEnemy))
		{
			return true;
		}

		// Bound here (not in RetreatPlayer) so no attack-expiry
		// broadcast between now and the retreat phase can be missed.
		TargetEnemy->OnEnemyAttackExpired.AddDynamic(Listener->Get(), &UEnemyAttackExpiredTestListener::HandleEnemyAttackExpired);

		TargetRoom = TargetEnemy->GetOwningRoom();
		APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(PIEWorld, 0);
		if (!Test->TestNotNull(TEXT("A player pawn should exist to drive real-tick proximity detection"), PlayerPawn))
		{
			return true;
		}

		if (TargetRoom)
		{
			PlayerPawn->SetActorLocation(TargetRoom->GetActorLocation(), /*bSweep=*/false);
			Phase = EPhase::WaitForRoomActivated;
		}
		else
		{
			PlayerPawn->SetActorLocation(TargetEnemy->GetActorLocation(), /*bSweep=*/false);
			Phase = EPhase::WaitForAttack;
		}
		return false;
	}

	case EPhase::WaitForRoomActivated:
	{
		if (TargetRoom->IsRoomActivated())
		{
			APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(PIEWorld, 0);
			if (!Test->TestNotNull(TEXT("A player pawn should still exist once the owning room activates"), PlayerPawn))
			{
				return true;
			}
			PlayerPawn->SetActorLocation(TargetEnemy->GetActorLocation(), /*bSweep=*/false);
			Phase = EPhase::WaitForAttack;
		}
		return false;
	}

	case EPhase::WaitForAttack:
		// Idle->Alert happens one real tick after the teleport, Alert->Attack the tick
		// after that (TickCheckDetection's "at most one transition per call" else-if
		// structure) - this resolves within a small handful of frames at zero
		// distance, for any of the four concrete enemy types.
		if (TargetEnemy->GetEnemyState() == EEnemyState::Attack)
		{
			Phase = EPhase::RetreatPlayer;
		}
		return false;

	case EPhase::RetreatPlayer:
	{
		// Listener is already bound (ResolveTopology) - this phase only retreats the
		// player pawn, mirroring issue #313's own bug report framing ("player
		// retreats out of range"). 10000 units clears every concrete type's
		// DetectionRangeUnits (base default 1500) with a comfortable margin, so
		// Attack cannot be immediately re-triggered once it reverts.
		APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(PIEWorld, 0);
		if (!Test->TestNotNull(TEXT("A player pawn should still exist to retreat out of attack range"), PlayerPawn))
		{
			return true;
		}
		PlayerPawn->SetActorLocation(TargetEnemy->GetActorLocation() + FVector(10000.0f, 0.0f, 0.0f), /*bSweep=*/false);
		Phase = EPhase::WaitForAttackExpiry;
		return false;
	}

	case EPhase::WaitForAttackExpiry:
		// This is the assertion that fails pre-#313 (Attack had no exit but
		// ReceiveControl(), which this test never calls) and passes post-PR#336.
		if (TargetEnemy->GetEnemyState() == EEnemyState::Alert)
		{
			Test->TestEqual(TEXT("Enemy should revert from Attack to Alert on its own once the Attack-duration timeout elapses, with no ReceiveControl() call (issue #313's guaranteed exit)"),
				TargetEnemy->GetEnemyState(), EEnemyState::Alert);
			Test->TestEqual(TEXT("OnEnemyAttackExpired should fire exactly once during the recovery window"),
				Listener->Get()->CallCount, 1);
			EnemyLocationAtRecoveryStart = TargetEnemy->GetActorLocation();
			MovementCheckStartTime = FPlatformTime::Seconds();
			Phase = EPhase::WaitForMovementResumption;
		}
		return false;

	case EPhase::WaitForMovementResumption:
	{
		// Proof that chase movement actually resumed (IsMovementBehaviorActive() ->
		// TickChaseMovement), not just that the state flag flipped - this test has no
		// friendship to call state-active accessors directly, so displacement toward
		// the retreated player is the only observable proxy.
		const bool bResumedChase = FVector::DistSquared(TargetEnemy->GetActorLocation(), EnemyLocationAtRecoveryStart) > FMath::Square(10.0f);
		if (bResumedChase)
		{
			Test->TestTrue(TEXT("The enemy should resume chase movement toward the (retreated) player within the bounded window, with no new control-ability applied"), bResumedChase);
			Phase = EPhase::Done;
		}
		else if (FPlatformTime::Seconds() - MovementCheckStartTime > 3.0)
		{
			Test->AddError(TEXT("Enemy did not resume chase movement after Attack-duration expiry"));
			return true;
		}
		return false;
	}

	case EPhase::Done:
		return true;
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolPIEPostContactAttackRecoveryTest,
	"KrowdKontrol.PIE.PostContactAttackRecovery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolPIEPostContactAttackRecoveryTest::RunTest(const FString& Parameters)
{
	TSharedRef<TStrongObjectPtr<UEnemyAttackExpiredTestListener>> Listener =
		MakeShared<TStrongObjectPtr<UEnemyAttackExpiredTestListener>>(NewObject<UEnemyAttackExpiredTestListener>());

	AutomationOpenMap(TEXT("/Game/Maps/L_Level01"));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForEngineFramesCommand(5));
	ADD_LATENT_AUTOMATION_COMMAND(FKrowdKontrolDriveEnemyIntoAttackAndRecoverCommand(this, Listener));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
