// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PlaceholderCubeActor.generated.h"

class UStaticMeshComponent;

// Minimal placeholder-first actor (MISSION.md Quality Standards): a bare cube using
// the engine's default mesh, standing in for gameplay elements before any real art
// is sourced. See issue #2.
UCLASS()
class KROWDKONTROL_API APlaceholderCubeActor : public AActor
{
	GENERATED_BODY()

public:
	APlaceholderCubeActor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Placeholder")
	TObjectPtr<UStaticMeshComponent> MeshComponent;
};
