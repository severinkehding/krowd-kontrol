// Adds KrowdKontrol.PIE.SniperRangeBreakChase (issue #360) - the
// docs/prd-functional-pie-tests.md-mandated KrowdKontrol.PIE.* companion to this
// issue's fix (AEnemyBase::TickCheckDetection's new Attack -> Alert range-break
// branch, RevertAttackToAlert(), and ASniperEnemy::MovementSpeed). Mirrors
// KrowdKontrolPIEPostContactAttackRecoveryTest.cpp's (issue #314) latent-command
// state-machine shape exactly, since that file is this issue's own direct sibling -
// #313/#314 proved Attack exits on a timer with no ReceiveControl() call, #360/this
// file proves Attack also exits on proximity, then the enemy chases and re-acquires,
// all through real per-frame ticking, never a direct/friend call to
// TickCheckDetection/TickChaseMovement/RevertAttackToAlert.
//
// Resolves the first ASniperEnemy specifically (not a generic AEnemyBase), since this
// issue's AC is Sniper-specific (MovementSpeed, AttackTellLightComponent). Opened
// against L_Level02, not L_Level01: issue #42's own changelog deliberately deferred
// ASniperEnemy placement out of L_Level01 ("ASniperEnemy is deliberately deferred to
// a later, denser level"), and issue #43's changelog confirms L_Level02 is the first
// level to place it (2x ASniperEnemy) - so this is a deviation from the plan's stated
// L_Level01, not an ambiguity; see implementation.md.
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the other
// KrowdKontrol.* automation tests.

#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "UObject/StrongObjectPtr.h"
#include "SniperEnemy.h"
#include "RoomActor.h"
#include "Kismet/GameplayStatics.h"
#include "EnemyAttackExpiredTestListener.h"
#include "Components/PointLightComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

// State-machine latent command, not a flat list of ADD_LATENT_AUTOMATION_COMMAND
// calls: the sniper has to be driven through several real-tick-only waits in
// sequence (optional room-activation, Idle->Alert->Attack, retreat just past attack
// range, range-break expiry, chase-movement resumption, re-acquire), each of which
// can take several real frames.
class FKrowdKontrolDriveSniperRangeBreakChaseCommand : public IAutomationLatentCommand
{
public:
	FKrowdKontrolDriveSniperRangeBreakChaseCommand(FAutomationTestBase* InTest, TSharedRef<TStrongObjectPtr<UEnemyAttackExpiredTestListener>> InListener)
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
		WaitForRangeBreak,
		WaitForMovementResumption,
		WaitForReAcquire,
		Done
	};

	FAutomationTestBase* Test;
	TSharedRef<TStrongObjectPtr<UEnemyAttackExpiredTestListener>> Listener;
	double DriveStartTime;
	double MovementCheckStartTime = 0.0;
	ASniperEnemy* TargetSniper = nullptr;
	ARoomActor* TargetRoom = nullptr;
	FVector SniperLocationAtRangeBreakStart = FVector::ZeroVector;
	EPhase Phase = EPhase::ResolveTopology;
};

