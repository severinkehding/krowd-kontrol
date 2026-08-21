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
class UAbilityLockoutComponent;
class UAbilityCastVFXComponent;
class UGizmoFirstContactComponent;
class UFirstStunBeaconComponent;
class UAbilityMatchupSignalComponent;
class UAbilityMatchupNudgeComponent;
class UPunishmentManagerComponent;
class USpeedReductionPunishmentComponent;
class UPunishmentArbitrationComponent;
class ULevelFailComponent;

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

	// Grants the Automation Framework test direct access to the 5 private Cast*Ability
	// wrappers below, so a headless test can confirm each wrapper forwards to its own
	// EAbilitySlot rather than only indirectly through SetupPlayerInputComponent's
	// BindAction registrations - same rationale UAbilityCastComponent's
	// FKrowdKontrolAbilityCastComponentTest friendship documents.
	friend class FKrowdKontrolFlatCamera3DAbilityCastWiringTest;

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

	// Camera framing (issue #188, PRD "Level Playability & Presentation" REQ-4) -
	// EditAnywhere so designers can retune "feels far away / hard to read" without a
	// C++ recompile. Ranges below are the newly documented defaults' valid bounds;
	// KrowdKontrol.Unit.FlatCamera3DPipelineCameraFraming asserts both the bounds and
	// that these properties genuinely drive CameraBoom/TopDownCamera via
	// ApplyCameraFraming() below, not just at construction time.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlatCamera3DPrototype|Camera", meta = (ClampMin = "300.0", ClampMax = "600.0"))
	float CameraArmLength = 450.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlatCamera3DPrototype|Camera", meta = (ClampMin = "-75.0", ClampMax = "-45.0"))
	float CameraBoomPitch = -60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlatCamera3DPrototype|Camera", meta = (ClampMin = "60.0", ClampMax = "90.0"))
	float CameraFieldOfView = 75.0f;

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

	// Punishment-trigger plumbing (issue #177, PRD "Punishment System" REQ-1) -
	// fires OnPunishmentTriggered whenever PlayerEnergyComponent reports real
	// contact damage. No punishment effect is applied by this pawn; future
	// issues bind their own listeners to this component's delegate.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FlatCamera3DPrototype")
	TObjectPtr<UPunishmentManagerComponent> PunishmentManagerComponent;

	// Punishment 2 (issue #179, PRD "Punishment System" REQ-3) - reduces this
	// pawn's MovementComponent->MaxSpeed for a fixed duration whenever
	// PunishmentManagerComponent reports a trigger, then restores it. Wired to
	// this pawn's own MovementComponent and PunishmentManagerComponent in the
	// constructor below.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FlatCamera3DPrototype")
	TObjectPtr<USpeedReductionPunishmentComponent> SpeedReductionPunishmentComponent;

	// Level-fail signal plumbing (issue #171, PRD "Run Lifecycle & Progression Signals"
	// REQ-3) - fires OnLevelFailed exactly once when PlayerEnergyComponent's energy
	// reaches 0. AKrowdKontrolPlayerController::WireWidgetsToPawn is the consumer; see
	// that class for what happens on fire (input disabled, level timer discarded).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FlatCamera3DPrototype")
	TObjectPtr<ULevelFailComponent> LevelFailComponent;

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

	// Punishment 1 (issue #178, PRD "Punishment System" REQ-2) - locks the most
	// recently cast ability (Stun fallback if none yet) for a fixed duration whenever
	// PunishmentManagerComponent reports a trigger. Bound to
	// AbilityCastComponent->OnAbilityCastApplied and
	// PunishmentManagerComponent->OnPunishmentTriggered in the constructor below; the
	// gate itself is consulted by AbilityCastComponent::TryCastAbility.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FlatCamera3DPrototype")
	TObjectPtr<UAbilityLockoutComponent> AbilityLockoutComponent;

	// Single-active-punishment arbitration (issue #180, PRD "Punishment System
	// (Punishments 1 & 2 + arbitration)" REQ-4) - the sole listener bound to
	// PunishmentManagerComponent->OnPunishmentTriggered; decides which of
	// AbilityLockoutComponent/SpeedReductionPunishmentComponent (if either) actually
	// activates, and force-ends both the instant Overcrowd (resolved lazily in its own
	// BeginPlay()) goes Active. Wired in the constructor below, after both
	// AbilityLockoutComponent and SpeedReductionPunishmentComponent already exist.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FlatCamera3DPrototype")
	TObjectPtr<UPunishmentArbitrationComponent> PunishmentArbitrationComponent;

	// Ability-side colour telegraph (issue #67) - flashes AbilityData::Get(Ability)
	// .Colour at the cast target's location whenever AbilityCastComponent's
	// OnAbilityCastApplied fires. Bound in the constructor below.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FlatCamera3DPrototype")
	TObjectPtr<UAbilityCastVFXComponent> AbilityCastVFXComponent;

	// New Stun-triggered narrative hook (issue #59) - fires the first-contact Gizmo
	// bark exactly once, the first time the player successfully casts Stun. Bound in
	// the constructor below, alongside AbilityCastVFXComponent.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FlatCamera3DPrototype")
	TObjectPtr<UGizmoFirstContactComponent> GizmoFirstContactComponent;

	// New target-zone beacon hook (issue #29) - intensifies the nearest
	// APlaceholderTargetZoneActor's beacon exactly once, the first time the player
	// successfully casts Stun. Bound in the constructor below, alongside
	// GizmoFirstContactComponent.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FlatCamera3DPrototype")
	TObjectPtr<UFirstStunBeaconComponent> FirstStunBeaconComponent;

	// New ability-vs-enemy colour-matchup instrumentation hook (issue #37, PRD 09
	// REQ-5) - classifies every successful cast as colour-matched or not against the
	// target's real EEnemyType. Bound in the constructor below, alongside the other
	// OnAbilityCastApplied subscribers. No player-facing behavior yet - this issue is
	// detection-only; see AbilityMatchupSignalComponent.h.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FlatCamera3DPrototype")
	TObjectPtr<UAbilityMatchupSignalComponent> AbilityMatchupSignalComponent;

	// New onboarding nudge hook (issue #40, PRD 09 REQ-5 / PRD 02 REQ-4) - shows a
	// brief on-screen reminder the first time the player reaches 3 consecutive
	// non-colour-matched successful casts. Bound in the constructor below, to
	// AbilityMatchupSignalComponent's own OnAbilityMatchupSignal delegate rather than
	// AbilityCastComponent's OnAbilityCastApplied directly.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FlatCamera3DPrototype")
	TObjectPtr<UAbilityMatchupNudgeComponent> AbilityMatchupNudgeComponent;

	// Applies CameraArmLength/CameraBoomPitch/CameraFieldOfView onto
	// CameraBoom/TopDownCamera. Called once from the constructor (CDO + freshly
	// spawned instances) and again from PostEditChangeProperty below (placed-instance
	// Details-panel edits) - the single source of truth for "property -> component",
	// so KrowdKontrol.Unit.FlatCamera3DPipelineCameraFraming can call it directly to
	// prove the wiring is real, not test-only.
	void ApplyCameraFraming();

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

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
