#pragma once

#include "CoreMinimal.h"
#include "BossBase.h"
#include "DualZoneBoss.generated.h"

class ATargetZone;
class UPointLightComponent;

// Mid-boss 3 (PRD 04 Locked Design, issue #52): splits its fight room into two
// ATargetZone instances (ZoneA/ZoneB, wired per-placed-instance like
// ADoorConnectorActor::RoomA/RoomB) via the inherited Split hook
// (ABossBase::SetIsSplit), tracks each zone's independent banked-actor count, and
// enters the inherited Enrage state (ABossBase::SetIsEnraged) the moment the
// imbalance between the two counts exceeds EnrageImbalanceThreshold. Tests the
// spatial-routing skill (PRD 01 REQ-5) at boss scale. Depends on ABossBase (issue
// #44) and ATargetZone (issue #80), both merged; see issue #52.
UCLASS()
class KROWDKONTROL_API ADualZoneBoss : public ABossBase
{
	GENERATED_BODY()

public:
	ADualZoneBoss();

	virtual void BeginPlay() override;

	// Per-placed-instance zone references, not Blueprint-class-default values -
	// mirrors ADoorConnectorActor::RoomA/RoomB (DoorConnectorActor.h).
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Dual Zone Boss")
	TObjectPtr<ATargetZone> ZoneA;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Dual Zone Boss")
	TObjectPtr<ATargetZone> ZoneB;

	// Visual "arming" tell (issue #52 AC #3, PRD 04 REQ-2): lit the moment
	// BeginPlay() calls AdvanceToArmed(), mirroring
	// ASleepShieldBoss::ShieldTellLightComponent's off-by-default,
	// RootComponent-standing-in-for-a-mesh pattern (SleepShieldBoss.h/.cpp) - the
	// same placeholder-first shape this codebase already uses for every other boss
	// visual tell. Non-reserved placeholder colour - see constructor.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dual Zone Boss")
	TObjectPtr<UPointLightComponent> ArmingTellLightComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dual Zone Boss")
	float ArmingTellIntensity = 3000.0f;

	// abs(BankedCountA - BankedCountB) strictly greater than this triggers Enrage.
	// Placeholder value, not a locked design value - matches this codebase's
	// placeholder-first convention (see ATargetZone::ZoneCollisionComponent's own
	// placeholder extent).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dual Zone Boss")
	int32 EnrageImbalanceThreshold = 3;

	int32 GetBankedCountA() const { return BankedCountA; }
	int32 GetBankedCountB() const { return BankedCountB; }

protected:
	UFUNCTION()
	void HandleZoneABanked(AActor* BankedActor);

	UFUNCTION()
	void HandleZoneBBanked(AActor* BankedActor);

private:
	void RecheckImbalance();

	int32 BankedCountA = 0;
	int32 BankedCountB = 0;
};
