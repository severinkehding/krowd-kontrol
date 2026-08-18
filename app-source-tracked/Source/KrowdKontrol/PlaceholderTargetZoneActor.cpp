// Fill out your copyright notice in the Description page of Project Settings.

#include "PlaceholderTargetZoneActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

APlaceholderTargetZoneActor::APlaceholderTargetZoneActor()
{
	PrimaryActorTick.bCanEverTick = false;

	BeaconMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BeaconMeshComponent"));
	RootComponent = BeaconMeshComponent;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMeshFinder(
		TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMeshFinder.Succeeded())
	{
		BeaconMeshComponent->SetStaticMesh(CylinderMeshFinder.Object);
	}
	// Flatten the cylinder into a floor-marker disc rather than sourcing a real decal
	// asset - placeholder-first per MISSION.md Quality Standards.
	BeaconMeshComponent->SetRelativeScale3D(FVector(1.5f, 1.5f, 0.05f));

	BeaconLightComponent = CreateDefaultSubobject<UPointLightComponent>(TEXT("BeaconLightComponent"));
	BeaconLightComponent->SetupAttachment(BeaconMeshComponent);

	// Saturated green, deliberately chosen because it is not one of MISSION.md Hard
	// Invariant 3's five reserved gameplay-information colours (Purple/Teal/Orange/
	// Blue/White - PRD 13 REQ-4 extends that reservation to world-space UI, not just
	// screen-space HUD chrome).
	// PLACEHOLDER COLOUR - see issue #72 review: this saturated green may itself
	// constitute a "6th saturated information colour" under Hard Invariant 3, since the
	// beacon's purpose is to carry gameplay information (target-zone location). Needs a
	// human design ruling before this placeholder is replaced with the real visual.
	BeaconLightComponent->SetLightColor(FLinearColor(0.2f, 1.0f, 0.3f));
	// Placeholder brightness/radius - not tuned against any real room scale yet.
	BeaconLightComponent->SetIntensity(BeaconBaselineIntensity);
	BeaconLightComponent->SetAttenuationRadius(300.0f);
}

void APlaceholderTargetZoneActor::IntensifyBeacon()
{
	BeaconLightComponent->SetIntensity(BeaconIntensifiedIntensity);
}
