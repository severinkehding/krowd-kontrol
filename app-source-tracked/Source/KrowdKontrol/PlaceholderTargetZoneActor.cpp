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
	BeaconLightComponent->SetLightColor(FLinearColor(0.2f, 1.0f, 0.3f));
	BeaconLightComponent->SetIntensity(3000.0f);
	BeaconLightComponent->SetAttenuationRadius(300.0f);
}
