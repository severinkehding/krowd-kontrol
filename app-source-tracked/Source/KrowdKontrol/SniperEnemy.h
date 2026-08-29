#pragma once

#include "CoreMinimal.h"
#include "EnemyBase.h"
#include "SniperEnemy.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;
class UEnemyTypeIndicatorComponent;
class USoundBase;
class UAudioComponent;
class UAbilityTargetingIndicatorComponent;

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

	// World-space "shot incoming" telegraph line from this sniper to the player -
	// issue #359. Layered alongside (never replacing) AttackTellLightComponent/
	// AttackTellSound above; reuses UAbilityTargetingIndicatorComponent's Line shape
	// kind exactly as UAbilityPressHoldComponent's own cursor-aim line does
	// (AbilityPressHoldComponent.cpp:30-43), refreshed every Tick while the
	// telegraph is active so it tracks the player's live position.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sniper")
	TObjectPtr<UAbilityTargetingIndicatorComponent> TelegraphIndicatorComponent;

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

	// Sniper engagement tunables (issue #362): all 4 first-pass balance numbers
	// introduced by the SN-1PR sniper-shot PRD (#358 damage, #359 telegraph, #360
	// chase speed/attack range) live in this class as EditDefaultsOnly UPROPERTYs -
	// AttackTelegraphSeconds below, plus AttackRangeUnits, ShotDamageAmount, and
	// MovementSpeed further down this file. None of these are final balance
	// decisions; each is called out below with its current value.
	// SN-1PR's first-pass telegraph duration (issue #359) - the countdown between
	// entering Attack and the shot actually firing. Subject to operator playtest
	// tuning.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sniper", meta = (ClampMin = "0.0"))
	float AttackTelegraphSeconds = 1.2f;

	// SN-1PR's first-pass attack-range value (issue #360's design context, formally
	// named here for #362) - deliberately close to DetectionRangeUnits's default
	// (1500.0f, inherited unchanged), so SN-1PR enters Attack almost immediately
	// after Alert without needing to close distance first - the mechanical
	// definition of "long-range" in this state machine. Subject to operator
	// playtest tuning.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sniper", meta = (ClampMin = "0.0"))
	float AttackRangeUnits = 1400.0f;

	// Report this type's own (Blueprint-tunable) telegraph so
	// AEnemyBase::GetAttackDurationSeconds() derives an Attack window no tuning value
	// can make the base timeout cut short and silently suppress the shot (PR #336
	// pass-2 escalation, HIGH finding; formula centralised in the base class per
	// pass-2 code-quality finding).
	virtual float GetAttackTelegraphSeconds() const override { return AttackTelegraphSeconds; }

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

	// SN-1PR's first-pass per-hit damage value (issue #358) - deliberately at or below
	// UPlayerEnergyComponent::MaxDamagePerHit (10.0f) so a landed shot always costs
	// exactly this amount, not the clamp ceiling (contrast ABomberEnemy::
	// ExplosionDamageAmount / ARootSurgeBoss::AttackDamageAmount, which both
	// deliberately exceed the clamp instead). Subject to operator playtest tuning.
	// Re-ported 2026-08-29 after the concurrent #387/#388 sniper work clobbered
	// PR #385's wiring out of the shared app/ (the E2E zero-damage finding's cause).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sniper", meta = (ClampMin = "0.0"))
	float ShotDamageAmount = 8.0f;

	// Issue #360: chase speed while closing distance back into range after a
	// range-break (AEnemyBase::TickChaseMovement, driven while Alert) - SN-1PR
	// previously had no override here since it never needed to chase (its attack range
	// almost equals its detection range - see GetAttackRangeUnits() below), so
	// AEnemyBase::GetMovementSpeedUnitsPerSecond()'s 600.0f base default sat unused.
	// Named and tunable like every other concrete type's own chase-speed property
	// (ABomberEnemy::MovementSpeed/ARunnerEnemy::MovementSpeed) rather than a bare
	// literal, and deliberately below both AEnemyBase's own base-class default (600.0f)
	// and the player pawn's own UFloatingPawnMovement::MaxSpeed (this project's
	// unmodified engine default, 1200.0f - no C++ override exists anywhere in this
	// module, confirmed by grep) so outrunning a chasing sniper is genuinely
	// achievable, not just nominally possible. Subject to operator playtest tuning.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sniper", meta = (ClampMin = "0.0"))
	float MovementSpeed = 300.0f;

	// Fires once the attack telegraph elapses.
	UPROPERTY(BlueprintAssignable, Category = "Sniper")
	FOnSniperShotFired OnSniperShotFired;

protected:
	virtual float GetAttackRangeUnits() const override;
	virtual float GetMovementSpeedUnitsPerSecond() const override;
	virtual UPointLightComponent* GetEliteTrimLightComponent() const override { return EliteTrimLightComponent; }
	virtual void OnControlledEntry(EAbilitySlot Ability) override;
	virtual void OnAttackEntry() override;
	virtual void OnControlledExpired() override;
	virtual void OnAttackExpired() override;
	virtual void Tick(float DeltaTime) override;

	// Issue #121: SN-1PR is specifically countered by Sleep with a 7s lock vs the 5s
	// baseline every other enemy/ability combination uses.
	virtual float GetControlledDurationOverrideSeconds(EAbilitySlot Ability) const override;

private:
	void AdvanceAttackTelegraph(float DeltaSeconds);

	// Shows TelegraphIndicatorComponent as a Line from this sniper to the live
	// player pawn's current location - no-ops safely (does not call Show()) if no
	// player pawn is currently resolvable via UGameplayStatics::GetPlayerPawn(),
	// which covers both "no UWorld yet" (bare NewObject<>() test doubles - silent,
	// benign) and "no PlayerController possessing a pawn" (a CreateNewMap() test
	// World with no controller wired up - logged once via
	// bHasWarnedMissingTelegraphTarget, since that case should never happen once
	// the sniper is genuinely in Attack) - see SniperEnemy.cpp's
	// UpdateTelegraphIndicator() GOTCHA comment for why
	// AEnemyBase::FindPlayerEnergyComponent() must NOT be used here instead.
	void UpdateTelegraphIndicator();

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

	// Warn-once guard for UpdateTelegraphIndicator()'s "real UWorld but no resolvable
	// player pawn" branch - deliberately does NOT cover the benign "no UWorld yet" case
	// (see UpdateTelegraphIndicator()'s GOTCHA comment), only the case that should never
	// happen once the sniper is genuinely in Attack.
	bool bHasWarnedMissingTelegraphTarget = false;
};
