#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PostRunSummaryWidget.generated.h"

class UTextBlock;

// Post-run summary screen (PRD 13 REQ-5): displays clear time and the "Crowd
// Mastery" stat after a level/run clears. Real clear-time and Crowd Mastery
// tracking don't exist yet (PRD 06 REQ-2/REQ-3, tracked separately) - this widget
// builds its own UI tree in C++ (no Widget Blueprint asset - see plan.md Approach
// Decisions for issue #74) and seeds itself with placeholder values on
// construction, purely to prove out the screen's layout and both fields rendering.
// SetSummaryValues() is the wiring point a future real tracking system replaces
// the placeholder call with.
UCLASS()
class KROWDKONTROL_API UPostRunSummaryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Formats ClearTimeSeconds as M:SS (matches PRD 06 REQ-2's own display example,
	// "Your best: 4:32") and CrowdMasteryCount as a plain integer count, and updates
	// both text fields. This is the call a future real clear-time/Crowd Mastery
	// tracking system (PRD 06 REQ-2/REQ-3) is expected to make with live data;
	// NativeOnInitialized() calls it with placeholder values so the screen is
	// self-demonstrating today.
	UFUNCTION(BlueprintCallable, Category = "Post-Run Summary")
	void SetSummaryValues(float ClearTimeSeconds, int32 CrowdMasteryCount);

	// Read-only accessors for what's currently displayed - used by the Automation
	// Framework test, also generally useful to anything that wants to confirm the
	// screen's state without re-deriving formatting.
	UFUNCTION(BlueprintPure, Category = "Post-Run Summary")
	FText GetClearTimeDisplayText() const;

	UFUNCTION(BlueprintPure, Category = "Post-Run Summary")
	FText GetCrowdMasteryDisplayText() const;

protected:
	// Fires synchronously from CreateWidget(), before any Slate/viewport realization
	// - unlike NativeConstruct(), this doesn't depend on TakeWidget()/AddToViewport(),
	// which matters for the -nullrhi headless Automation run this project's tests use
	// (harness/run_ue_automation.sh). See plan.md Approach Decisions for issue #74.
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
	void BuildWidgetTree();

	UPROPERTY()
	TObjectPtr<UTextBlock> ClearTimeText;

	UPROPERTY()
	TObjectPtr<UTextBlock> CrowdMasteryText;

	// Placeholder values only (issue #74) - real clear-time/Crowd Mastery tracking
	// is out of scope here, tracked by PRD 06 REQ-2/REQ-3.
	static constexpr float PlaceholderClearTimeSeconds = 272.0f;
	static constexpr int32 PlaceholderCrowdMasteryCount = 14;
};
