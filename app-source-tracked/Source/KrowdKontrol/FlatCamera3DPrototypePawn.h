// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "FlatCamera3DPrototypePawn.generated.h"

class UStaticMeshComponent;
class UFloatingPawnMovement;
class USpringArmComponent;
class UCameraComponent;
class UInputComponent;
class UAbilityUnlockComponent;

// Minimal flat-camera-3D prototype pawn for PRD 14 REQ-1's Paper2D-vs-flat-camera-3D
// pipeline comparison (issue #56). A primitive cube mesh driven by WASD/arrow input in
// world-space top-down movement, with a camera locked to a fixed top-down pitch via a
// non-collision-testing spring arm. Does not itself decide Paper2D vs flat-camera-3D -
// that's a human call made by comparing this against the companion Paper2D prototype
// (issue #55).
UCLASS()
class KROWDKONTROL_API AFlatCamera3DPrototypePawn : public APawn
{
	GENERATED_BODY()

public:
	AFlatCamera3DPrototypePawn();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FlatCamera3DPrototype")
	TObjectPtr<UStaticMeshComponent> MeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FlatCamera3DPrototype")
	TObjectPtr<UFloatingPawnMovement> MovementComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FlatCamera3DPrototype")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FlatCamera3DPrototype")
	TObjectPtr<UCameraComponent> TopDownCamera;

	// Makes the run's crowd-control unlock state (issue #69) reachable from the only
	// pawn placed in the project's actual playable level - this pawn was previously
	// the sole player pawn with no unlock tracking attached at all, so nothing could
	// ever observe unlock state during real play. Cast-permission/ability-tray wiring
	// to this component's state is not part of this pawn - see issue #71.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FlatCamera3DPrototype")
	TObjectPtr<UAbilityUnlockComponent> AbilityUnlockComponent;

	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

private:
	void MoveForward(float Value);
	void MoveRight(float Value);
};
