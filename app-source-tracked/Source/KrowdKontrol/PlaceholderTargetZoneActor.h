// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PlaceholderTargetZoneActor.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;

// Minimal placeholder-first actor (MISSION.md Quality Standards): a flattened mesh
// disc plus a point light standing in for a world-space target-zone beacon, before
// any real target-zone mechanic (detection radius, banking) exists. This is not the
// real ATargetZone that RoomEnemyBudgetController.h's comments already reserve for a
// future "OnActorBanked" integration - this class only carries the beacon visual. See
// issue #72 and PRD 13 REQ-6.
UCLASS()
class KROWDKONTROL_API APlaceholderTargetZoneActor : public AActor
{
	GENERATED_BODY()

public:
	APlaceholderTargetZoneActor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Target Zone")
	TObjectPtr<UStaticMeshComponent> BeaconMeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Target Zone")
	TObjectPtr<UPointLightComponent> BeaconLightComponent;
};
