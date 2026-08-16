#pragma once

#include "CoreMinimal.h"
#include "EnemyBase.h"
#include "BomberEnemy.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBomberExploded);

// B0-0MR: short-range explosive attacker (PRD 03). Extends AEnemyBase (issue #12)
// with a sphere silhouette, Fear-only Orange "core glow", attack-tell + telegraph,
// and a small GetAttackRangeUnits() (opposite of ASniperEnemy's). Unlike Sniper, the
// telegraph elapsing also drains UPlayerEnergyComponent via ApplyContactDamage() -
// clamped, floors at 0 - the "never lethal, crowd-drain" AC. See issue #15.
UCLASS()
class KROWDKONTROL_API ABomberEnemy : public AEnemyBase
{
	GENERATED_BODY()

	// Grants direct access to AdvanceAttackTelegraph for deterministic test timing -
	// same rationale ASniperEnemy's friendship documents.
	friend class FKrowdKontrolBomberEnemyTest;

public:
	ABomberEnemy();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bomber")
	TObjectPtr<UStaticMeshComponent> MeshComponent;

	// Orange, intensifies on Fear-triggered OnControlledEntry.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bomber")
	TObjectPtr<UPointLightComponent> CoreGlowLightComponent;

	// Non-reserved placeholder colour, on during Attack.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bomber")
	TObjectPtr<UPointLightComponent> AttackTellLightComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bomber", meta = (ClampMin = "0.0"))
	float AttackTelegraphSeconds = 2.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bomber")
	float CoreGlowBaselineIntensity = 800.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bomber")
	float CoreGlowIntensifiedIntensity = 2400.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bomber")
	float AttackTellIntensity = 2000.0f;
	// Deliberately large - non-lethality comes from ApplyContactDamage()'s clamp, not
	// from this value being small; the size makes that guarantee testable.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bomber")
	float ExplosionDamageAmount = 999.0f;

	// "Slow movement" AC sub-clause (issue #15, PRD 03's table). The Enemy AI state
	// machine is proximity-only today (see EnemyBase.h's TickCheckDetection) - no
	// movement/pathing component exists anywhere yet, so this isn't wired to a live
	// nav system. Exposed as a plain tunable, well below UCharacterMovementComponent's
	// engine default MaxWalkSpeed (600.0f) - the closest "normal" reference point
	// available, since ASniperEnemy (long-range; never needs to close distance) has no
	// comparable property of its own - so the clause is at least testably addressed
	// pending that integration.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bomber", meta = (ClampMin = "0.0"))
	float MovementSpeed = 200.0f;

	// Fires once the attack telegraph elapses.
	UPROPERTY(BlueprintAssignable, Category = "Bomber")
	FOnBomberExploded OnBomberExploded;

protected:
	virtual float GetAttackRangeUnits() const override;
	virtual void OnControlledEntry(EAbilitySlot Ability) override;
	virtual void OnAttackEntry() override;
	virtual void Tick(float DeltaTime) override;

private:
	void AdvanceAttackTelegraph(float DeltaSeconds);
	void TriggerExplosion();

	float RemainingTelegraphSeconds = 0.0f;
	bool bExplodedForCurrentAttack = false;
};
