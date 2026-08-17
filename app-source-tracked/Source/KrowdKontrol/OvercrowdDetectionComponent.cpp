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
	const int32 QualifyingCount = CountHotUncontrolledEnemiesNearby();

	if (QualifyingCount < OvercrowdCrowdThreshold)
	{
		UncontrolledSeconds = 0.0f;
		return;
	}

	UncontrolledSeconds += DeltaSeconds;

	if (CurrentState == EPanicOverloadState::Inactive
		&& UncontrolledSeconds >= OvercrowdUncontrolledDurationSeconds)
	{
		// Flip before broadcasting (see MusicSubsystem::SetMusicState) so a re-entrant
		// listener sees CurrentState already updated.
		CurrentState = EPanicOverloadState::Active;
		OnPanicOverloadStateChanged.Broadcast(CurrentState);
	}
}

int32 UOvercrowdDetectionComponent::CountHotUncontrolledEnemiesNearby() const
{
	const AActor* Owner = GetOwner();
	if (!Owner || !GetWorld())
	{
		return 0;
	}

	const FVector OwnerLocation = Owner->GetActorLocation();
	const float RadiusSquared = FMath::Square(OvercrowdRadiusUnits);

	int32 Count = 0;
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
			++Count;
		}
	}
	return Count;
}
