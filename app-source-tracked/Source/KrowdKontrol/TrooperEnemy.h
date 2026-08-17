#pragma once

#include "CoreMinimal.h"
#include "EnemyBase.h"
#include "TrooperEnemy.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTrooperRayFired);

// TR-UPR: the 4th and final core enemy type (PRD 03, MISSION.md Hard Invariant 5), a
// medium-range rapid attacker. Extends AEnemyBase (issue #12) with a distinct standing
// Plane silhouette, a Teal glow that intensifies only when Root is the ability that put
// it into Controlled, a separate attack "tell" light, and a GetAttackRangeUnits()
// override strictly between ABomberEnemy's short range and ASniperEnemy's long range.
// Unlike both siblings, AdvanceAttackTelegraph re-arms itself after each ray fires
// instead of latching a fire-once guard, so it keeps firing at a fixed cadence for as
// long as it remains in Attack - the one mechanical trait ("rapid single-ray attacks")
// that sets it apart. See issue #14.
UCLASS()
class KROWDKONTROL_API ATrooperEnemy : public AEnemyBase
{
	GENERATED_BODY()

	// Grants the Automation Framework test direct access to AdvanceAttackTelegraph
	// below, so a headless test can drive deterministic telegraph timing without a
	// real per-frame Tick() loop - same rationale ASniperEnemy/ABomberEnemy's own
	// friendship documents.
	friend class FKrowdKontrolTrooperEnemyTest;

public:
	ATrooperEnemy();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Trooper")
	TObjectPtr<UStaticMeshComponent> MeshComponent;

	// Teal, intensifies on Root-triggered OnControlledEntry.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Trooper")
	TObjectPtr<UPointLightComponent> GlowLightComponent;

	// Non-reserved placeholder colour, on during Attack.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Trooper")
	TObjectPtr<UPointLightComponent> AttackTellLightComponent;

	// Rapid cadence - clearly shorter than Sniper's 1.2f and Bomber's 2.0f, reflecting
	// "rapid" in the PRD table.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Trooper", meta = (ClampMin = "0.0"))
	float AttackTelegraphSeconds = 0.4f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Trooper")
	float GlowBaselineIntensity = 800.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Trooper")
	float GlowIntensifiedIntensity = 2400.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Trooper")
	float AttackTellIntensity = 2000.0f;

	// Broadcasts repeatedly while Attack persists, unlike ASniperEnemy/ABomberEnemy's
	// fire-once delegates.
	UPROPERTY(BlueprintAssignable, Category = "Trooper")
	FOnTrooperRayFired OnTrooperRayFired;

protected:
	virtual float GetAttackRangeUnits() const override;
	virtual void OnControlledEntry(EAbilitySlot Ability) override;
	virtual void OnAttackEntry() override;
	virtual void Tick(float DeltaTime) override;

private:
	// Re-arms itself after each ray fires (no fire-once guard) - the one deliberate
	// divergence from ASniperEnemy/ABomberEnemy's own AdvanceAttackTelegraph.
	void AdvanceAttackTelegraph(float DeltaSeconds);

	float RemainingTelegraphSeconds = 0.0f;
};
