// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LevelLightingRigActor.generated.h"

class UDirectionalLightComponent;
class USkyLightComponent;

// Reusable baseline lighting rig for PRD "Level Playability & Presentation" REQ-2 /
// issue #186: L_Level01 and L_Level02 have no scene lighting at all - only per-actor
// accent point lights (enemy glow/trim, beacon, attack-tell) exist, leaving the level
// pitch black. This actor bundles a DirectionalLightComponent + SkyLightComponent only
// (deliberately not fog/PostProcessVolume - see the issue's own optional-scope note and
// the parent PRD's "no post-process grading beyond basic readability" out-of-scope
// line), tuned dim-but-readable and colour-locked away from
// ReservedGameplayColours::GetAll() (MISSION.md Hard Invariant 3), so the existing
// accent lights keep "popping" against it. Placeable directly into any gameplay level,
// including future Levels 3-5, as a single one-step placement.
UCLASS()
class KROWDKONTROL_API ALevelLightingRigActor : public AActor
{
	GENERATED_BODY()

public:
	ALevelLightingRigActor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lighting Rig")
	TObjectPtr<USceneComponent> RigRootComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lighting Rig")
	TObjectPtr<UDirectionalLightComponent> DirectionalLightComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lighting Rig")
	TObjectPtr<USkyLightComponent> SkyLightComponent;
};
