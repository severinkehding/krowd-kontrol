#include "OvercrowdDetectionComponent.h"
#include "EnemyBase.h"
#include "EngineUtils.h"

UOvercrowdDetectionComponent::UOvercrowdDetectionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UOvercrowdDetectionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	AdvancePanicOverloadState(DeltaTime);
}

void UOvercrowdDetectionComponent::AdvancePanicOverloadState(float DeltaSeconds)
{
	if (CurrentState == EPanicOverloadState::Active)
	{
		if (!HasConvergedEnemyBeenControlled())
		{
			return;
		}

		// Recovery (PRD 08 REQ-2, issue #18): landing any CC ability on a converged
		// enemy immediately ends Panic Overload - no separate "panic button", the
		// same 5 CC verbs recover it. Reset UncontrolledSeconds so the crowd must
		// re-arm from zero rather than instantly re-triggering this same tick if the
		// remaining nearby count is still >= OvercrowdCrowdThreshold.
		// Flip before broadcasting (see MusicSubsystem::SetMusicState) so a
		// re-entrant listener sees CurrentState already updated.
		CurrentState = EPanicOverloadState::Inactive;
		UncontrolledSeconds = 0.0f;
		ConvergedEnemies.Reset();
		OnPanicOverloadStateChanged.Broadcast(CurrentState);
		return;
	}

	const TArray<TWeakObjectPtr<AEnemyBase>> QualifyingEnemies = GetHotUncontrolledEnemiesNearby();

	if (QualifyingEnemies.Num() < OvercrowdCrowdThreshold)
	{
		UncontrolledSeconds = 0.0f;
		return;
	}

	UncontrolledSeconds += DeltaSeconds;

	if (UncontrolledSeconds >= OvercrowdUncontrolledDurationSeconds)
	{
		// Flip before broadcasting (see MusicSubsystem::SetMusicState) so a
		// re-entrant listener sees CurrentState already updated.
		CurrentState = EPanicOverloadState::Active;
		ConvergedEnemies = QualifyingEnemies;
		OnPanicOverloadStateChanged.Broadcast(CurrentState);
	}
}

TArray<TWeakObjectPtr<AEnemyBase>> UOvercrowdDetectionComponent::GetHotUncontrolledEnemiesNearby() const
{
	TArray<TWeakObjectPtr<AEnemyBase>> Result;

	const AActor* Owner = GetOwner();
	if (!Owner || !GetWorld())
	{
		return Result;
	}

	const FVector OwnerLocation = Owner->GetActorLocation();
	const float RadiusSquared = FMath::Square(OvercrowdRadiusUnits);

	for (TActorIterator<AEnemyBase> It(GetWorld()); It; ++It)
	{
		const EEnemyState State = It->GetEnemyState();
		const bool bHotUncontrolled = State == EEnemyState::Alert || State == EEnemyState::Attack;
		if (!bHotUncontrolled)
		{
			continue;
		}
		if (FVector::DistSquared(It->GetActorLocation(), OwnerLocation) <= RadiusSquared)
		{
			Result.Add(TWeakObjectPtr<AEnemyBase>(*It));
		}
	}
	return Result;
}

bool UOvercrowdDetectionComponent::HasConvergedEnemyBeenControlled() const
{
	for (const TWeakObjectPtr<AEnemyBase>& Enemy : ConvergedEnemies)
	{
		if (const AEnemyBase* EnemyPtr = Enemy.Get())
		{
			if (EnemyPtr->GetEnemyState() == EEnemyState::Controlled)
			{
				return true;
			}
		}
	}
	return false;
}

void UOvercrowdDetectionComponent::NotifyLevelReached(int32 LevelIndex)
{
	const FOvercrowdLevelThreshold* Found = LevelThresholds.FindByPredicate(
		[LevelIndex](const FOvercrowdLevelThreshold& Entry) { return Entry.LevelIndex == LevelIndex; });

	if (!Found)
	{
		if (!LevelThresholds.IsEmpty())
		{
			UE_LOG(LogTemp, Warning,
				TEXT("UOvercrowdDetectionComponent::NotifyLevelReached: no LevelThresholds entry for level %d on '%s'."),
				LevelIndex, *GetNameSafe(this));
		}
		return;
	}

	OvercrowdCrowdThreshold = Found->CrowdThreshold;
	OvercrowdRadiusUnits = Found->RadiusUnits;
	OvercrowdUncontrolledDurationSeconds = Found->UncontrolledDurationSeconds;
	UncontrolledSeconds = 0.0f;
}
