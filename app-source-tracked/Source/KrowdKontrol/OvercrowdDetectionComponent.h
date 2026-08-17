#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "OvercrowdDetectionComponent.generated.h"

// Exactly 2 states: Inactive (default) and Active, one-directional in this issue's
// scope - recovery (Active -> Inactive) is deferred to a separate, later issue that
// depends on this state existing first. See MusicSubsystem.h's EMusicState for the
// mirrored enum/delegate placement convention.
UENUM(BlueprintType)
enum class EPanicOverloadState : uint8
{
	Inactive,
	Active
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPanicOverloadStateChanged, EPanicOverloadState, NewState);

// Detects "overcrowd" (PRD 08 Punishment 3, MISSION.md `08`, issue #16): counts how
// many hot-and-uncontrolled enemies (AEnemyBase::GetEnemyState() == Alert || Attack -
// explicitly NOT Controlled, unlike IThreatState::GetThreatState()'s Hot, which reads
// Controlled as Hot too) sit within OvercrowdRadiusUnits of this component's owner,
// and flips CurrentState from Inactive to Active (firing OnPanicOverloadStateChanged
// exactly once) once that qualifying count has stayed at/above
// OvercrowdCrowdThreshold for OvercrowdUncontrolledDurationSeconds continuously.
//
// Attached to the player pawn (mirrors UPlayerEnergyComponent's placement - never
// hardcoded into a concrete pawn C++ class, wired up via Blueprint/editor instead),
// reading GetOwner()'s location directly rather than re-deriving "find the player
// pawn" the way AEnemyBase::FindPlayerEnergyComponent() has to.
//
// The state transition is one-directional in this issue's scope: once Active,
// CurrentState never reverts on its own - recovery is a separate, later issue. The
// pre-trigger duration timer still resets if the qualifying count drops below
// OvercrowdCrowdThreshold before the duration elapses; that is detection-arming
// logic, not recovery.
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class KROWDKONTROL_API UOvercrowdDetectionComponent : public UActorComponent
{
	GENERATED_BODY()

	// Grants the Automation Framework test direct access to AdvancePanicOverloadState
	// below, which isn't part of the public API - see the comment on
	// AdvancePanicOverloadState for why.
	friend class FKrowdKontrolOvercrowdDetectionComponentTest;

	// Same grant, for UOvercrowdAudioSubsystem's own test (issue #38), which also drives
	// this component to Active via AdvancePanicOverloadState rather than a real tick loop.
	// Non-transitive - see MusicSubsystem.h's friend-class comment for why each test class
	// needs its own explicit grant.
	friend class FKrowdKontrolOvercrowdAudioSubsystemTest;

public:
	UOvercrowdDetectionComponent();

	// Minimum simultaneous hot-and-uncontrolled enemies within OvercrowdRadiusUnits
	// required to arm the duration timer. Placeholder default, not a locked design
	// value - same rationale AbilityCooldownComponent::DefaultAbilityCooldownSeconds's
	// comment documents.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Overcrowd", meta = (ClampMin = "1"))
	int32 OvercrowdCrowdThreshold = 5;

	// Radius, in units, within which a hot-and-uncontrolled enemy counts toward
	// OvercrowdCrowdThreshold. Placeholder default, not a locked design value.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Overcrowd", meta = (ClampMin = "0.0"))
	float OvercrowdRadiusUnits = 800.0f;

	// Continuous duration, in seconds, the qualifying count must stay at/above
	// OvercrowdCrowdThreshold before CurrentState flips to Active. Placeholder
	// default, not a locked design value.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Overcrowd", meta = (ClampMin = "0.0"))
	float OvercrowdUncontrolledDurationSeconds = 2.0f;

	EPanicOverloadState GetPanicOverloadState() const { return CurrentState; }

	// Fires exactly once, on the transition into Active only.
	UPROPERTY(BlueprintAssignable, Category = "Overcrowd")
	FOnPanicOverloadStateChanged OnPanicOverloadStateChanged;

protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	// Re-evaluates the qualifying count, accumulates/resets UncontrolledSeconds, and
	// flips CurrentState once the sustained-duration requirement is met. Called every
	// frame from TickComponent() in real gameplay, and directly by the Automation test
	// (via friend, since it can't drive a live tick loop under the -nullrhi headless
	// run). Deliberately not public or BlueprintCallable - this is the only place the
	// state can be advanced; a Blueprint graph must never be able to force-trigger or
	// bypass the duration requirement.
	void AdvancePanicOverloadState(float DeltaSeconds);

	// Counts hot-and-uncontrolled AEnemyBase instances (Alert or Attack, never
	// Controlled) within OvercrowdRadiusUnits of GetOwner().
	int32 CountHotUncontrolledEnemiesNearby() const;

	EPanicOverloadState CurrentState = EPanicOverloadState::Inactive;

	// Resets to 0 the instant the qualifying count drops below OvercrowdCrowdThreshold.
	float UncontrolledSeconds = 0.0f;
};
