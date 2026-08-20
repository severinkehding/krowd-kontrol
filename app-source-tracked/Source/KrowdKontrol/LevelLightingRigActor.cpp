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

	DirectionalLightComponent = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("DirectionalLightComponent"));
	DirectionalLightComponent->SetupAttachment(RigRootComponent);

	// Desaturated, cool-neutral grey-blue. Deliberately NOT one of
	// ReservedGameplayColours::GetAll()'s five values (Purple/Teal/Orange/Blue/White -
	// MISSION.md Hard Invariant 3) and deliberately NOT pure white, so ambient scene
	// lighting can never visually read as the White=Stun signal. Matches MISSION.md's
	// "neon-noir: desaturated dark environments" identity and the parent PRD's
	// "lighting stays neutral/cool" instruction.
	// PLACEHOLDER COLOUR/INTENSITY - needs a human visual tuning pass in-editor, since
	// Automation tests run -nullrhi (harness/run_ue_automation.sh) and cannot validate
	// perceptual brightness, only structural/property correctness.
	DirectionalLightComponent->SetLightColor(FLinearColor(0.55f, 0.6f, 0.68f, 1.0f));
	DirectionalLightComponent->SetIntensity(1.5f);
	// Angled downward (negative pitch) - a conventional "sun" angle so it doesn't read
	// as a flat front-fill.
	DirectionalLightComponent->SetRelativeRotation(FRotator(-40.0f, -30.0f, 0.0f));

	SkyLightComponent = CreateDefaultSubobject<USkyLightComponent>(TEXT("SkyLightComponent"));
	SkyLightComponent->SetupAttachment(RigRootComponent);

	// Same non-reserved colour as the directional light, for a consistent ambient tone.
	SkyLightComponent->SetLightColor(FLinearColor(0.55f, 0.6f, 0.68f, 1.0f));
	// PLACEHOLDER - kept below the directional light's contribution so it reads as
	// ambient fill, not the dominant light source. Deliberately does not call
	// RecaptureSky(): that is a rendering-thread operation, unsafe/unnecessary under
	// the -nullrhi Automation test path this project's gate uses, and the Editor
	// auto-recaptures sky lights on level load/placement in normal use anyway.
	SkyLightComponent->SetIntensity(0.4f);
}