bool FKrowdKontrolDriveSniperRangeBreakChaseCommand::Update()
{
	// Wall-clock timeout, not a frame count: a stuck detection/range-break/re-acquire
	// state must fail loudly instead of hanging the automation run indefinitely. 30s
	// comfortably covers the room-activation countdown (~3s) plus the telegraph and
	// the bounded movement/re-acquire waits below.
	if (FPlatformTime::Seconds() - DriveStartTime > 30.0)
	{
		Test->AddError(TEXT("Timed out driving the sniper through acquire -> break -> chase -> re-acquire within 30 seconds"));
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
		for (TActorIterator<ASniperEnemy> It(PIEWorld); It; ++It)
		{
			TargetSniper = *It;
			break;
		}
		if (!Test->TestNotNull(TEXT("L_Level02 should have at least one ASniperEnemy actor to drive through a range-break chase"), TargetSniper))
		{
			return true;
		}

		// Bound here (not in RetreatPlayer) so no attack-expiry broadcast between now
		// and the retreat phase can be missed.
		TargetSniper->OnEnemyAttackExpired.AddDynamic(Listener->Get(), &UEnemyAttackExpiredTestListener::HandleEnemyAttackExpired);

		TargetRoom = TargetSniper->GetOwningRoom();
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
			PlayerPawn->SetActorLocation(TargetSniper->GetActorLocation(), /*bSweep=*/false);
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
			PlayerPawn->SetActorLocation(TargetSniper->GetActorLocation(), /*bSweep=*/false);
			Phase = EPhase::WaitForAttack;
		}
		return false;
	}

	case EPhase::WaitForAttack:
		// Idle->Alert happens one real tick after the teleport, Alert->Attack the tick
		// after that (TickCheckDetection's "at most one transition per call" else-if
		// structure) - this resolves within a small handful of frames at zero
		// distance, since ASniperEnemy::GetAttackRangeUnits() (1400.0f) is almost
		// equal to its inherited DetectionRangeUnits (1500.0f).
		if (TargetSniper->GetEnemyState() == EEnemyState::Attack)
		{
			Phase = EPhase::RetreatPlayer;
		}
		return false;

	case EPhase::RetreatPlayer:
	{
		// Retreats just beyond attack range (1400.0f), not the full 10000-unit clear
		// the sibling PostContactAttackRecovery test uses for the detection-range
		// case - 1450 units keeps the player within DetectionRangeUnits (1500.0f) so
		// the sniper stays Alert-engaged and visibly chases, rather than losing the
		// player (and this issue's own AC) entirely.
		APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(PIEWorld, 0);
		if (!Test->TestNotNull(TEXT("A player pawn should still exist to retreat just beyond attack range"), PlayerPawn))
		{
			return true;
		}
		PlayerPawn->SetActorLocation(TargetSniper->GetActorLocation() + FVector(1450.0f, 0.0f, 0.0f), /*bSweep=*/false);
		Phase = EPhase::WaitForRangeBreak;
		return false;
	}

	case EPhase::WaitForRangeBreak:
		// This is the assertion that fails pre-#360 (Attack had no proximity-driven
		// exit, only ReceiveControl()/the #313 timeout) and passes post-fix.
		if (TargetSniper->GetEnemyState() == EEnemyState::Alert)
		{
			Test->TestEqual(TEXT("Sniper should revert from Attack to Alert on its own once the player leaves attack range mid-telegraph, with no ReceiveControl() call"),
				TargetSniper->GetEnemyState(), EEnemyState::Alert);
			Test->TestEqual(TEXT("OnEnemyAttackExpired should fire exactly once during the range-break"),
				Listener->Get()->CallCount, 1);
			Test->TestEqual(TEXT("Attack tell should be cleared once the sniper reverts to Alert"),
				TargetSniper->AttackTellLightComponent->Intensity, 0.0f);
			SniperLocationAtRangeBreakStart = TargetSniper->GetActorLocation();
			MovementCheckStartTime = FPlatformTime::Seconds();
			Phase = EPhase::WaitForMovementResumption;
		}
		return false;

	case EPhase::WaitForMovementResumption:
	{
		// Proof that chase movement actually resumed (IsMovementBehaviorActive() ->
		// TickChaseMovement, driven by ASniperEnemy::MovementSpeed via
		// GetMovementSpeedUnitsPerSecond()), not just that the state flag flipped -
		// this test has no friendship to call state-active accessors directly, so
		// displacement toward the retreated player is the only observable proxy.
		const bool bResumedChase = FVector::DistSquared(TargetSniper->GetActorLocation(), SniperLocationAtRangeBreakStart) > FMath::Square(10.0f);
		if (bResumedChase)
		{
			Test->TestTrue(TEXT("The sniper should resume chase movement toward the (retreated) player within the bounded window, with no new control-ability applied"), bResumedChase);
			MovementCheckStartTime = FPlatformTime::Seconds();
			Phase = EPhase::WaitForReAcquire;
		}
		else if (FPlatformTime::Seconds() - MovementCheckStartTime > 3.0)
		{
			Test->AddError(TEXT("Sniper did not resume chase movement after the range-break"));
			return true;
		}
		return false;
	}

	case EPhase::WaitForReAcquire:
		// Issue #360's own re-acquire requirement, absent from the sibling
		// PostContactAttackRecovery test (which retreats far enough that the enemy
		// never re-engages): the chasing sniper must close the 1450->1400 unit gap
		// and re-enter Attack with a genuinely fresh telegraph (tell light lit again,
		// not a frozen leftover value from before the range-break).
		if (TargetSniper->GetEnemyState() == EEnemyState::Attack)
		{
			Test->TestEqual(TEXT("Sniper should re-acquire the player and re-enter Attack once it closes back within attack range"),
				TargetSniper->GetEnemyState(), EEnemyState::Attack);
			Test->TestTrue(TEXT("Attack tell should be lit again on re-acquire, proving a fresh telegraph rather than a frozen one"),
				TargetSniper->AttackTellLightComponent->Intensity > 0.0f);
			Phase = EPhase::Done;
		}
		else if (FPlatformTime::Seconds() - MovementCheckStartTime > 10.0)
		{
			Test->AddError(TEXT("Sniper did not re-acquire the player and re-enter Attack after chasing back into range"));
			return true;
		}
		return false;

	case EPhase::Done:
		return true;
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolPIESniperRangeBreakChaseTest,
	"KrowdKontrol.PIE.SniperRangeBreakChase",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolPIESniperRangeBreakChaseTest::RunTest(const FString& Parameters)
{
	TSharedRef<TStrongObjectPtr<UEnemyAttackExpiredTestListener>> Listener =
		MakeShared<TStrongObjectPtr<UEnemyAttackExpiredTestListener>>(NewObject<UEnemyAttackExpiredTestListener>());

	AutomationOpenMap(TEXT("/Game/Maps/L_Level02"));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForEngineFramesCommand(5));
	ADD_LATENT_AUTOMATION_COMMAND(FKrowdKontrolDriveSniperRangeBreakChaseCommand(this, Listener));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
