#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "OnScreenPromptWidget.generated.h"

class UBorder;
class UTextBlock;

// Non-blocking on-screen prompt widget (PRD 09 REQ-4): displays a short text cue
// during live gameplay via ShowPrompt(), auto-dismissing at a hard-capped ~2 seconds,
// without ever pausing the game or intercepting player input. This issue covers only
// the prompt widget itself, not what triggers it - matches UGizmoNarrativeSubsystem's
// "foundation only, no wiring" precedent. Mirrors UAbilityCooldownTrayWidget's
// NativeOnInitialized()/Initialize()/NativeTick() lifecycle and per-frame countdown
// pattern, and UPostRunSummaryWidget's simpler single-border-root layout (one message,
// not five slots). Builds its own UI tree in C++ - no Widget Blueprint asset, same as
// every other HUD widget in this module.
UCLASS()
class KROWDKONTROL_API UOnScreenPromptWidget : public UUserWidget
{
	GENERATED_BODY()

	friend class FKrowdKontrolOnScreenPromptWidgetTest;
	friend class FKrowdKontrolReservedGameplayColoursTest;

public:
	// Hard cap on how long a prompt can stay visible - PRD 09 REQ-4 forbids blocking
	// input for more than ~2 seconds. ShowPrompt() clamps DurationSeconds to this,
	// unconditionally, so no caller - today or future - can exceed it.
	static constexpr float MaxPromptDurationSeconds = 2.0f;

	// Displays Message and (re)starts the auto-dismiss countdown. Calling this again
	// while a prompt is already showing replaces its text and resets the timer rather
	// than stacking/queuing - there is exactly one message slot.
	// NOTE: the default below is a literal, not MaxPromptDurationSeconds - UnrealHeaderTool
	// cannot parse a qualified/unqualified static member reference as a UFUNCTION default
	// parameter ("C++ Default parameter not parsed"), only a literal. Keep this in sync
	// with MaxPromptDurationSeconds by hand; ShowPrompt()'s own FMath::Clamp is what
	// actually enforces the cap regardless of what any caller passes.
	UFUNCTION(BlueprintCallable, Category = "On-Screen Prompt")
	void ShowPrompt(const FText& Message, float DurationSeconds = 2.0f);

	// Decrements the remaining display time and dismisses the prompt once it hits
	// zero. Called every frame from NativeTick() once this widget is in a live
	// viewport, and called directly by the Automation test (which can't drive
	// NativeTick under the -nullrhi headless run).
	UFUNCTION(BlueprintCallable, Category = "On-Screen Prompt")
	void AdvanceDismissTimer(float DeltaSeconds);

	UFUNCTION(BlueprintPure, Category = "On-Screen Prompt")
	bool IsPromptVisible() const;

	UFUNCTION(BlueprintPure, Category = "On-Screen Prompt")
	float GetRemainingSeconds() const;

	// Read-only accessor for what's currently displayed - used by the Automation
	// Framework test, also generally useful to anything that wants to confirm the
	// prompt's current text.
	UFUNCTION(BlueprintPure, Category = "On-Screen Prompt")
	FText GetPromptDisplayText() const;

protected:
	// Fires synchronously from CreateWidget(), before any Slate/viewport realization -
	// matters for the -nullrhi headless Automation run this project's tests use (see
	// UPostRunSummaryWidget's NativeOnInitialized() precedent, issue #74).
	virtual void NativeOnInitialized() override;

	// Safety net mirroring UPostRunSummaryWidget::Initialize() - guarantees
	// WidgetTree->RootWidget exists before this widget's first TakeWidget() call even
	// when CreateWidget() is called without an owning player/controller (exactly how
	// the Automation Framework test constructs this widget).
	virtual bool Initialize() override;

	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void BuildWidgetTree();

	// Builds the widget tree exactly once, regardless of which of
	// NativeOnInitialized()/Initialize() fires first - mirrors
	// UAbilityCooldownTrayWidget::EnsureWidgetTreeBuilt().
	void EnsureWidgetTreeBuilt();

	// Shared by ShowPrompt()'s zero-duration path and AdvanceDismissTimer()'s
	// countdown-expiry path - both dismiss the same way.
	void ClearPromptDisplay();

	UPROPERTY()
	TObjectPtr<UBorder> PromptBorder;

	UPROPERTY()
	TObjectPtr<UTextBlock> PromptText;

	// Runtime state, not designer config - hence no EditDefaultsOnly/EditAnywhere,
	// private so no code path - Blueprint or C++ - can mutate it except through
	// ShowPrompt()/AdvanceDismissTimer().
	UPROPERTY()
	float RemainingSeconds = 0.0f;
};
