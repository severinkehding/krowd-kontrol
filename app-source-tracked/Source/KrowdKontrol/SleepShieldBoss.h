#pragma once

#include "CoreMinimal.h"
#include "BossBase.h"
#include "SleepShieldBoss.generated.h"

class UPointLightComponent;

// Mid-boss 1 (PRD 04 Locked Design, issue #48): periodically shields itself
// (ABossBase::SetHasShield, freely reversible) and is only vulnerable while at
// least one Sleep-controlled minion (AEnemyBase::GetEnemyState() == Controlled &&
// GetControllingAbility() == EAbilitySlot::Sleep) sits within
// ShieldDropRadiusUnits of this boss - forcing the player to herd normal enemies
// near the boss instead of fighting it directly. The FIRST time a qualifying
// minion is found, this also permanently advances EBossState to Vulnerable
// (ABossBase's Armed->Vulnerable edge has no reverse edge, by design - see
// BossBase.h) - after that, HasShield() keeps freely toggling with live minion
// proximity, but EBossState never reverts. See SleepShieldBoss.cpp for the full
// design rationale. Depends on ABossBase (issue #44) and
// AEnemyBase::GetControllingAbility() (issue #138, PR #149), both merged; see
// issue #48.
UCLASS()
class KROWDKONTROL_API ASleepShieldBoss : public ABossBase
{
	GENERATED_BODY()

	// Grants the Automation Framework test direct access to CheckShieldState below,
	// so a headless test can drive deterministic proximity checks without a real
	// per-frame Tick() loop - same rationale AEnemyBase's
	// FKrowdKontrolEnemyBaseTest/UOvercrowdDetectionComponent's
	// FKrowdKontrolOvercrowdDetectionComponentTest friendships document.
	friend class FKrowdKontrolSleepShieldBossTest;

public:
	ASleepShieldBoss();

	virtual void BeginPlay() override;

	// Radius, in units, within which a Sleep-controlled AEnemyBase drops this
	// boss's shield. Placeholder default, not a locked design value - same
	// rationale UOvercrowdDetectionComponent::OvercrowdRadiusUnits documents.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sleep Shield Boss", meta = (ClampMin = "0.0"))
	float ShieldDropRadiusUnits = 800.0f;

	// Visual "arming" tell (issue #48 AC #3, PRD 04 REQ-2): lit while HasShield()
	// is true, off while it is false. Non-reserved placeholder colour - see
	// constructor.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sleep Shield Boss")
	TObjectPtr<UPointLightComponent> ShieldTellLightComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sleep Shield Boss")
	float ShieldTellIntensity = 3000.0f;

	// Mirrors inherited HasShield()/GetBossState(), which are plain C++ accessors
	// with no UPROPERTY backing and so aren't visible to reflection-based inspection
	// (e.g. the MCP property toolset). Kept in sync from OnShieldChanged() (fires
	// only on an actual HasShield() change - see ABossBase::SetHasShield's guard)
	// and from BeginPlay()/CheckShieldState() (the only two places this subclass
	// advances boss state).
	UPROPERTY(BlueprintReadOnly, Category = "Sleep Shield Boss")
	bool bHasShieldReflected = false;

	UPROPERTY(BlueprintReadOnly, Category = "Sleep Shield Boss")
	EBossState BossStateReflected = EBossState::Idle;

protected:
	virtual void Tick(float DeltaTime) override;
	virtual void OnShieldChanged(bool bNewHasShield) override;

private:
	// Re-evaluates HasNearbySleepControlledMinion() every tick and calls
	// SetHasShield() unconditionally with the result (a continuous, freely-
	// reversible toggle); the first time it finds a qualifying minion, also calls
	// AdvanceToVulnerable() (a one-way call, safe to call repeatedly - see
	// ABossBase::AdvanceToVulnerable's own guard). No-ops once GetBossState() ==
	// EBossState::Banked, so a boss already delivered to a target zone stops
	// re-toggling its shield.
	void CheckShieldState();

	// Mirrors UOvercrowdDetectionComponent::GetHotUncontrolledEnemiesNearby()'s
	// TActorIterator<AEnemyBase> + inclusive (<=) DistSquared radius shape, but
	// filters on Controlled+Sleep instead of Alert/Attack, and early-outs on the
	// first match (no snapshot array needed here).
	bool HasNearbySleepControlledMinion() const;
};
