// Fill out your copyright notice in the Description page of Project Settings.

#include "PlaceholderTargetZoneActor.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

APlaceholderTargetZoneActor::APlaceholderTargetZoneActor()
{
	PrimaryActorTick.bCanEverTick = false;

	// Plain, unscaled root - mirrors DoorConnectorActor.cpp's DoorConnectorRoot. The
	// column below must NOT be nested under BeaconMeshComponent: that component is
	// itself flattened to a 0.05 Z-scale disc (see below), and UE composes a child
	// component's relative transform through its parent's scale, so a tall column
	// parented directly to the disc would render squashed to ~5% of its intended
	// height instead of poking above a room's walls. Keeping BeaconMeshComponent and
	// BeaconColumnMeshComponent as siblings under a neutral root avoids that entirely.
	USceneComponent* TargetZoneRootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("TargetZoneRootComponent"));
	RootComponent = TargetZoneRootComponent;

	BeaconMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BeaconMeshComponent"));
	BeaconMeshComponent->SetupAttachment(TargetZoneRootComponent);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMeshFinder(
		TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMeshFinder.Succeeded())
	{
		BeaconMeshComponent->SetStaticMesh(CylinderMeshFinder.Object);
	}
	// Flatten the cylinder into a floor-marker disc rather than sourcing a real decal
	// asset - placeholder-first per MISSION.md Quality Standards.
	BeaconMeshComponent->SetRelativeScale3D(FVector(1.5f, 1.5f, 0.05f));

	BeaconColumnMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BeaconColumnMeshComponent"));
	BeaconColumnMeshComponent->SetupAttachment(TargetZoneRootComponent);
	if (CylinderMeshFinder.Succeeded())
	{
		BeaconColumnMeshComponent->SetStaticMesh(CylinderMeshFinder.Object);
	}
	// Thin-and-tall instead of flattened - a vertical marker that pokes above a room's
	// walls (BeaconColumnHeight > ARoomActor::RoomWallHeight) so the beacon is visible
	// from an adjacent room or across open floor, not just standing next to it.
	BeaconColumnMeshComponent->SetRelativeScale3D(FVector(0.15f, 0.15f, BeaconColumnHeight / 100.f));
	// Column's own pivot is centered on itself, so this puts its base at the root's
	// local origin (flush with the disc's floor level) and its top at BeaconColumnHeight.
	BeaconColumnMeshComponent->SetRelativeLocation(FVector(0.f, 0.f, BeaconColumnHeight * 0.5f));
	// No collision - a visual-only wayfinding marker must never block player movement,
	// mirroring DoorConnectorActor.cpp's DoorMarkerMeshComponent precedent (issue #191).
	BeaconColumnMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	BeaconLightComponent = CreateDefaultSubobject<UPointLightComponent>(TEXT("BeaconLightComponent"));
	BeaconLightComponent->SetupAttachment(BeaconColumnMeshComponent);
	// 50 is the *unscaled* engine cylinder's local half-height (its mesh vertices span
	// -50..+50, same "100uu unit size" convention DoorConnectorActor.cpp's marker
	// comment documents), not BeaconColumnHeight * 0.5f - the light is a child of
	// BeaconColumnMeshComponent, so its own relative location is already composed
	// through the column's BeaconColumnHeight/100 scale. Using the mesh-local half-
	// height here lands the light exactly on the column's rendered top face for any
	// BeaconColumnHeight, without hardcoding a height-dependent number that would
	// silently drift out of sync if the column's scale ever changes independently.
	BeaconLightComponent->SetRelativeLocation(FVector(0.f, 0.f, 50.f));

	// Saturated green, deliberately chosen because it is not one of MISSION.md Hard
	// Invariant 3's five reserved gameplay-information colours (Purple/Teal/Orange/
	// Blue/White - PRD 13 REQ-4 extends that reservation to world-space UI, not just
	// screen-space HUD chrome).
	// PLACEHOLDER COLOUR - see issue #72 review: this saturated green may itself
	// constitute a "6th saturated information colour" under Hard Invariant 3, since the
	// beacon's purpose is to carry gameplay information (target-zone location). Needs a
	// human design ruling before this placeholder is replaced with the real visual.
	// Left unchanged by issue #190 - that issue is scoped to visibility distance, not
	// colour policy, and silently changing it here would pre-empt a still-open,
	// human-ruling-required design question.
	BeaconLightComponent->SetLightColor(FLinearColor(0.2f, 1.0f, 0.3f));
	// Placeholder brightness/radius - not tuned against any real room scale yet.
	BeaconLightComponent->SetIntensity(BeaconBaselineIntensity);
	BeaconLightComponent->SetAttenuationRadius(900.0f);
}

void APlaceholderTargetZoneActor::IntensifyBeacon()
{
	BeaconLightComponent->SetIntensity(BeaconIntensifiedIntensity);
}
