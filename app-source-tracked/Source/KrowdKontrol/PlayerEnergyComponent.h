#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PlayerEnergyComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEnergyChanged, float, NewEnergy);

// Tracks the player's energy (health) resource and enforces, by construction, that
// energy can only ever be reduced through ApplyContactDamage() (PRD 01 REQ-4: energy
// must only decrease from enemy contact, never from casting an ability). Each hit's
// raw damage is clamped to MaxDamagePerHit before being applied (REQ-6: enemy count,
// not per-hit damage, is the difficulty lever). See issue #78.
//
// ApplyContactDamage is the ONLY public method permitted to reduce CurrentEnergy - no
// other public mutator (setter, BlueprintCallable, or otherwise) may ever be added to
// this class. Doing so would reopen the exact hole this component exists to close.
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class KROWDKONTROL_API UPlayerEnergyComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPlayerEnergyComponent();

	// Ceiling CurrentEnergy is seeded to (on BeginPlay) and clamped to.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player Energy", meta = (ClampMin = "0.0"))
	float MaxEnergy = 100.0f;

	// Cap on how much a single ApplyContactDamage call can subtract, regardless of the
	// raw amount passed in.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player Energy", meta = (ClampMin = "0.0"))
	float MaxDamagePerHit = 10.0f;

	// Runtime state, not designer config - hence no EditDefaultsOnly/EditAnywhere.
	// BlueprintReadOnly (not BlueprintReadWrite) so no Blueprint graph can wire a
	// settable pin that would bypass ApplyContactDamage.
	UPROPERTY(BlueprintReadOnly, Category = "Player Energy")
	float CurrentEnergy = 0.0f;

	// Fires whenever ApplyContactDamage actually changes CurrentEnergy. Ready for a
	// future HUD to consume; nothing subscribes to it yet.
	UPROPERTY(BlueprintAssignable, Category = "Player Energy")
	FOnEnergyChanged OnEnergyChanged;

	// The sole public mutator of CurrentEnergy. Clamps RawAmount to
	// [0, MaxDamagePerHit], subtracts, clamps CurrentEnergy to [0, MaxEnergy], and
	// broadcasts OnEnergyChanged when the value actually changes. Returns the actual
	// amount subtracted (post-clamp).
	UFUNCTION(BlueprintCallable, Category = "Player Energy")
	float ApplyContactDamage(float RawAmount, AActor* DamageSource);

protected:
	virtual void BeginPlay() override;
};
