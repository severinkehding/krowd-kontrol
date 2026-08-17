#pragma once

#include "CoreMinimal.h"
#include "EnemyBase.h"
#include "RunnerEnemy.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;
class UEnemyTypeIndicatorComponent;
class USoundBase;
class UAudioComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRunnerDrainFired);

// RU-NNR: the 4th and last core enemy type (PRD 03, MISSION.md Hard Invariant 5), a
// fast-running short-range drain-ray attacker. Extends AEnemyBase (issue #12) with a
// distinct elongated-cube "dart" silhouette (all 5 /Engine/BasicShapes/ primitives are
// otherwise claimed - see RunnerEnemy.cpp's constructor comment for why reusing Cube,
// at a scale no other actor uses, is still distinct from the other 3 core enemy
// types), a Purple "drain glow" that intensifies only when Snare is the ability that
// put it into Controlled, a separate attack "tell" light plus a telegraph countdown
// timer for the attack warning, and a fast GetMovementSpeedUnitsPerSecond() override
// (the opposite of ABomberEnemy's slow one). Follows ASniperEnemy/ABomberEnemy's
// fire-once attack cadence, not ATrooperEnemy's re-arming one. See issue #13.
UCLASS()
class KROWDKONTROL_API ARunnerEnemy : public AEnemyBase
{
	GENERATED_BODY()

	// Grants the Automation Framework test direct access to AdvanceAttackTelegraph
	// below, so a headless test can drive deterministic telegraph timing without a
	// real per-frame Tick() loop - same rationale ASniperEnemy/ABomberEnemy/
	// ATrooperEnemy's own friendship documents.
	friend class FKrowdKontrolRunnerEnemyTest;

public:
	ARunnerEnemy();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Runner")
	TObjectPtr<UStaticMeshComponent> MeshComponent;

	// Purple, intensifies on Snare-triggered OnControlledEntry.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Runner")
	TObjectPtr<UPointLightComponent> DrainGlowLightComponent;

	// Non-reserved placeholder colour, on during Attack.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Runner")
	TObjectPtr<UPointLightComponent> AttackTellLightComponent;

	// Colourblind-safe non-colour marker (PRD 13 REQ-7, issue #77) - "RU-NNR" text
	// floating above the mesh, independent of DrainGlowLightComponent's Purple.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Runner")
	TObjectPtr<UEnemyTypeIndicatorComponent> EnemyTypeIndicatorComponent;

	// Quick single-shot windup - distinct from Sniper's 1.2f/Bomber's 2.0f/Trooper's 0.4f.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Runner", meta = (ClampMin = "0.0"))
	float AttackTelegraphSeconds = 0.6f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Runner")
	float DrainGlowBaselineIntensity = 800.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Runner")
	float DrainGlowIntensifiedIntensity = 2400.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Runner")
	float AttackTellIntensity = 2000.0f;

	// Defaults to /Engine/EditorSounds/Notifications/CompileFailed (set in the
	// constructor via ConstructorHelpers::FObjectFinder, same pattern as
	// MeshComponent's CubeMeshFinder above) so issue #28's "a distinct sound effect
	// plays" AC is met out of the box, not left silent pending designer
	// configuration. Still placeholder-first (MISSION.md): a primitive built-in
	// engine chime standing in until a real, per-enemy-type-distinguishable sound is
	// sourced - deliberately a different built-in asset from ASniperEnemy's
	// 1kSineTonePing, ABomberEnemy's WhiteNoise, and ATrooperEnemy's CompileSuccess,
	// so RU-NNR's tell is audibly distinct from all 3 siblings. Still Blueprint/
	// Details-panel overridable.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Runner")
	TSoftObjectPtr<USoundBase> AttackTellSound;

	// Fast chase speed (issue #122's per-type override pattern), well above
	// AEnemyBase's 600 u/s base default - the opposite of ABomberEnemy's slow 200.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Runner", meta = (ClampMin = "0.0"))
	float MovementSpeed = 950.0f;

	// Fires once the attack telegraph elapses.
	UPROPERTY(BlueprintAssignable, Category = "Runner")
	FOnRunnerDrainFired OnRunnerDrainFired;

protected:
	virtual float GetAttackRangeUnits() const override;
	virtual float GetMovementSpeedUnitsPerSecond() const override;
	virtual void OnControlledEntry(EAbilitySlot Ability) override;
	virtual void OnAttackEntry() override;
	virtual void Tick(float DeltaTime) override;

private:
	void AdvanceAttackTelegraph(float DeltaSeconds);

	float RemainingTelegraphSeconds = 0.0f;
	bool bDrainFiredForCurrentAttack = false;

	// Test-visible via the existing FKrowdKontrolRunnerEnemyTest friend grant above -
	// set by OnAttackEntry() when AttackTellSound resolves, so the Automation test can
	// assert the audio cue actually spawned without querying real audio hardware.
	UPROPERTY()
	TObjectPtr<UAudioComponent> AttackTellAudioComponent;

	// Defensive one-shot guard, mirroring ASniperEnemy/ABomberEnemy/ATrooperEnemy's
	// bHasWarnedMissingAttackTellSound shape - currently unreachable a second time in
	// practice, since EnemyBase's linear state machine (EnemyBase.h) only ever calls
	// OnAttackEntry() once per instance. Kept so a future change that made Attack
	// re-enterable wouldn't silently start spamming this warning.
	bool bHasWarnedMissingAttackTellSound = false;
};
