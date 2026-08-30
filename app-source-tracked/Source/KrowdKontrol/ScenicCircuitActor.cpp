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
	DistanceAlongPath += SpeedUnitsPerSecond * DeltaSeconds;
	BobPhaseSeconds += DeltaSeconds;
	ApplyPathPosition();
}

float AScenicCircuitActor::TotalPathLength() const
{
	float Total = 0.0f;
	for (int32 i = 0; i < Waypoints.Num(); ++i)
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
	float Remaining = FMath::Fmod(DistanceAlongPath, Total);
	if (Remaining < 0.0f)
	{
		Remaining += Total;
	}
	for (int32 i = 0; i < Waypoints.Num(); ++i)
	{
		const FVector& A = Waypoints[i];
		const FVector& B = Waypoints[(i + 1) % Waypoints.Num()];
		const float SegLength = FVector::Dist(A, B);
		if (Remaining > SegLength && SegLength > KINDA_SMALL_NUMBER)
		{
			Remaining -= SegLength;
			continue;
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
