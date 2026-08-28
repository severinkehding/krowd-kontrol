#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LevelBriefingData.h"
#include "BriefingCardWidget.generated.h"

class UBorder;
class UTextBlock;

// Pre-level briefing card (issue #246, PRD "Mission Briefing & Live Quest Tracker"
// REQ-1): shown the moment a level begins, displaying that level's display name,
// objective one-liners, and an optional new-ability-unlock line, all authored per
// level in a UDataTable row (FLevelBriefingRow) - never hardcoded C++ strings per
// map. Builds its own UI tree in C++ - no Widget Blueprint asset, same as every
// other HUD widget in this module. Mirrors UPostRunSummaryWidget's chrome-tree
// build shape and UOnScreenPromptWidget's self-driven countdown/dismiss idiom, but
// with its own 8s cap and, unlike OnScreenPromptWidget's "never pause, never
// intercept input" contract, this widget's safe state comes from pausing the world
// (ShowBriefing()/DismissBriefing() call UGameplayStatics::SetGamePaused()) - for
// real players only; both calls are GIsAutomationTesting-guarded no-ops under the
// Automation Framework, since other KrowdKontrol.PIE.* tests drive real per-frame AI
// ticking that a paused World would freeze (see BriefingCardWidget.cpp).
UCLASS()
class KROWDKONTROL_API UBriefingCardWidget : public UUserWidget
{
	GENERATED_BODY()

	friend class FKrowdKontrolBriefingCardWidgetTest;
	friend class FKrowdKontrolReservedGameplayColoursTest;

public:
	// The concrete "pick and document" auto-dismiss duration this issue asks for.
	static constexpr float BriefingAutoDismissSeconds = 8.0f;

	// Populates the card from Row and shows it: sets LevelNameText; joins
	// Row.ObjectiveLines with newlines into ObjectiveText; shows/collapses
	// NewAbilityText depending on whether Row.NewAbilityUnlockLine is empty; starts
	// the 8s auto-dismiss countdown; pauses the world
	// (UGameplayStatics::SetGamePaused) so no gameplay can progress while the card
	// is up. Re-triggering while already showing replaces rather than stacks,
	// matching UOnScreenPromptWidget::ShowPrompt()'s precedent.
	UFUNCTION(BlueprintCallable, Category = "Level Briefing")
	void ShowBriefing(const FLevelBriefingRow& Row);

	// Clears the card and un-pauses the world. Safe to call whether or not a
	// briefing is currently showing (idempotent, mirrors
	// UOnScreenPromptWidget::ClearPromptDisplay()'s null-safety).
	UFUNCTION(BlueprintCallable, Category = "Level Briefing")
	void DismissBriefing();

	// Decrements the remaining display time and dismisses the card once it hits
	// zero. Called every frame from NativeTick() once this widget is in a live
	// viewport. No automatic per-frame tick loop runs under the -nullrhi headless
	// Automation run, so tests call this directly for precise timing assertions
	// (KrowdKontrolBriefingCardWidgetTest.cpp case (e)) and can still exercise the
	// real NativeTick() path with an explicit call (case (f)).
	UFUNCTION(BlueprintCallable, Category = "Level Briefing")
	void AdvanceDismissTimer(float DeltaSeconds);

	UFUNCTION(BlueprintPure, Category = "Level Briefing")
	bool IsBriefingVisible() const { return RemainingSeconds > 0.0f; }

	// Read-only accessors for what's currently displayed - used by the Automation
	// Framework test, also generally useful to anything that wants to confirm the
	// card's current content.
	UFUNCTION(BlueprintPure, Category = "Level Briefing")
	FText GetLevelNameDisplayText() const;

	UFUNCTION(BlueprintPure, Category = "Level Briefing")
	FText GetObjectiveDisplayText() const;

	UFUNCTION(BlueprintPure, Category = "Level Briefing")
	FText GetNewAbilityDisplayText() const;

protected:
	// Fires synchronously from CreateWidget(), before any Slate/viewport realization
	// - matters for the -nullrhi headless Automation run this project's tests use
	// (see UPostRunSummaryWidget's NativeOnInitialized() precedent, issue #74).
	virtual void NativeOnInitialized() override;

	// Safety net mirroring UPostRunSummaryWidget::Initialize() - guarantees
	// WidgetTree->RootWidget exists before this widget's first TakeWidget() call
	// even when CreateWidget() is called without an owning player/controller.
	virtual bool Initialize() override;

	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void BuildWidgetTree();

	// Builds the widget tree exactly once, regardless of which of
	// NativeOnInitialized()/Initialize() fires first - mirrors
	// UOnScreenPromptWidget::EnsureWidgetTreeBuilt().
	void EnsureWidgetTreeBuilt();

	// Kept as members (rather than BuildWidgetTree() locals) so
	// KrowdKontrolReservedGameplayColoursTest.cpp can audit their colours via
	// friend-class access, matching UPostRunSummaryWidget's identical precedent.
	UPROPERTY()
	TObjectPtr<UBorder> RootBorder;

	UPROPERTY()
	TObjectPtr<UTextBlock> LevelNameText;

	UPROPERTY()
	TObjectPtr<UTextBlock> ObjectiveText;

	UPROPERTY()
	TObjectPtr<UTextBlock> NewAbilityText;

	// Runtime state, not designer config - hence no EditDefaultsOnly/EditAnywhere,
	// private so no code path - Blueprint or C++ - can mutate it except through
	// ShowBriefing()/DismissBriefing()/AdvanceDismissTimer().
	UPROPERTY()
	float RemainingSeconds = 0.0f;
};
