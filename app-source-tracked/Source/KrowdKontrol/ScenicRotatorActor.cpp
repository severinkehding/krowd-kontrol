#include "ScenicRotatorActor.h"
#include "Components/StaticMeshComponent.h"

AScenicRotatorActor::AScenicRotatorActor()
{
	PrimaryActorTick.bCanEverTick = true;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("ScenicRotatorRoot"));
	SetRootComponent(Root);

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ScenicRotatorMeshComponent"));
	MeshComponent->SetupAttachment(Root);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshComponent->SetCollisionProfileName(TEXT("NoCollision"));
	MeshComponent->SetGenerateOverlapEvents(false);
	MeshComponent->SetCanEverAffectNavigation(false);
}

void AScenicRotatorActor::BeginPlay()
{
	Super::BeginPlay();
	ApplyProgress(0.0f);
}

void AScenicRotatorActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (DurationSeconds <= KINDA_SMALL_NUMBER)
	{
		return;
	}
	ElapsedSeconds += DeltaSeconds;
	float Alpha;
	if (PlayMode == EScenicRotatePlayMode::Once)
	{
		Alpha = FMath::Clamp(ElapsedSeconds / DurationSeconds, 0.0f, 1.0f);
	}
	else
	{
		// triangle wave: 0->1 over one duration, back 1->0 over the next, forever
		const float Cycle = FMath::Fmod(ElapsedSeconds, DurationSeconds * 2.0f);
		Alpha = Cycle <= DurationSeconds ? Cycle / DurationSeconds : 2.0f - Cycle / DurationSeconds;
	}
	ApplyProgress(Alpha);
}

void AScenicRotatorActor::ApplyProgress(float Alpha01)
{
	// smoothstep so the sweep settles at both extremes instead of snapping
	const float Eased = Alpha01 * Alpha01 * (3.0f - 2.0f * Alpha01);
	FRotator NewRotation;
	NewRotation.Pitch = FMath::Lerp(StartRotation.Pitch, EndRotation.Pitch, Eased);
	NewRotation.Yaw = FMath::Lerp(StartRotation.Yaw, EndRotation.Yaw, Eased);
	NewRotation.Roll = FMath::Lerp(StartRotation.Roll, EndRotation.Roll, Eased);
	MeshComponent->SetRelativeRotation(NewRotation);
}
