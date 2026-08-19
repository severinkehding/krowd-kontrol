#pragma once

#include "CoreMinimal.h"
#include "BossBase.h"
#include "RootSurgeBoss.generated.h"

class UPointLightComponent;
class UWaveSpawnerComponent;
class UPlayerEnergyComponent;

// Mid-boss 2 (PRD 04 Locked Design, issue #50): spawns TR-UPR adds via
// UWaveSpawnerComponent at an accelerated cadence (each Waves[i].DelaySeconds derived
// from an authored BaselineWaveDelaySeconds entry via WaveDelayAccelerationMultiplier,
// strictly < 1.0), runs its own longer-range attack telegraph independent of wave-spawn
// state (dealing UPlayerEnergyComponent damage the same way ABomberEnemy::
// TriggerExplosion() does), and advances EBossState to Vulnerable (one-way, via the
// inherited AdvanceToVulnerable()) the first time one of THIS boss's own
// WaveSpawnerComponent-spawned adds is found Controlled with EAbilitySlot::Root -
// forcing the player to split attention between the boss's own attack and Root-locking
// its adds under time pressure. Mirrors ASleepShieldBoss (mid-boss 1) and ADualZoneBoss
// (mid-boss 3): a new ABossBase subclass, no BossBase.h/.cpp changes, no
// TransitionToBanked() call of its own (herding to a TargetZone is not yet wired for
// any boss - see SleepShieldBoss.cpp's own deferral comment). See issue #50.
UCLASS()
class KROWDKONTROL_API ARootSurgeBoss : public ABossBase
{
	GENERATED_BODY()

	// Grants the Automation Framework test direct access to CheckVulnerableState/
	// AdvanceAttackTelegraph below, so a headless test can drive deterministic checks
	// without a real per-frame Tick() loop - same rationale ASleepShieldBoss's
	// FKrowdKontrolSleepShieldBossTest friendship documents.
	friend class FKrowdKontrolRootSurgeBossTest;

public:
	ARootSurgeBoss();

	virtual void BeginPlay() override;

	// Visual "arming" tell (issue #50 AC #3, PRD 04 REQ-2): lit the moment BeginPlay()
	// calls AdvanceToArmed(), mirroring ASleepShieldBoss::ShieldTellLightComponent/
	// ADualZoneBoss::ArmingTellLightComponent's off-by-default, RootComponent-standing-
	// in-for-a-mesh pattern. Non-reserved placeholder colour - see constructor.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Root Surge Boss")
	TObjectPtr<UPointLightComponent> ArmingTellLightComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Root Surge Boss")
	float ArmingTellIntensity = 3000.0f;

	// Spawns this boss's own TR-UPR adds (AC #1) - never a bespoke spawner. Populated
	// in the constructor from BaselineWaveDelaySeconds * WaveDelayAccelerationMultiplier.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Root Surge Boss")
	TObjectPtr<UWaveSpawnerComponent> WaveSpawnerComponent;

	// "As authored for a normal (non-boss) encounter" - placeholder values, not a
	// locked design value; only the accelerated-cadence relationship to
	// WaveDelayAccelerationMultiplier is AC-mandated (AC #5). Kept readable so the
	// test can independently recompute the expected accelerated delays.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Root Surge Boss")
	TArray<float> BaselineWaveDelaySeconds;

	// Must stay strictly < 1.0 - AC #5's literal, operator-clarified contract (config-
	// level comparison: every derived Waves[i].DelaySeconds < BaselineWaveDelaySeconds[i]).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Root Surge Boss", meta = (ClampMin = "0.0", ClampMax = "0.99"))
	float WaveDelayAccelerationMultiplier = 0.5f;

	// Longer range than any existing enemy (AC #2) - must exceed ASniperEnemy's
	// 1400.0f, the current max. NOT YET RANGE-GATED: AdvanceAttackTelegraph() fires on
	// a timer regardless of this value (no live player-distance check yet - this
	// module's headless Automation tests never drive a real PlayerController/
	// BeginPlay() pass, so a distance check would be untestable, same deferred-gating
	// scope limit ASleepShieldBoss/ADualZoneBoss's own attack logic defers) - authored
	// here so the number exists when that wiring lands, not because it currently does
	// anything. Unlike AEnemyBase::GetAttackRangeUnits() (EnemyBase.h), which is a real,
	// load-bearing Alert->Attack gate, this property is descriptive only.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Root Surge Boss", meta = (ClampMin = "0.0"))
	float AttackRangeUnits = 1800.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Root Surge Boss", meta = (ClampMin = "0.0"))
	float AttackTelegraphSeconds = 1.5f;

	// Exceeds UPlayerEnergyComponent::MaxDamagePerHit (10.0f) - deliberately, same
	// "non-lethality comes from the clamp, not this value" pattern as
	// ABomberEnemy::ExplosionDamageAmount.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Root Surge Boss", meta = (ClampMin = "0.0"))
	float AttackDamageAmount = 20.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Root Surge Boss")
	TObjectPtr<UPointLightComponent> AttackTellLightComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Root Surge Boss")
	float AttackTellIntensity = 2000.0f;

protected:
	virtual void Tick(float DeltaTime) override;

	// Re-evaluates HasRootLockedAdd() and calls AdvanceToVulnerable() (a one-way call,
	// safe to call repeatedly) the first time it finds a qualifying add. No-ops once
	// GetBossState() == EBossState::Banked, mirroring
	// ASleepShieldBoss::CheckShieldState()'s early-return guard.
	void CheckVulnerableState();

	// Decrements RemainingTelegraphSeconds while this boss is not Banked; on reaching
	// zero, lights AttackTellLightComponent, damages the player via
	// UPlayerEnergyComponent::ApplyContactDamage() (mirroring
	// ABomberEnemy::TriggerExplosion()), and re-arms immediately (rapid re-arm, no
	// fire-once guard - mirrors ATrooperEnemy::AdvanceAttackTelegraph()).
	void AdvanceAttackTelegraph(float DeltaSeconds);

private:
	// Iterates WaveSpawnerComponent->GetSpawnedActors() (this boss's own adds only,
	// not a global nearby-actor scan) looking for one Controlled with
	// EAbilitySlot::Root. State check strictly before the stale-tolerant
	// GetControllingAbility() read, mirroring
	// ASleepShieldBoss::HasNearbySleepControlledMinion()'s exact guard order.
	bool HasRootLockedAdd() const;

	// Same TActorIterator<APawn> body as AEnemyBase::FindPlayerEnergyComponent()
	// (EnemyBase.cpp), duplicated here because that method is protected on AEnemyBase
	// and not reachable from a ABossBase subclass - no shared free function exists to
	// call instead. Returns nullptr if no APawn carries a UPlayerEnergyComponent.
	UPlayerEnergyComponent* FindPlayerEnergyComponent() const;

	float RemainingTelegraphSeconds = 0.0f;
};
