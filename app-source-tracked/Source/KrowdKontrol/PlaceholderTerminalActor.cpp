// Fill out your copyright notice in the Description page of Project Settings.

#include "PlaceholderTerminalActor.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

APlaceholderTerminalActor::APlaceholderTerminalActor()
{
	PrimaryActorTick.bCanEverTick = false;

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	RootComponent = MeshComponent;

	// Reuse the cylinder already used by PlaceholderTargetZoneActor rather than
	// introducing a third distinct placeholder mesh reference - a terminal reads at
	// least as plausibly as a squat cylinder as a cube would.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMeshFinder(
		TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMeshFinder.Succeeded())
	{
		MeshComponent->SetStaticMesh(CylinderMeshFinder.Object);
	}
}

void APlaceholderTerminalActor::Interact()
{
	if (TerminalLog.bHasBeenTriggered)
	{
		return;
	}

	TerminalLog.bHasBeenTriggered = true;
	OnTerminalLogRevealed.Broadcast(TerminalLog.BarkID, TerminalLog.Lines);
}
