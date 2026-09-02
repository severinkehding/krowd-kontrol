#include "ScenicCircuitActor.h"
#include "Components/StaticMeshComponent.h"

AScenicCircuitActor::AScenicCircuitActor()
{
	PrimaryActorTick.bCanEverTick = true;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("ScenicCircuitRoot"));
	SetRootComponent(Root);

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ScenicMeshComponent"));
	MeshComponent->SetupAttachment(Root);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshComponent->SetCollisionProfileName(TEXT("NoCollision"));
	MeshComponent->SetGenerateOverlapEvents(false);
	MeshComponent->SetCanEverAffectNavigation(false);
}

void AScenicCircuitActor::BeginPlay()
{
	Super::BeginPlay();
	DistanceAlongPath = StartOffsetAlongPath;
	ApplyPathPosition();
}

void AScenicCircuitActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (Waypoints.Num() < 2 || SpeedUnitsPerSecond <= 0.0f)
	{
		return;
	}
	SpeedUnitsPerSecond += AccelerationUnitsPerSecond2 * DeltaSeconds;
	DistanceAlongPath += SpeedUnitsPerSecond * DeltaSeconds;
	BobPhaseSeconds += DeltaSeconds;
	ApplyPathPosition();
}

float AScenicCircuitActor::TotalPathLength() const
{
	float Total = 0.0f;
	const int32 SegmentCount = bLoop ? Waypoints.Num() : Waypoints.Num() - 1;
	for (int32 i = 0; i < SegmentCount; ++i)
	{
		Total += FVector::Dist(Waypoints[i], Waypoints[(i + 1) % Waypoints.Num()]);
	}
	return Total;
}

void AScenicCircuitActor::ApplyPathPosition()
{
	if (Waypoints.Num() < 2)
	{
		return;
	}
	const float Total = TotalPathLength();
	if (Total <= KINDA_SMALL_NUMBER)
	{
		return;
	}
	float Remaining;
	if (bLoop)
	{
		Remaining = FMath::Fmod(DistanceAlongPath, Total);
		if (Remaining < 0.0f)
		{
			Remaining += Total;
		}
	}
	else
	{
		// one-shot: park exactly at the final waypoint once the path is spent
		Remaining = FMath::Min(DistanceAlongPath, Total);
	}
	const int32 SegmentCount = bLoop ? Waypoints.Num() : Waypoints.Num() - 1;
	for (int32 i = 0; i < SegmentCount; ++i)
	{
		const FVector& A = Waypoints[i];
		const FVector& B = Waypoints[(i + 1) % Waypoints.Num()];
		const float SegLength = FVector::Dist(A, B);
		const bool bLastSegment = i == SegmentCount - 1;
		if (Remaining > SegLength && SegLength > KINDA_SMALL_NUMBER && !(!bLoop && bLastSegment))
		{
			Remaining -= SegLength;
			continue;
		}
		if (!bLoop && bLastSegment)
		{
			Remaining = FMath::Min(Remaining, SegLength);
		}
		const FVector Direction = SegLength > KINDA_SMALL_NUMBER ? (B - A) / SegLength : FVector::ForwardVector;
		FVector NewLocation = A + Direction * Remaining;
		if (BobAmplitudeUnits > 0.0f)
		{
			NewLocation.Z += FMath::Sin(BobPhaseSeconds * BobFrequencyHz * 2.0f * PI) * BobAmplitudeUnits;
		}
		SetActorLocation(NewLocation);
		if (bFaceTravelDirection && !Direction.IsNearlyZero())
		{
			FRotator Facing = Direction.Rotation();
			Facing.Yaw += YawOffsetDegrees;
			Facing.Pitch = 0.0f;
			Facing.Roll = 0.0f;
			SetActorRotation(Facing);
		}
		return;
	}
}
