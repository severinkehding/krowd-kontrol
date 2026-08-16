#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BossBase.generated.h"

// Idle -> Armed -> Vulnerable -> Banked, with Banked as the only reachable
// "defeated" state (no HP-bar-to-zero kill path exists - there is no other
// enum value to reach). See issue #44, PRD 04, MISSION.md Hard Invariant 2.
UENUM(BlueprintType)
enum class EBossState : uint8
{
	Idle,
	Armed,
	Vulnerable,
	Banked
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBossBanked);

// Shared, structurally-safe foundation for every boss encounter (3 mid-bosses +
// Drain, PRD 04): a linear Idle->Armed->Vulnerable->Banked state machine plus
// generic shield/split/enrage flags with subclass-overridable hooks. A mid-boss
// subclass calls only AdvanceToArmed()/AdvanceToVulnerable()/TransitionToBanked()
// and overrides the hooks it needs - it never has to re-derive or risk violating
// the "defeat always ends in Banked, never a kill" guarantee. Abstract: never
// placed/spawned directly, only subclassed. See issue #44.
UCLASS(Abstract)
class KROWDKONTROL_API ABossBase : public AActor
{
	GENERATED_BODY()

public:
	ABossBase();

	// Fires exactly once, on the transition into Banked.
	UPROPERTY(BlueprintAssignable, Category = "Boss")
	FOnBossBanked OnBossBanked;

	EBossState GetBossState() const { return CurrentState; }

	// No-ops unless CurrentState == Idle.
	UFUNCTION(BlueprintCallable, Category = "Boss")
	void AdvanceToArmed();

	// No-ops unless CurrentState == Armed.
	UFUNCTION(BlueprintCallable, Category = "Boss")
	void AdvanceToVulnerable();

	// No-ops unless CurrentState == Vulnerable. Terminal - once reached, every
	// transition method above becomes permanently a no-op, since none of their
	// guard conditions can ever match Banked again.
	UFUNCTION(BlueprintCallable, Category = "Boss")
	void TransitionToBanked();

	UFUNCTION(BlueprintCallable, Category = "Boss|Shield")
	void SetHasShield(bool bNewHasShield);
	bool HasShield() const { return bHasShield; }

	UFUNCTION(BlueprintCallable, Category = "Boss|Split")
	void SetIsSplit(bool bNewIsSplit);
	bool IsSplit() const { return bIsSplit; }

	UFUNCTION(BlueprintCallable, Category = "Boss|Enrage")
	void SetIsEnraged(bool bNewIsEnraged);
	bool IsEnraged() const { return bIsEnraged; }

protected:
	// C++-only (not BlueprintNativeEvent) until a real mid-boss subclass exists to
	// inform whether these hooks need Blueprint override - same rationale
	// ThreatState.h and Herdable.h document for their own extension points.
	virtual void OnShieldChanged(bool bNewHasShield) {}
	virtual void OnSplitChanged(bool bNewIsSplit) {}
	virtual void OnEnrageChanged(bool bNewIsEnraged) {}

private:
	EBossState CurrentState = EBossState::Idle;
	bool bHasShield = false;
	bool bIsSplit = false;
	bool bIsEnraged = false;
};
