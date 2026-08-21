#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SpeedReductionPunishmentComponent.generated.h"

class UFloatingPawnMovement;

// Punishment 2 (issue #179, PRD "Punishment System" REQ-3): reduces the owning
// pawn's UFloatingPawnMovement::MaxSpeed by a configurable factor for a fixed
// duration whenever UPunishmentManagerComponent::OnPunishmentTriggered (issue
// #177) fires, then restores it. Re-triggering while already active refreshes
// the duration at the same reduced speed rather than compounding the factor -
// see HandlePunishmentTriggered()'s IsTimerActive guard. Single-active-
// punishment arbitration across this, ability-lockout, and Overcrowd is a
// separate, later issue (REQ-4); this component activates independently on
// every trigger until that lands.
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class KROWDKONTROL_API USpeedReductionPunishmentComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USpeedReductionPunishmentComponent();

	// Fraction of the pawn's original MaxSpeed retained while this punishment is
	// active - e.g. 0.5 = 50% speed (reduced by half). PRD REQ-3's own example.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Speed Reduction", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SpeedMultiplierWhileActive = 0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Speed Reduction", meta = (ClampMin = "0.0"))
	float SpeedReductionDurationSeconds = 3.0f;

	// Wired explicitly by the owning pawn's constructor (same idiom
	// MovementComponent->SetUpdatedComponent() and PunishmentManagerComponent's
	// AddDynamic wiring already use) - the concrete UFloatingPawnMovement whose
	// MaxSpeed this component reduces/restores. Runtime wiring, not designer
	// config - hence no EditDefaultsOnly/EditAnywhere, mirroring
	// PlayerEnergyComponent.h's identical rationale for its own runtime-only
	// fields.
	UPROPERTY()
	TObjectPtr<UFloatingPawnMovement> MovementComponent;

	// Bound to UPunishmentManagerComponent::OnPunishmentTriggered. If no
	// reduction is currently active, captures the real pre-punishment MaxSpeed
	// and applies SpeedMultiplierWhileActive. Either way, (re)starts the expiry
	// timer - SetTimer on an already-active handle replaces it, which is what
	// gives re-triggering its "refresh duration, don't compound" semantics.
	UFUNCTION()
	void HandlePunishmentTriggered();

	// Test-support accessor, same rationale as WaveSpawnerComponent::IsWaveTimerActive():
	// this harness never drives a real BeginPlay lifecycle, so the Automation test calls
	// EndPlay() directly to verify its timer cleanup and needs a way to observe the result.
	bool IsSpeedReductionTimerActive() const;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void RestoreOriginalSpeed();

	float OriginalMaxSpeed = 0.0f;
	FTimerHandle SpeedReductionTimerHandle;

	friend class FKrowdKontrolSpeedReductionPunishmentComponentTest;
};
