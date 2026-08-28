// Fill out your copyright notice in the Description page of Project Settings.

#include "PlaceholderTargetZoneActor.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "AbilityTargetingIndicatorComponent.h"

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
	TargetZoneRootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("TargetZoneRootComponent"));
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

	BankingRadiusIndicatorComponent = CreateDefaultSubobject<UAbilityTargetingIndicatorComponent>(TEXT("BankingRadiusIndicatorComponent"));
}

void APlaceholderTargetZoneActor::IntensifyBeacon()
{
	BeaconLightComponent->SetIntensity(BeaconIntensifiedIntensity);
}

void APlaceholderTargetZoneActor::ApplyChainColour(FLinearColor Colour)
{
	if (!ChainColourMaterialInstance)
	{
		static const TSoftObjectPtr<UMaterialInterface> BaseMaterialSoftPtr(
			FSoftObjectPath(TEXT("/Game/_Placeholder/Abilities/M_AbilityIndicator.M_AbilityIndicator")));
		UMaterialInterface* BaseMaterial = BaseMaterialSoftPtr.LoadSynchronous();
		if (!BaseMaterial)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("APlaceholderTargetZoneActor: failed to load placeholder material '%s' on '%s' - pole will keep its default tint."),
				*BaseMaterialSoftPtr.ToString(), *GetNameSafe(this));
			return;
		}
		ChainColourMaterialInstance = UMaterialInstanceDynamic::Create(BaseMaterial, this);
		if (!ChainColourMaterialInstance)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("APlaceholderTargetZoneActor: UMaterialInstanceDynamic::Create() returned null on '%s' - pole will keep its default tint."),
				*GetNameSafe(this));
			return;
		}
		if (BeaconMeshComponent)
		{
			BeaconMeshComponent->SetMaterial(0, ChainColourMaterialInstance);
		}
		if (BeaconColumnMeshComponent)
		{
			BeaconColumnMeshComponent->SetMaterial(0, ChainColourMaterialInstance);
		}
	}
	ChainColourMaterialInstance->SetVectorParameterValue(TEXT("Colour"), Colour);
	CurrentChainColour = Colour;
	if (BeaconLightComponent)
	{
		BeaconLightComponent->SetLightColor(Colour);
	}
}

void APlaceholderTargetZoneActor::ShowBankingRadiusIndicator(float RadiusUnits, FLinearColor Colour)
{
	if (!BankingRadiusIndicatorComponent)
	{
		return;
	}
	FAbilityIndicatorShapeSpec ShapeSpec;
	ShapeSpec.Kind = EAbilityIndicatorShapeKind::CircleAtActor;
	ShapeSpec.Origin = GetActorLocation();
	ShapeSpec.RangeUnits = RadiusUnits;
	BankingRadiusIndicatorComponent->Show(ShapeSpec, Colour);
}

void APlaceholderTargetZoneActor::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	EnsureBeaconHierarchy();
}

void APlaceholderTargetZoneActor::PostLoad()
{
	Super::PostLoad();
	// PostInitializeComponents never runs for actors deserialized into an editor
	// world, which is where the stale hierarchy is both observed (E2E holdout MCP
	// inspection) and persisted (map Save). Healing on PostLoad too means simply
	// opening and re-saving an affected map bakes the corrected hierarchy into the
	// .umap for good.
	EnsureBeaconHierarchy();
}

void APlaceholderTargetZoneActor::EnsureBeaconHierarchy()
{
	// PR #199 fix-pass 1 review: actors of this class already placed in L_Level01 before
	// this PR (issue #190) keep the pre-existing serialized RootComponent/AttachParent
	// from the *old* hierarchy (root = BeaconMeshComponent, the flattened disc; light
	// parented directly to it) even after the constructor above starts building the new
	// one - those are per-instance overrides baked into the saved map, and the
	// constructor's CreateDefaultSubobject/SetupAttachment calls only establish this
	// class's new defaults, they don't retroactively repoint an already-placed
	// instance's explicitly-serialized pointers. Force the correct hierarchy here,
	// unconditionally, so every instance - old or new - actually ends up with the light
	// on top of the column instead of silently keeping the old, wall-occludable,
	// disc-mounted position.
	//
	// Attachment must respect registration state: on the PostLoad path components are
	// not yet registered (SetupAttachment is the legal call there), on the
	// PostInitializeComponents path they are (AttachToComponent territory).
	if (!TargetZoneRootComponent)
	{
		return;
	}

	auto AttachTo = [](USceneComponent* Child, USceneComponent* Parent)
	{
		if (Child->IsRegistered())
		{
			Child->AttachToComponent(Parent, FAttachmentTransformRules::KeepRelativeTransform);
		}
		else
		{
			Child->SetupAttachment(Parent);
		}
	};

	if (RootComponent != TargetZoneRootComponent)
	{
		// The stale serialized root (BeaconMeshComponent on pre-#190 maps) is what
		// carries the actor's placed world transform - TargetZoneRootComponent is
		// constructor-fresh at identity. Swapping roots without copying location/
		// rotation teleported every legacy-placed marker (and the ATargetZone that
		// ARoomActor::EnsureBankingZonesWired() heals onto it) to the world origin,
		// which is exactly what KrowdKontrol.PIE.SerializedPlacedActorHealth guards.
		// Scale is deliberately NOT copied: the old root was the 0.05-Z-flattened
		// disc, and inheriting that scale onto the neutral root would squash the
		// whole hierarchy (see the constructor's sibling-not-nested comment).
		const FTransform StaleRootTransform =
			RootComponent ? RootComponent->GetComponentTransform() : FTransform::Identity;
		SetRootComponent(TargetZoneRootComponent);
		TargetZoneRootComponent->SetWorldLocationAndRotation(
			StaleRootTransform.GetLocation(), StaleRootTransform.GetRotation());
	}

	if (BeaconMeshComponent && BeaconMeshComponent->GetAttachParent() != TargetZoneRootComponent)
	{
		AttachTo(BeaconMeshComponent, TargetZoneRootComponent);
		// Same rationale as the light below: when this component was itself the stale
		// root, its "relative" transform is the actor's old world placement, which is
		// meaningless relative to the new root (it would compose to double the
		// placement). Re-apply the constructor's canonical disc pose.
		BeaconMeshComponent->SetRelativeLocation(FVector::ZeroVector);
		BeaconMeshComponent->SetRelativeRotation(FRotator::ZeroRotator);
		BeaconMeshComponent->SetRelativeScale3D(FVector(1.5f, 1.5f, 0.05f));
	}

	if (BeaconColumnMeshComponent && BeaconColumnMeshComponent->GetAttachParent() != TargetZoneRootComponent)
	{
		AttachTo(BeaconColumnMeshComponent, TargetZoneRootComponent);
	}

	if (BeaconLightComponent && BeaconLightComponent->GetAttachParent() != BeaconColumnMeshComponent)
	{
		AttachTo(BeaconLightComponent, BeaconColumnMeshComponent);
		// Re-apply the constructor's canonical offset (column-top, in the mesh-local
		// half-height convention explained above) rather than trusting whatever relative
		// location survived from the stale attachment - it was relative to a different
		// parent and has no reason to still be correct after re-parenting.
		BeaconLightComponent->SetRelativeLocation(FVector(0.f, 0.f, 50.f));
	}
}
