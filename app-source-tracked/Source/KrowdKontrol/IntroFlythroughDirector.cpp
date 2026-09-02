#include "IntroFlythroughDirector.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

AIntroFlythroughDirector::AIntroFlythroughDirector()
{
	PrimaryActorTick.bCanEverTick = true;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("IntroDirectorRoot"));
	SetRootComponent(Root);

	CameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("IntroCameraComponent"));
	CameraComponent->SetupAttachment(Root);
}

void AIntroFlythroughDirector::BeginPlay()
{
	Super::BeginPlay();
	ApplyProgress(0.0f);
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		PC->SetViewTargetWithBlend(this, 0.0f);
	}
}

void AIntroFlythroughDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bTravelRequested)
	{
		return;
	}
	ElapsedSeconds += DeltaSeconds;

	ApplyProgress(FMath::Clamp(ElapsedSeconds / DurationSeconds, 0.0f, 1.0f));

	if (!bFadeStarted && ElapsedSeconds >= FadeStartSeconds)
	{
		bFadeStarted = true;
		if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
		{
			if (PC->PlayerCameraManager)
			{
				PC->PlayerCameraManager->StartCameraFade(0.0f, 1.0f, FadeDurationSeconds,
					FLinearColor::Black, /*bShouldFadeAudio=*/false, /*bHoldWhenFinished=*/true);
			}
		}
	}

	if (ElapsedSeconds >= DurationSeconds)
	{
		bTravelRequested = true;
		UWorld* World = GetWorld();
		// Real travel only in a real game world - the same guard (and reason)
		// as AKrowdKontrolPlayerController::RequestLevelRestart().
		if (World && World->IsGameWorld() && NextLevelMapName != NAME_None)
		{
			UGameplayStatics::OpenLevel(this, NextLevelMapName);
		}
	}
}

FVector AIntroFlythroughDirector::ComputePathPosition(float Alpha01) const
{
	if (Waypoints.Num() == 0)
	{
		return GetActorLocation();
	}
	if (Waypoints.Num() == 1)
	{
		return Waypoints[0];
	}
	// smoothstep the time parameter so the flight eases out as it reaches the
	// hangar mouth instead of stopping dead
	const float Eased = Alpha01 * Alpha01 * (3.0f - 2.0f * Alpha01);

	float Total = 0.0f;
	for (int32 i = 0; i < Waypoints.Num() - 1; ++i)
	{
		Total += FVector::Dist(Waypoints[i], Waypoints[i + 1]);
	}
	if (Total <= KINDA_SMALL_NUMBER)
	{
		return Waypoints[0];
	}
	float Remaining = Eased * Total;
	for (int32 i = 0; i < Waypoints.Num() - 1; ++i)
	{
		const float SegLength = FVector::Dist(Waypoints[i], Waypoints[i + 1]);
		if (Remaining > SegLength && i < Waypoints.Num() - 2)
		{
			Remaining -= SegLength;
			continue;
		}
		const FVector Direction = SegLength > KINDA_SMALL_NUMBER
			? (Waypoints[i + 1] - Waypoints[i]) / SegLength : FVector::ForwardVector;
		return Waypoints[i] + Direction * FMath::Min(Remaining, SegLength);
	}
	return Waypoints.Last();
}

void AIntroFlythroughDirector::ApplyProgress(float Alpha01)
{
	const FVector NewLocation = ComputePathPosition(Alpha01);
	SetActorLocation(NewLocation);
	SetActorRotation((FocusPoint - NewLocation).Rotation());
}
