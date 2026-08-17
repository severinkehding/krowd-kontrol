#pragma once

#include "CoreMinimal.h"
#include "EnemyBase.h"
#include "SniperEnemy.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;
class UEnemyTypeIndicatorComponent;
class USoundBase;
class UAudioComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSniperShotFired);

// SN-1PR: the first concrete core enemy type (PRD 03, MISSION.md Hard Invariant 5),
// a long-range sniper. Extends AEnemyBase (issue #12) with a distinct cone
// silhouette, a Blue-tinted "eye glow" light that intensifies only when Sleep is the
// ability that put it into Controlled, a separate attack "tell" light plus a
// telegraph countdown timer for the attack warning, a one-shot attack-tell audio cue
// (issue #36) fired from the same OnAttackEntry() extension point, and a large
// GetAttackRangeUnits() override so it enters Attack almost as soon as it's Alert -
// the mechanical definition of "long-range" in this state machine: it doesn't need to
// close distance first. See issue #17.
UCLASS()
class KROWDKONTROL_API ASniperEnemy : public AEnemyBase
{
	GENERATED_BODY()

	// Grants the Automation Framework test direct access to AdvanceAttackTelegraph
	// below, so a headless test can drive deterministic telegraph timing without a
	// real per-frame Tick() loop - same rationale UAbilityCooldownComponent's
	// friendship documents.
	friend class FKrowdKontrolSniperEnemyTest;

public:
	ASniperEnemy();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sniper")
	TObjectPtr<UStaticMeshComponent> MeshComponent;

	// Blue, intensifies on Sleep-triggered OnControlledEntry.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sniper")
	TObjectPtr<UPointLightComponent> EyeGlowLightComponent;

	// Non-reserved placeholder colour, on during Attack.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sniper")
	TObjectPtr<UPointLightComponent> AttackTellLightComponent;

	// Elite configuration (PRD 03 REQ-4, issue #19): non-reserved secondary trim
	// light, lit only while AEnemyBase::bIsElite is true - see
	// AEnemyBase::GetEliteTrimLightComponent()'s comment for why this is declared
	// per-subclass rather than once on AEnemyBase.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sniper")
	TObjectPtr<UPointLightComponent> EliteTrimLightComponent;

	// Colourblind-safe non-colour marker (PRD 13 REQ-7, issue #77) - "SN-1PR" text
	// floating above the mesh, independent of EyeGlowLightComponent's Blue.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sniper")
	TObjectPtr<UEnemyTypeIndicatorComponent> EnemyTypeIndicatorComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sniper", meta = (ClampMin = "0.0"))
	float AttackTelegraphSeconds = 1.2f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sniper")
	float EyeGlowBaselineIntensity = 800.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sniper")
	float EyeGlowIntensifiedIntensity = 2400.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sniper")
	float AttackTellIntensity = 2000.0f;

	// Defaults to /Engine/EngineSounds/1kSineTonePing (set in the constructor via
	// ConstructorHelpers::FObjectFinder, same pattern as MeshComponent's ConeMeshFinder
	// below) so issue #36's "a distinct sound effect plays" AC is met out of the box,
	// not left silent pending designer configuration - unlike
	// UMusicSubsystem::CalmTrack/CombatTrack, which are legitimately Config-driven
	// because a designer places real music later. This is still placeholder-first
	// (MISSION.md): a primitive built-in engine tone standing in until a real,
	// per-enemy-type-distinguishable sound is sourced. Still Blueprint/Details-panel
	// overridable.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sniper")
	TSoftObjectPtr<USoundBase> AttackTellSound;

	// Fires once the attack telegraph elapses.
	UPROPERTY(BlueprintAssignable, Category = "Sniper")
	FOnSniperShotFired OnSniperShotFired;

protected:
	virtual float GetAttackRangeUnits() const override;
	virtual UPointLightComponent* GetEliteTrimLightComponent() const override { return EliteTrimLightComponent; }
	virtual void OnControlledEntry(EAbilitySlot Ability) override;
	virtual void OnAttackEntry() override;
	virtual void Tick(float DeltaTime) override;

private:
	void AdvanceAttackTelegraph(float DeltaSeconds);

	float RemainingTelegraphSeconds = 0.0f;
	bool bShotFiredForCurrentAttack = false;

	// Test-visible via the existing FKrowdKontrolSniperEnemyTest friend grant above -
	// set by OnAttackEntry() when AttackTellSound resolves, so the Automation test can
	// assert the audio cue actually spawned without querying real audio hardware.
	UPROPERTY()
	TObjectPtr<UAudioComponent> AttackTellAudioComponent;

	// Defensive one-shot guard, mirroring UMusicSubsystem::bHasWarnedMissingTrack's shape -
	// currently unreachable a second time in practice, since EnemyBase's linear state
	// machine (EnemyBase.h) only ever calls OnAttackEntry() once per instance. Kept so a
	// future change that made Attack re-enterable wouldn't silently start spamming this
	// warning.
	bool bHasWarnedMissingAttackTellSound = false;
};
