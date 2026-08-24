#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PostRunSummaryWidget.generated.h"

class UBorder;
class UTextBlock;
class ULevelClearTimeSubsystem;
class UCrowdMasterySubsystem;
class ULevelLifecycleSubsystem;

// Post-run summary screen (PRD 13 REQ-5): displays clear time, persisted best clear
// time, and the "Crowd Mastery" stat after a level/run clears. This widget builds its
// own UI tree in C++ (no Widget Blueprint asset - mirrors
// UAbilityCooldownTrayWidget/UEnergyMeterWidget's existing precedent) and seeds itself
// with placeholder values on construction so the screen is self-demonstrating today.
// Issue #175 (PRD 06 REQ-6) wires it to real gameplay: the widget self-subscribes to
// ULevelLifecycleSubsystem::OnLevelClear (mirroring UQuestTrackerWidget's own
// self-subscribe-to-OnLevelBegin shape) and, on a real level clear, calls
// SetSummaryValues() with real clear-time/best-time/Crowd-Mastery data and adds itself
// to the viewport - see HandleLevelClear()'s own comment for the full sequence.
UCLASS()
class KROWDKONTROL_API UPostRunSummaryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Formats ClearTimeSeconds/BestClearTimeSeconds as M:SS (matches PRD 06 REQ-2's own
	// display example, "Your best: 4:32") and CrowdMasteryCount as a plain integer
	// count, and updates all three text fields. This is the call
	// UPostRunSummaryWidget::HandleLevelClear() makes with live data on a real level
	// clear; NativeOnInitialized() calls it with placeholder values so the screen is
	// self-demonstrating before that ever happens.
	UFUNCTION(BlueprintCallable, Category = "Post-Run Summary")
	void SetSummaryValues(float ClearTimeSeconds, float BestClearTimeSeconds, int32 CrowdMasteryCount);

	// Read-only accessors for what's currently displayed - used by the Automation
	// Framework test, also generally useful to anything that wants to confirm the
	// screen's state without re-deriving formatting.
	UFUNCTION(BlueprintPure, Category = "Post-Run Summary")
	FText GetClearTimeDisplayText() const;

	UFUNCTION(BlueprintPure, Category = "Post-Run Summary")
	FText GetBestClearTimeDisplayText() const;

	UFUNCTION(BlueprintPure, Category = "Post-Run Summary")
	FText GetCrowdMasteryDisplayText() const;

protected:
	// Fires synchronously from CreateWidget(), before any Slate/viewport realization
	// - unlike NativeConstruct(), this doesn't depend on TakeWidget()/AddToViewport(),
	// which matters for the -nullrhi headless Automation run this project's tests use
	// (harness/run_ue_automation.sh).
	virtual void NativeOnInitialized() override;

	// Safety net: UUserWidget::Initialize() only calls NativeOnInitialized() when the
	// widget has a valid PlayerContext (a local player) or belongs to a
	// UWidgetBlueprintGeneratedClass flagged to skip that check - neither holds for
	// CreateWidget(World, Class) with no owning player/controller, which is exactly how
	// the Automation Framework test (and any other player-less caller) constructs this
	// widget. Building here too, unconditionally on first Initialize(), guarantees
	// WidgetTree->RootWidget exists before the widget's first TakeWidget() call - this
	// can't be deferred to NativeConstruct() instead, because TakeWidget() calls
	// RebuildWidget() (which reads WidgetTree->RootWidget) and caches its result BEFORE
	// NativeConstruct() ever fires, so a still-null RootWidget at that point would
	// permanently cache an empty SSpacer as this widget's Slate representation.
	virtual bool Initialize() override;

private:
	friend class FKrowdKontrolPostRunSummaryWidgetTest;
	friend class FKrowdKontrolReservedGameplayColoursTest;
	friend class FKrowdKontrolPostRunSummaryWidgetWiringTest;

	void BuildWidgetTree();

	// Builds the widget tree exactly once, regardless of which of NativeOnInitialized()
	// or Initialize() fires first - correctness no longer depends on either hook's call
	// order relative to the other, since both call this unconditionally and it's the
	// only place that checks ClearTimeText.
	void EnsureWidgetTreeBuilt();

	// Shared by both fields in SetSummaryValues(): sets Text on TextBlock if it exists,
	// otherwise logs which field is rendering blank and why.
	void SetTextBlockSafe(UTextBlock* TextBlock, const FText& Text, const TCHAR* FieldName) const;

	// Resolves this world's ULevelLifecycleSubsystem and subscribes to its
	// OnLevelClear - mirrors UQuestTrackerWidget::BindToLevelLifecycle()'s identical
	// self-subscribe idiom, adapted for OnLevelClear. Deliberately no
	// HasLevelBegun()-style late-subscribe catch-up: OnLevelClear only ever broadcasts
	// from ULevelLifecycleSubsystem::RefreshLevelClearState(), called from Tick(), and
	// this widget's own construction (from CreateHUDWidgets(), an actor's BeginPlay())
	// is guaranteed to complete before the first Tick() of any frame - so it is
	// impossible for OnLevelClear to fire before this widget has bound to it.
	void BindToLevelLifecycle();

	// Bound to ULevelLifecycleSubsystem::OnLevelClear via BindToLevelLifecycle().
	// Reads this run's real clear time (ULevelClearTimeSubsystem::GetLastClearTimeSeconds()),
	// the persisted personal best (GetBestClearTimeSeconds()), and this run's Crowd
	// Mastery peak (UCrowdMasterySubsystem::GetRunningMaxControlledCount()), calls
	// SetSummaryValues() with the real values, then AddToViewport()s itself. Relies on
	// ULevelClearTimeSubsystem::HandleLevelClear() having already run for this same
	// broadcast (it subscribes earlier, from world-Initialize() time, vs. this widget's
	// actor-BeginPlay()-time bind) so LastClearTimeSeconds/the persisted best are both
	// already up to date by the time this handler reads them.
	UFUNCTION()
	void HandleLevelClear();

	// Resolves (and caches) the current UGameInstance's ULevelClearTimeSubsystem,
	// mirroring AKrowdKontrolPlayerController::ResolveLevelClearTimeSubsystem()'s exact
	// pattern - GetGameInstance() is null in this project's CreateNewMap()-based
	// Automation test worlds, so the Automation Framework test injects a
	// directly-constructed instance into CachedLevelClearTimeSubsystem via the
	// friendship above instead of going through this resolver.
	ULevelClearTimeSubsystem* ResolveLevelClearTimeSubsystem();

	UPROPERTY()
	TObjectPtr<ULevelClearTimeSubsystem> CachedLevelClearTimeSubsystem;

	bool bHasWarnedMissingLevelClearTimeSubsystem = false;

	// Kept as a member (rather than a BuildWidgetTree() local) so
	// KrowdKontrolReservedGameplayColoursTest.cpp can audit its background colour via
	// friend-class access, matching UAbilityCooldownTrayWidget::SlotIconBorders's
	// precedent for the same reason.
	UPROPERTY()
	TObjectPtr<UBorder> RootBorder;

	UPROPERTY()
	TObjectPtr<UTextBlock> ClearTimeText;

	UPROPERTY()
	TObjectPtr<UTextBlock> BestClearTimeText;

	UPROPERTY()
	TObjectPtr<UTextBlock> CrowdMasteryText;

	// Placeholder values only (issue #74) - shown until a real OnLevelClear broadcast
	// calls SetSummaryValues() with real data (issue #175).
	static constexpr float PlaceholderClearTimeSeconds = 272.0f;
	static constexpr float PlaceholderBestClearTimeSeconds = 272.0f;
	static constexpr int32 PlaceholderCrowdMasteryCount = 14;
};
