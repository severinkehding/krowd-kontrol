// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Paper2DPrototypePawn.generated.h"

class USceneComponent;
class UPaperSpriteComponent;
class UFloatingPawnMovement;
class USpringArmComponent;
class UCameraComponent;
class UInputComponent;
class UPlayerEnergyComponent;
class UPunishmentManagerComponent;

// Minimal Paper2D prototype pawn for PRD 14 REQ-1's Paper2D-vs-flat-camera-3D pipeline
// comparison (issue #55). A sprite driven by WASD/arrow input in world-space top-down
// movement, with a camera locked to a genuine orthographic top-down projection via a
// non-collision-testing spring arm. Does not itself decide Paper2D vs flat-camera-3D -
// that's a human call made by comparing this against the companion flat-camera-3D
// prototype (issue #56, merged as PR #102; see FlatCamera3DPrototypePawn.h/.cpp in this
// same module).
UCLASS()
class KROWDKONTROL_API APaper2DPrototypePawn : public APawn
{
	GENERATED_BODY()

public:
	APaper2DPrototypePawn();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Paper2DPrototype")
	TObjectPtr<USceneComponent> PawnRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Paper2DPrototype")
	TObjectPtr<UPaperSpriteComponent> SpriteComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Paper2DPrototype")
	TObjectPtr<UFloatingPawnMovement> MovementComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Paper2DPrototype")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Paper2DPrototype")
	TObjectPtr<UCameraComponent> TopDownCamera;

	// Makes real per-hit energy tracking (issue #78) reachable from this pawn, so
	// UEnergyMeterWidget::BindToEnergyComponent() (issue #132) and
	// AEnemyBase::FindPlayerEnergyComponent() have a live component to find.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Paper2DPrototype")
	TObjectPtr<UPlayerEnergyComponent> PlayerEnergyComponent;

	// Punishment-trigger plumbing (issue #177, PRD "Punishment System" REQ-1) -
	// fires OnPunishmentTriggered whenever PlayerEnergyComponent reports real
	// contact damage. No punishment effect is applied by this pawn; future
	// issues bind their own listeners to this component's delegate.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Paper2DPrototype")
	TObjectPtr<UPunishmentManagerComponent> PunishmentManagerComponent;

	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

private:
	void MoveForward(float Value);
	void MoveRight(float Value);
};
