#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "AbilityUnlockLevelSubsystem.generated.h"

// Issue #217, PRD "Level Progression & Teaching Arc" REQ-1 (ability-unlock half):
// UAbilityUnlockComponent::NotifyLevelReached already holds the correct merged
// level->ability mapping (2->Sleep, 3->Root, 4->Fear, 5->Snare, issue #69) but nothing
// calls it in real play. Subscribes to ULevelLifecycleSubsystem::OnLevelBegin in
// Initialize() - mirrors UCrowdMasterySubsystem's identical precedent (world
// subsystems Initialize() before any actor has begun play, so this is safe regardless
// of whether OnWorldBeginPlay fires before or after actor BeginPlay) - and, on fire,
// derives the level index from the map name and forwards it to the possessed pawn's
// UAbilityUnlockComponent.
//
// LevelIndex source (issue #217 Notes): no level-sequence config exists yet (tracked
// separately - "Advance to next level on level-clear per a configured level sequence",
// issue #216) - this uses the map's own name as the interim per-map source, parsing
// the trailing digits from the project's established "L_LevelNN" naming (L_Level01,
// L_Level02 exist on disk today, per LevelLightingRigActor.h's class comment). Any
// non-matching map name (including the two prototype maps,
// L_FlatCamera3DPrototype/L_Paper2DPrototype) defaults to level 1, which
// UAbilityUnlockComponent::NotifyLevelReached already documents as a safe no-op -
// revisit ParseLevelIndexFromMapName() once issue #216's sequence config lands, per
// this issue's own Notes on reconciling the two.
UCLASS()
class KROWDKONTROL_API UAbilityUnlockLevelSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	// Matches ULevelLifecycleSubsystem::FOnLevelBegin's signature exactly. Bound for
	// real in Initialize() below. Resolves the current world's possessed pawn via
	// GetFirstPlayerController()->GetPawn() - reliable in real play since both
	// playable pawns self-possess via AutoPossessPlayer regardless of GameMode (see
	// AKrowdKontrolGameMode.h) - and forwards NotifyLevelReached() to its
	// UAbilityUnlockComponent, if any. Silently no-ops (with a one-shot warning) if no
	// pawn/component is found, e.g. a menu map with no playable pawn.
	UFUNCTION()
	void HandleLevelBegin(FName MapName);

	// Parses the numeric suffix of an "L_LevelNN" map name into a level index (e.g.
	// "L_Level02" -> 2). PIE session name-mangling is stripped first via
	// UWorld::RemovePIEPrefix(), the same idiom AKrowdKontrolPlayerController's
	// StripPIEPrefixFromMapName() already uses for map-name comparisons elsewhere in
	// this module. Any non-matching name returns 1 -
	// UAbilityUnlockComponent::NotifyLevelReached's documented level-1 no-op. Public +
	// static so the Automation Framework test can assert specific map-name -> index
	// mappings directly, without needing a live World.
	UFUNCTION(BlueprintCallable, Category = "Ability Unlock")
	static int32 ParseLevelIndexFromMapName(FName MapName);

	// Called by AKrowdKontrolPlayerController::BeginPlay()/OnPossess() when a pawn
	// becomes possessed. OnLevelBegin fires exactly once per world
	// (ULevelLifecycleSubsystem::bHasFiredLevelBegin), so if HandleLevelBegin ran
	// before AutoPossessPlayer resolved (order isn't guaranteed - see
	// AKrowdKontrolPlayerController::BeginPlay()'s own comment on this), the unlock
	// would otherwise be skipped for the rest of this world's lifetime. No-ops if
	// HandleLevelBegin already found a pawn+component, or hasn't fired yet.
	void RetryPendingUnlockForPawn(APawn* PossessedPawn);

private:
	// One-shot guard so a level with no possessed pawn/AbilityUnlockComponent (e.g. a
	// future menu map added to the lifecycle-eligible set) only logs once per world
	// instance, matching AKrowdKontrolPlayerController::bHasWarnedMissingLevelClearTimeSubsystem's
	// identical idiom.
	bool bHasWarnedMissingAbilityUnlockComponent = false;

	// Set when HandleLevelBegin finds no possessed pawn/component, so a later
	// RetryPendingUnlockForPawn() call can still deliver this level's unlock once a
	// pawn is possessed.
	bool bLevelBeginFiredWithNoPawn = false;
	int32 PendingLevelIndex = 1;
};
