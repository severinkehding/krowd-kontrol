#include "EnemyBase.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Pawn.h"
#include "PlayerEnergyComponent.h"
#include "EngineUtils.h"
#include "Components/PointLightComponent.h"

AEnemyBase::AEnemyBase()
{
	// Unlike ABossBase (never ticks; every transition is externally driven),
	// AEnemyBase must self-detect the player via a per-tick proximity check, so
	// ticking is required here. This divergence from the mirrored BossBase pattern is
	// deliberate, not an oversight - see issue #12.
	PrimaryActorTick.bCanEverTick = true;
}

void AEnemyBase::ReceiveControl(EAbilitySlot Ability)
{
	if (CurrentState != EEnemyState::Alert && CurrentState != EEnemyState::Attack)
	{
		return;
	}
	CurrentState = EEnemyState::Controlled;
	OnControlledEntry(Ability);
}

void AEnemyBase::TransitionToBanked()
{
	if (CurrentState != EEnemyState::Controlled)
	{
		return;
	}
	// Flip before broadcasting (see ABossBase::TransitionToBanked) so a listener that
	// re-enters from within OnEnemyBanked sees the terminal state immediately, and
	// this method is already a no-op for it.
	CurrentState = EEnemyState::Banked;
	OnEnemyBanked.Broadcast();
}

void AEnemyBase::AdvanceToAlert()
{
	if (CurrentState != EEnemyState::Idle)
	{
		return;
	}
	CurrentState = EEnemyState::Alert;
}

void AEnemyBase::AdvanceToAttack()
{
	if (CurrentState != EEnemyState::Alert)
	{
		return;
	}
	CurrentState = EEnemyState::Attack;
	OnAttackEntry();
}

void AEnemyBase::TickCheckDetection(const FVector& PlayerLocation)
{
	const float Distance = FVector::Dist(GetActorLocation(), PlayerLocation);
	// At most one state transition per call, via else-if: a very-close spawn advances
	// Idle->Alert this tick and Alert->Attack the next, never skipping Alert within a
	// single frame.
	if (CurrentState == EEnemyState::Idle && Distance <= DetectionRangeUnits)
	{
		AdvanceToAlert();
	}
	else if (CurrentState == EEnemyState::Alert && Distance <= GetAttackRangeUnits())
	{
		AdvanceToAttack();
	}
}

void AEnemyBase::TickChaseMovement(const FVector& PlayerLocation, float DeltaSeconds)
{
	if (CurrentState != EEnemyState::Alert)
	{
		return;
	}
	const FVector ToPlayer = PlayerLocation - GetActorLocation();
	const float DistanceRemaining = ToPlayer.Size();
	if (DistanceRemaining <= KINDA_SMALL_NUMBER)
	{
		return;
	}
	const float MoveDistance = FMath::Min(DistanceRemaining, GetEffectiveMovementSpeedUnitsPerSecond() * DeltaSeconds);
	SetActorLocation(GetActorLocation() + ToPlayer.GetSafeNormal() * MoveDistance);
}

void AEnemyBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// GetPlayerPawn returns nullptr in a headless Automation test with no
	// PlayerController spawned - this is fine and expected; TickCheckDetection and
	// TickChaseMovement are both friend-testable independently of a live Tick() call
	// for exactly this reason.
	if (APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0))
	{
		const FVector PlayerLocation = PlayerPawn->GetActorLocation();
		TickCheckDetection(PlayerLocation);
		TickChaseMovement(PlayerLocation, DeltaTime);
	}
}

UPlayerEnergyComponent* AEnemyBase::FindPlayerEnergyComponent() const
{
	if (!GetWorld())
	{
		return nullptr;
	}
	for (TActorIterator<APawn> It(GetWorld()); It; ++It)
	{
		if (UPlayerEnergyComponent* Energy = It->FindComponentByClass<UPlayerEnergyComponent>())
		{
			return Energy;
		}
	}
	UE_LOG(LogTemp, Warning,
		TEXT("AEnemyBase: FindPlayerEnergyComponent on '%s' found no APawn with a UPlayerEnergyComponent."),
		*GetNameSafe(this));
	return nullptr;
}

EThreatState AEnemyBase::GetThreatState() const
{
	const bool bIsEngaged = CurrentState == EEnemyState::Alert
		|| CurrentState == EEnemyState::Attack
		|| CurrentState == EEnemyState::Controlled;
	return bIsEngaged ? EThreatState::Hot : EThreatState::Idle;
}

void AEnemyBase::SetIsElite(bool bNewIsElite)
{
	bIsElite = bNewIsElite;
	if (UPointLightComponent* TrimLight = GetEliteTrimLightComponent())
	{
		TrimLight->SetIntensity(bIsElite ? EliteTrimIntensity : 0.0f);
	}
}

float AEnemyBase::GetEffectiveMovementSpeedUnitsPerSecond() const
{
	return GetMovementSpeedUnitsPerSecond() * (bIsElite ? EliteMovementSpeedMultiplier : 1.0f);
}
