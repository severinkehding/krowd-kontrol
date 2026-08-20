// Fill out your copyright notice in the Description page of Project Settings.

#include "LevelLightingRigActor.h"
#include "Components/SceneComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"

ALevelLightingRigActor::ALevelLightingRigActor()
{
	PrimaryActorTick.bCanEverTick = false;

	RigRootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RigRootComponent"));
	RootComponent = RigRootComponent;

	// Desaturated, cool-neutral grey-blue, shared by both lights below for a consistent
	// ambient tone. Deliberately NOT one of ReservedGameplayColours::GetAll()'s five
	// values (Purple/Teal/Orange/Blue/White - MISSION.md Hard Invariant 3) and
	// deliberately NOT pure white, so ambient scene lighting can never visually read as
	// the White=Stun signal. Matches MISSION.md's "neon-noir: desaturated dark
	// environments" identity and the parent PRD's "lighting stays neutral/cool"
	// instruction.
	// PLACEHOLDER COLOUR/INTENSITY - needs a human tuning pass in C++ (these are
	// VisibleAnywhere, not EditAnywhere, so there's nothing to drag in the Editor's
	// Details panel), since Automation tests run -nullrhi (harness/run_ue_automation.sh)
	// and cannot validate perceptual brightness, only structural/property correctness.
	const FLinearColor BaselineLightingColour(0.55f, 0.6f, 0.68f, 1.0f);

	DirectionalLightComponent = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("DirectionalLightComponent"));
	DirectionalLightComponent->SetupAttachment(RigRootComponent);
	DirectionalLightComponent->SetLightColor(BaselineLightingColour);
	DirectionalLightComponent->SetIntensity(1.5f);
	// Angled downward (negative pitch) - a conventional "sun" angle so it doesn't read
	// as a flat front-fill.
	DirectionalLightComponent->SetRelativeRotation(FRotator(-40.0f, -30.0f, 0.0f));

	SkyLightComponent = CreateDefaultSubobject<USkyLightComponent>(TEXT("SkyLightComponent"));
	SkyLightComponent->SetupAttachment(RigRootComponent);
	SkyLightComponent->SetLightColor(BaselineLightingColour);
	// PLACEHOLDER - kept below the directional light's contribution so it reads as
	// ambient fill, not the dominant light source. Deliberately does not call
	// RecaptureSky(): that is a rendering-thread operation, unsafe/unnecessary under
	// the -nullrhi Automation test path this project's gate uses, and the Editor
	// auto-recaptures sky lights on level load/placement in normal use anyway.
	SkyLightComponent->SetIntensity(0.4f);
}
