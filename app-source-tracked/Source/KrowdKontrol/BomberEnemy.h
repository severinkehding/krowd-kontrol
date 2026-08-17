#pragma once

#include "CoreMinimal.h"
#include "EnemyBase.h"
#include "BomberEnemy.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;
class UEnemyTypeIndicatorComponent;
class USoundBase;
class UAudioComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBomberExploded);

// B0-0MR: short-range explosive attacker (PRD 03). Extends AEnemyBase (issue #12)
// with a sphere silhouette, Fear-only Orange "core glow", attack-tell + telegraph,
// a one-shot attack-tell audio cue (issue #33) fired from the same OnAttackEntry()
// extension point, and a small GetAttackRangeUnits() (opposite of ASniperEnemy's).
// Unlike Sniper, the telegraph elapsing also drains UPlayerEnergyComponent via
// ApplyContactDamage() - clamped, floors at 0 - the "never lethal, crowd-drain" AC.
// See issue #15.
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

	// Colourblind-safe non-colour marker (PRD 13 REQ-7, issue #77) - "B0-0MR" text
	// floating above the mesh, independent of CoreGlowLightComponent's Orange.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bomber")
	TObjectPtr<UEnemyTypeIndicatorComponent> EnemyTypeIndicatorComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bomber", meta = (ClampMin = "0.0"))
	float AttackTelegraphSeconds = 2.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bomber")
	float CoreGlowBaselineIntensity = 800.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bomber")
	float CoreGlowIntensifiedIntensity = 2400.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bomber")
	float AttackTellIntensity = 2000.0f;

	// Defaults to /Engine/EngineSounds/WhiteNoise (set in the constructor via
	// ConstructorHelpers::FObjectFinder, same pattern as MeshComponent's
	// SphereMeshFinder below) so issue #33's "a distinct sound effect plays" AC is met
	// out of the box, not left silent pending designer configuration - unlike
	// UMusicSubsystem::CalmTrack/CombatTrack, which are legitimately Config-driven
	// because a designer places real music later. This is still placeholder-first
	// (MISSION.md): a primitive built-in engine noise-burst standing in until a real,
	// per-enemy-type-distinguishable sound is sourced - deliberately a different
	// built-in asset from ASniperEnemy's AttackTellSound (1kSineTonePing) so B0-0MR's
	// tell is audibly distinct. Still Blueprint/Details-panel overridable.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bomber")
	TSoftObjectPtr<USoundBase> AttackTellSound;

	// Deliberately large - non-lethality comes from ApplyContactDamage()'s clamp, not
	// from this value being small; the size makes that guarantee testable.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bomber")
	float ExplosionDamageAmount = 999.0f;

	// "Slow movement" AC sub-clause (issue #15, PRD 03's table), genuinely wired into
	// AEnemyBase::TickChaseMovement via GetMovementSpeedUnitsPerSecond() below (issue
	// #122) - no longer a declared-but-inert value. Well below
	// UCharacterMovementComponent's engine default MaxWalkSpeed (600.0f, also
	// AEnemyBase::GetMovementSpeedUnitsPerSecond()'s own base default), the closest
	// "normal" reference point available, since ASniperEnemy (long-range; never needs
	// to close distance) deliberately keeps that base default rather than declaring
	// its own override.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bomber", meta = (ClampMin = "0.0"))
	float MovementSpeed = 200.0f;

	// Fires once the attack telegraph elapses.
	UPROPERTY(BlueprintAssignable, Category = "Bomber")
	FOnBomberExploded OnBomberExploded;

protected:
	virtual float GetAttackRangeUnits() const override;
	virtual float GetMovementSpeedUnitsPerSecond() const override;
	virtual void OnControlledEntry(EAbilitySlot Ability) override;
	virtual void OnAttackEntry() override;
	virtual void Tick(float DeltaTime) override;

private:
	void AdvanceAttackTelegraph(float DeltaSeconds);
	void TriggerExplosion();

	float RemainingTelegraphSeconds = 0.0f;
	bool bExplodedForCurrentAttack = false;

	// Test-visible via the existing FKrowdKontrolBomberEnemyTest friend grant above -
	// set by OnAttackEntry() when AttackTellSound resolves, so the Automation test can
	// assert the audio cue actually spawned without querying real audio hardware.
	UPROPERTY()
	TObjectPtr<UAudioComponent> AttackTellAudioComponent;

	// Defensive one-shot guard, mirroring ASniperEnemy::bHasWarnedMissingAttackTellSound's
	// shape - currently unreachable a second time in practice, since EnemyBase's linear
	// state machine (EnemyBase.h) only ever calls OnAttackEntry() once per instance. Kept
	// so a future change that made Attack re-enterable wouldn't silently start spamming
	// this warning.
	bool bHasWarnedMissingAttackTellSound = false;
};
