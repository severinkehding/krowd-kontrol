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
class UPlayerEnergyComponent;
class UAbilityCooldownComponent;
class UAbilityCastComponent;

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

	// Makes real per-hit energy tracking (issue #78) reachable from this pawn, so
	// UEnergyMeterWidget::BindToEnergyComponent() (issue #132) and
	// AEnemyBase::FindPlayerEnergyComponent() have a live component to find.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FlatCamera3DPrototype")
	TObjectPtr<UPlayerEnergyComponent> PlayerEnergyComponent;

	// Backs the 5 ability-cast input bindings below (issue #138) - the only public
	// gate a real cast can be blocked by short of an unlocked ability with no eligible
	// target. See AbilityCooldownComponent.h: TryStartCooldown is the sole legal way
	// to start a cooldown, and UAbilityCastComponent is the only caller.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FlatCamera3DPrototype")
	TObjectPtr<UAbilityCooldownComponent> AbilityCooldownComponent;

	// The single production entry point that finally calls
	// AEnemyBase::ReceiveControl() from a real gameplay path (issue #138) - see
	// AbilityCastComponent.h.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FlatCamera3DPrototype")
	TObjectPtr<UAbilityCastComponent> AbilityCastComponent;

	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

private:
	void MoveForward(float Value);
	void MoveRight(float Value);

	// Thin wrappers, one per EAbilitySlot - BindAction requires a fixed no-argument
	// member-function signature, so these can't be collapsed into one parameterized
	// method without changing the input-binding approach entirely (out of scope).
	void CastStunAbility();
	void CastSleepAbility();
	void CastRootAbility();
	void CastFearAbility();
	void CastSnareAbility();
};
