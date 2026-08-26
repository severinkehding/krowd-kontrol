#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "AbilitySlot.h"
#include "CrowdMasterySubsystem.generated.h"

class AEnemyBase;

// docs/prd-run-lifecycle.md REQ-5, issue #174: keeps a running max of simultaneously-Controlled
// AEnemyBase instances for the current level ("Crowd Mastery"). World-scoped (not a
// UGameInstanceSubsystem like ULevelClearTimeSubsystem) because it needs to re-scan
// this world's AEnemyBase population via TActorIterator, mirroring
// UAbilityCastComponent::FindNearestValidTarget's identical scan idiom - same "no
// existing cross-actor registry" rationale ULevelLifecycleSubsystem.h documents for
// its own TActorIterator-based polling. Event/call-driven, not tick-driven: the
// issue's AC specifies sampling "on each OnAbilityCastApplied event and on
// Controlled-state expiry", so this is a plain UWorldSubsystem, not a
// UTickableWorldSubsystem - no per-frame cost, and the Automation Framework test can
// drive it deterministically the same way KrowdKontrolLevelLifecycleSubsystemTest.cpp
// drives ULevelLifecycleSubsystem::RefreshLevelClearState() directly rather than
// through a real Tick() loop.
//
// Subscribes for real to ULevelLifecycleSubsystem::OnLevelBegin in Initialize() (forces
// the sibling to construct/Initialize() first via Collection.InitializeDependency(),
// rather than assuming FSubsystemCollectionBase's registration order happens to work
// out) to reset the running max. HandleAbilityCastApplied/HandleEnemyControlledExpired
// are bound to every real UAbilityCastComponent/AEnemyBase instance's own delegate
// from that instance's own BeginPlay() (UAbilityCastComponent::BeginPlay,
// AEnemyBase::BeginPlay) - not from here, since this subsystem has no registry of
// either and world subsystems Initialize() before any actor/component in the level
// has begun play.
UCLASS()
class KROWDKONTROL_API UCrowdMasterySubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	// Re-scans this world's AEnemyBase population via TActorIterator and raises
	// RunningMaxControlledCount if the current count of Controlled instances exceeds
	// it. Public so both real production callers and the Automation Framework test
	// can drive it directly - same "public so tests can drive it deterministically"
	// rationale ULevelLifecycleSubsystem::RefreshLevelClearState() documents.
	UFUNCTION(BlueprintCallable, Category = "Crowd Mastery")
	void SampleControlledCount();

	UFUNCTION(BlueprintCallable, Category = "Crowd Mastery")
	int32 GetRunningMaxControlledCount() const { return RunningMaxControlledCount; }

	// Bound to every real UAbilityCastComponent instance's OnAbilityCastApplied in
	// UAbilityCastComponent::BeginPlay(). Ability/TargetEnemy are unused - every call
	// just re-samples the whole world's current Controlled count, same as
	// HandleEnemyControlledExpired below.
	UFUNCTION()
	void HandleAbilityCastApplied(EAbilitySlot Ability, AEnemyBase* TargetEnemy);

	// Bound to every real AEnemyBase instance's OnEnemyControlledExpired in
	// AEnemyBase::BeginPlay().
	UFUNCTION()
	void HandleEnemyControlledExpired();

	// Matches ULevelLifecycleSubsystem::FOnLevelBegin's signature exactly. Bound for
	// real in Initialize() below.
	UFUNCTION()
	void HandleLevelBegin(FName MapName);

	// Bound to every real ULevelLifecycleSubsystem::OnLevelClear in Initialize() below.
	// Deposits this level's RunningMaxControlledCount into this world's GameInstance's
	// UCrowdMasteryTotalSubsystem (PRD "Crowd Mastery Persistence" REQ-1, issue #327).
	UFUNCTION()
	void HandleLevelClear();

private:
	int32 RunningMaxControlledCount = 0;

	// One-shot guard so a still-missing GameInstance/UCrowdMasteryTotalSubsystem only
	// logs once per instance, not once per HandleLevelClear() call - same idiom
	// ULevelLifecycleSubsystem::bHasWarnedMissingLevelClearTimeSubsystem documents.
	bool bHasWarnedMissingCrowdMasteryTotalSubsystem = false;
};
