#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "AbilitySlot.h"
#include "CrowdMasterySubsystem.generated.h"

class AEnemyBase;

// PRD 06 REQ-2, issue #174: keeps a running max of simultaneously-Controlled
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
// Subscribes for real to ULevelLifecycleSubsystem::OnLevelBegin in Initialize() (both
// are auto-instantiated per-world subsystems, so the sibling already exists in the
// collection by then) to reset the running max - the one piece of real production
// wiring this issue's AC requires. HandleAbilityCastApplied/
// HandleEnemyControlledExpired are NOT bound to any real UAbilityCastComponent/
// AEnemyBase instance by this issue - no registry of either exists yet (only one
// UAbilityCastComponent ever exists, bound to the player pawn at construction time;
// AEnemyBase has no spawn-hook to bind per-instance delegates from a world
// subsystem). A future wiring issue does that, same still-open gap
// ULevelClearTimeSubsystem.h documents for its own OnLevelBegin/OnLevelClear callers.
UCLASS()
class KROWDKONTROL_API UCrowdMasterySubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

	friend class FKrowdKontrolCrowdMasterySubsystemTest;

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	// Re-scans this world's AEnemyBase population via TActorIterator and raises
	// RunningMaxControlledCount if the current count of Controlled instances exceeds
	// it. Public so a real production caller (once wired) and the Automation
	// Framework test can both drive it directly - same "public so tests can drive it
	// deterministically" rationale ULevelLifecycleSubsystem::RefreshLevelClearState()
	// documents.
	UFUNCTION(BlueprintCallable, Category = "Crowd Mastery")
	void SampleControlledCount();

	UFUNCTION(BlueprintCallable, Category = "Crowd Mastery")
	int32 GetRunningMaxControlledCount() const { return RunningMaxControlledCount; }

	// Matches UAbilityCastComponent::FOnAbilityCastApplied's signature exactly so a
	// future wiring issue can bind this via AddDynamic with no signature changes.
	// Ability/TargetEnemy are unused - every call just re-samples the whole world's
	// current Controlled count, same as HandleEnemyControlledExpired below.
	UFUNCTION()
	void HandleAbilityCastApplied(EAbilitySlot Ability, AEnemyBase* TargetEnemy);

	// Matches AEnemyBase::FOnEnemyControlledExpired's signature exactly (no params).
	UFUNCTION()
	void HandleEnemyControlledExpired();

	// Matches ULevelLifecycleSubsystem::FOnLevelBegin's signature exactly. Bound for
	// real in Initialize() below.
	UFUNCTION()
	void HandleLevelBegin(FName MapName);

private:
	int32 RunningMaxControlledCount = 0;
};
