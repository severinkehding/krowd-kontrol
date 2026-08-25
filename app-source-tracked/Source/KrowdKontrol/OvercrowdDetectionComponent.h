#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "OvercrowdDetectionComponent.generated.h"

class AEnemyBase;

// Exactly 2 states: Inactive (default) and Active. Active reverts to Inactive once
// recovery is satisfied (issue #18, PRD 08 REQ-2) - see UOvercrowdDetectionComponent's
// class comment below for the exact recovery condition. See MusicSubsystem.h's
// EMusicState for the mirrored enum/delegate placement convention.
UENUM(BlueprintType)
enum class EPanicOverloadState : uint8
{
	Inactive,
	Active
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPanicOverloadStateChanged, EPanicOverloadState, NewState);

// One per-level override for UOvercrowdDetectionComponent's 3 trigger thresholds
// (PRD 08 REQ-1, issue #23): NotifyLevelReached(LevelIndex) looks up the entry
// whose LevelIndex matches and overwrites OvercrowdCrowdThreshold/RadiusUnits/
// UncontrolledDurationSeconds with it. Mirrors FWaveEntry (WaveSpawnerComponent.h)
// - an embedded, EditDefaultsOnly config struct owned by the component that reads
// it, not a separate UDataAsset (no precedent for one in this codebase yet).
USTRUCT(BlueprintType)
struct FOvercrowdLevelThreshold
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Overcrowd", meta = (ClampMin = "1"))
	int32 LevelIndex = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Overcrowd", meta = (ClampMin = "1"))
	int32 CrowdThreshold = 5;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Overcrowd", meta = (ClampMin = "0.0"))
	float RadiusUnits = 800.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Overcrowd", meta = (ClampMin = "0.0"))
	float UncontrolledDurationSeconds = 2.0f;
};

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
// Recovery (issue #18, PRD 08 REQ-2): the instant CurrentState flips to Active, the
// qualifying enemy set at that moment is snapshotted into ConvergedEnemies - "the
// current convergence." While Active, every AdvancePanicOverloadState() call checks
// whether any surviving member of ConvergedEnemies now reports
// AEnemyBase::GetEnemyState() == Controlled (i.e. a CC ability landed on it via
// ReceiveControl()). The first tick that's true, CurrentState reverts to Inactive,
// UncontrolledSeconds resets to 0.0f (the crowd must re-arm from scratch), and
// ConvergedEnemies is cleared. ConvergedEnemies is captured once and not refreshed
// while Active - an enemy that wanders into range afterward is not part of "the
// current convergence" and CC landed on it does not end Panic Overload. The
// pre-trigger duration timer still resets if the qualifying count drops below
// OvercrowdCrowdThreshold before the duration elapses; that is detection-arming
// logic, unrelated to recovery.
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

	// Same grant, for the per-level-threshold test (issue #23), which also drives
	// this component to Active via AdvancePanicOverloadState after calling
	// NotifyLevelReached - non-transitive, same rationale as the two grants above.
	friend class FKrowdKontrolOvercrowdLevelThresholdTest;

	// Same grant, for UOvercrowdVisualEffectSubsystem's own test (issue #20), which also
	// drives this component to Active via AdvancePanicOverloadState rather than a real
	// tick loop. Non-transitive - see MusicSubsystem.h's friend-class comment for why each
	// test class needs its own explicit grant.
	friend class FKrowdKontrolOvercrowdVisualEffectSubsystemTest;

	// Same grant, for the audio/visual sync test (issue #20), which drives this component
	// to Active once and asserts both UOvercrowdAudioSubsystem and
	// UOvercrowdVisualEffectSubsystem flip in step off the same broadcast.
	friend class FKrowdKontrolOvercrowdAudioVisualSyncTest;

	// Same grant, for the punishment-arbitration test (issue #180), which drives this
	// component to Active via AdvancePanicOverloadState to prove Overcrowd preempts
	// ability-lock/speed-reduction - non-transitive, same rationale as the grants above.
	friend class FKrowdKontrolPunishmentArbitrationComponentTest;

	// Same grant, for the punishment debug menu widget's own test (issue #26), which
	// drives this component to Active via AdvancePanicOverloadState to prove
	// ForceEndPanicOverload() ends it immediately - non-transitive, same rationale as
	// the grants above.
	friend class FKrowdKontrolPunishmentDebugMenuWidgetTest;

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

	// Per-level overrides for the 3 fields above. Empty by default - an empty array
	// makes NotifyLevelReached() a silent no-op, so existing placements that never
	// call it keep behaving exactly as they do today, off the 3 fields' own
	// EditDefaultsOnly values. Not required to cover every level; only levels that
	// need their own tuning need an entry here.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Overcrowd")
	TArray<FOvercrowdLevelThreshold> LevelThresholds;

	// Explicit level-progression signal a caller (today, an Automation test; later,
	// a real level-progression subsystem - same not-yet-built status as
	// UAbilityUnlockComponent::NotifyLevelReached's own caller, per that function's
	// header comment) invokes once per level reached. Looks up LevelThresholds for
	// LevelIndex and overwrites OvercrowdCrowdThreshold/OvercrowdRadiusUnits/
	// OvercrowdUncontrolledDurationSeconds with the match, resetting
	// UncontrolledSeconds to 0 (an in-progress accumulation measured against the old
	// thresholds is meaningless against the new ones). No match and a non-empty
	// LevelThresholds logs a warning and changes nothing, mirroring
	// UAbilityUnlockComponent::NotifyLevelReached's out-of-range warning. An empty
	// LevelThresholds is a silent no-op.
	UFUNCTION(BlueprintCallable, Category = "Overcrowd")
	void NotifyLevelReached(int32 LevelIndex);

	EPanicOverloadState GetPanicOverloadState() const { return CurrentState; }

	// Broadcasts once on every Inactive->Active transition and once on every
	// Active->Inactive recovery transition (issue #18); since recovery re-arms
	// detection rather than latching a "done" state, this pair can repeat any
	// number of times over the component's life. Not a lifetime-firing cap.
	UPROPERTY(BlueprintAssignable, Category = "Overcrowd")
	FOnPanicOverloadStateChanged OnPanicOverloadStateChanged;

	// Immediately reverts an Active Panic Overload to Inactive - resets UncontrolledSeconds
	// and ConvergedEnemies and broadcasts OnPanicOverloadStateChanged(Inactive). Guards on
	// CurrentState up front and returns early if already Inactive, the same guarded-early-return
	// shape as USpeedReductionPunishmentComponent::EndSpeedReduction(). Used by the punishment
	// debug menu (issue #26) to satisfy "existing active effects end immediately when toggled
	// off" without changing kk.Punishment.OvercrowdEnabled's own gate-only-new-activations
	// semantics (see IsOvercrowdEnabledByCVar() below).
	void ForceEndPanicOverload();

	// Whether kk.Punishment.OvercrowdEnabled currently allows this punishment to activate -
	// consulted only at the Inactive->Active transition inside AdvancePanicOverloadState().
	// Unlike UAbilityLockoutComponent::IsLockoutEnabledByCVar() - which PunishmentArbitrationComponent
	// calls directly to decide fallback routing to speed-reduction (issue #181) - this accessor has
	// no external caller: Overcrowd is priority-1 in arbitration, which only ever checks
	// IsOvercrowdActive() (state), never this CVar. The CVar itself is a file-scope static in this
	// component's own .cpp, so this accessor is still the only way another translation unit could
	// read it, if one ever needs to.
	static bool IsOvercrowdEnabledByCVar();

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

	// Returns the hot-and-uncontrolled AEnemyBase instances (Alert or Attack, never
	// Controlled) within OvercrowdRadiusUnits of GetOwner(). Returning the actual
	// actors (not just a count) lets the caller snapshot them into ConvergedEnemies
	// on the Inactive->Active transition.
	TArray<TWeakObjectPtr<AEnemyBase>> GetHotUncontrolledEnemiesNearby() const;

	// True if any surviving member of ConvergedEnemies (captured at the Inactive->Active
	// transition) currently reports GetEnemyState() == Controlled - i.e. a CC ability
	// (ReceiveControl) landed on it. Only meaningful while CurrentState == Active; the
	// caller (AdvancePanicOverloadState) only invokes this then.
	bool HasConvergedEnemyBeenControlled() const;

	EPanicOverloadState CurrentState = EPanicOverloadState::Inactive;

	// Snapshot of GetHotUncontrolledEnemiesNearby()'s result at the exact moment
	// CurrentState flipped to Active - "the current convergence" per PRD 08 REQ-2. Not
	// refreshed while Active; an enemy entering range afterward is not part of this
	// convergence and CC landed on it does not end Panic Overload. Cleared on the
	// Active->Inactive recovery transition.
	TArray<TWeakObjectPtr<AEnemyBase>> ConvergedEnemies;

	// Resets to 0 the instant the qualifying count drops below OvercrowdCrowdThreshold,
	// and also on the Active->Inactive recovery transition (issue #18), so the crowd
	// must re-arm from scratch either way.
	float UncontrolledSeconds = 0.0f;
};
