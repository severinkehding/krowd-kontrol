#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "QuestTrackerWidget.generated.h"

class UBorder;
class UTextBlock;
class AActor;

// Persistent quest tracker HUD widget (PRD "Mission Briefing & Live Quest Tracker"
// REQ-2, issue #247): a small, top-right-corner-anchored panel showing
// "Robots penned: X/Y", the level's live banked-enemy progress. Top-right is the one
// HUD corner not already claimed by UEnergyMeterWidget (top-left, PRD 13 REQ-1),
// UAbilityCooldownTrayWidget (bottom-right, issue #66), or UOnScreenPromptWidget
// (top-center, issue #34). X updates event-driven only, from every live
// ATargetZone::OnActorBanked broadcast in the level (never per-frame polling); Y is
// captured once, from a TActorIterator<AEnemyBase> sweep, when
// ULevelLifecycleSubsystem::OnLevelBegin fires - not from NativeOnInitialized()/
// CreateHUDWidgets() timing, because ATargetZone instances aren't guaranteed to exist
// yet at that point (ARoomActor::BeginPlay()'s EnsureBankingZonesWired() is what
// spawns/wires them, and actor BeginPlay() order across independent actors isn't the
// engine's contract to guarantee - OnLevelBegin fires only after every actor's
// BeginPlay() has already completed, removing the race). This widget builds its own
// UI tree in C++ (no Widget Blueprint asset), mirroring UEnergyMeterWidget/
// UOnScreenPromptWidget. This issue covers only the banked-count line - two further
// lines (current room state, suggested ability) are separate follow-up issues from
// the same PRD that attach to this same widget class.
UCLASS()
class KROWDKONTROL_API UQuestTrackerWidget : public UUserWidget
{
	GENERATED_BODY()

	friend class FKrowdKontrolQuestTrackerWidgetTest;

public:
	// Read-only accessors for what's currently displayed/tracked - used by the
	// Automation Framework test, also generally useful to anything that wants to
	// confirm this widget's state without re-deriving formatting.
	UFUNCTION(BlueprintPure, Category = "Quest Tracker")
	FText GetQuestTrackerDisplayText() const;

	UFUNCTION(BlueprintPure, Category = "Quest Tracker")
	int32 GetBankedCount() const { return BankedCount; }

	UFUNCTION(BlueprintPure, Category = "Quest Tracker")
	int32 GetTotalEnemyCount() const { return TotalEnemyCount; }

protected:
	// Fires synchronously from CreateWidget(), before any Slate/viewport realization -
	// matters for the -nullrhi headless Automation run this project's tests use (see
	// UPostRunSummaryWidget's NativeOnInitialized() precedent, issue #74).
	virtual void NativeOnInitialized() override;

	// Safety net mirroring UEnergyMeterWidget::Initialize() - guarantees
	// WidgetTree->RootWidget exists before this widget's first TakeWidget() call even
	// when CreateWidget() is called without an owning player/controller (exactly how
	// the Automation Framework test constructs this widget).
	virtual bool Initialize() override;

	// Deliberately no NativeTick() override - this widget has no cosmetic per-frame
	// state (unlike UEnergyMeterWidget's damage-flash timer). 100% event-driven,
	// matching issue #247's explicit "never per-frame polling" requirement.

private:
	void BuildWidgetTree();

	// Builds the widget tree exactly once, regardless of which of
	// NativeOnInitialized()/Initialize() fires first - mirrors
	// UEnergyMeterWidget::EnsureWidgetTreeBuilt().
	void EnsureWidgetTreeBuilt();

	// Resolves this world's ULevelLifecycleSubsystem and subscribes to its
	// OnLevelBegin - mirrors UCrowdMasterySubsystem::Initialize()'s identical
	// self-subscribe idiom, adapted from a subsystem's Initialize() to a widget's
	// NativeOnInitialized(). AddUniqueDynamic makes this safe to call more than once.
	void BindToLevelLifecycle();

	// Bound to ULevelLifecycleSubsystem::OnLevelBegin. Captures TotalEnemyCount via a
	// one-shot TActorIterator<AEnemyBase> sweep and binds HandleActorBanked to every
	// live ATargetZone's OnActorBanked. Safe to have fired at most once - OnLevelBegin
	// itself only ever broadcasts once per world (LevelLifecycleSubsystem.h:34).
	UFUNCTION()
	void HandleLevelBegin(FName MapName);

	// Bound to each discovered ATargetZone's OnActorBanked via AddUniqueDynamic -
	// must be a real UFUNCTION() for that to compile, same as any BlueprintAssignable
	// delegate binding. Matches FOnActorBanked's signature exactly (TargetZone.h:11).
	UFUNCTION()
	void HandleActorBanked(AActor* BankedActor);

	// Re-renders BankedCountText from the current BankedCount/TotalEnemyCount.
	void RefreshDisplay();

	UPROPERTY()
	TObjectPtr<UBorder> ChromeBorder;

	UPROPERTY()
	TObjectPtr<UTextBlock> BankedCountText;

	UPROPERTY()
	int32 BankedCount = 0;

	UPROPERTY()
	int32 TotalEnemyCount = 0;

	// Actors already counted into BankedCount - ATargetZone::OnActorBanked fires once
	// per overlapping *component*, not once per actor (see TargetZone.cpp's own
	// "KNOWN GAP" comment), so this widget dedups at the point it turns the raw
	// broadcast into a number the player sees, rather than assuming the source is
	// already actor-unique.
	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> BankedActors;

	// Fixed pixel footprint from the viewport's top-right corner. 160px + 24px margin
	// = 184px footprint, which is ~14.4% of a 1280px-wide viewport (this project's
	// documented minimum target resolution - see the resolution-safety test) - inside
	// issue #247's explicit "no more than roughly 15% of screen width" envelope, with
	// the 1280px case being the binding (smallest, tightest) one; every wider
	// resolution is strictly safer for the same fixed-size box. Recompute this comment
	// if either constant below changes.
	static constexpr float TrackerMarginPx = 24.0f;
	static constexpr float TrackerWidthPx = 160.0f;
	static constexpr float TrackerHeightPx = 32.0f;
};
