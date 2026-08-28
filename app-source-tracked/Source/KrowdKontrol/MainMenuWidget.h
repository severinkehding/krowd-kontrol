#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MainMenuWidget.generated.h"

class UBorder;
class UTextBlock;
class UButton;
class USizeBox;
class UVerticalBox;
class UHorizontalBox;
class UMainMenuLevelButtonWidget;
class UCrowdMasteryTotalSubsystem;

// Main menu chrome (issue #324, docs/prd-main-menu.md REQ-3): title, Quit button, and
// an anchored-but-empty region reserved for the mastery-display PRD's widget. Also
// builds the data-driven level-select list (issue #325, docs/prd-main-menu.md REQ-2)
// - one UMainMenuLevelButtonWidget per ULevelSequenceSubsystem::
// GetShippedLevelMapNames() entry, see PopulateLevelSelectButtons() below. Builds
// its tree in C++ - same no-Widget-Blueprint pattern as
// UPostRunSummaryWidget/UBriefingCardWidget. First UButton usage in this module (every
// prior HUD widget is display-only); UPunishmentDebugMenuWidget's UCheckBox
// OnCheckStateChanged.AddDynamic precedent is the closest existing analogue for
// binding a UMG input delegate in C++.
UCLASS()
class KROWDKONTROL_API UMainMenuWidget : public UUserWidget
{
	GENERATED_BODY()

	friend class FKrowdKontrolMainMenuWidgetTest;
	friend class FKrowdKontrolReservedGameplayColoursTest;
	friend class FKrowdKontrolMainMenuLevelSelectTest;
	friend class FKrowdKontrolMainMenuMasteryResetTest;

public:
	// Fills the reserved mastery-display anchor (docs/prd-crowd-mastery-persistence.md)
	// without any relayout - a later PRD's widget is the intended caller. No-op if
	// Content is null or the anchor hasn't been built yet.
	UFUNCTION(BlueprintCallable, Category = "Main Menu")
	void SetMasteryDisplayContent(UWidget* Content);

	UFUNCTION(BlueprintPure, Category = "Main Menu")
	FText GetTitleDisplayText() const;

protected:
	virtual void NativeOnInitialized() override;
	virtual bool Initialize() override;

private:
	void BuildWidgetTree();
	void EnsureWidgetTreeBuilt();

	// Builds one UMainMenuLevelButtonWidget per ULevelSequenceSubsystem::
	// GetShippedLevelMapNames() entry into LevelSelectBox - the data-driven level
	// list (issue #325). Called once from EnsureWidgetTreeBuilt(), alongside
	// BuildWidgetTree(), so a future L4/L5 DataTable row needs zero change here.
	void PopulateLevelSelectButtons();

	// Bound to QuitButton->OnClicked. Delegates to UKismetSystemLibrary::QuitGame() -
	// see MainMenuWidget.cpp for why that single call already does the right thing in
	// both PIE and packaged/-game contexts, with no manual GIsEditor branching needed.
	UFUNCTION()
	void HandleQuitClicked();

	// Bound to each UMainMenuLevelButtonWidget instance's OnLevelSelected. Loads
	// MapName directly via UGameplayStatics::OpenLevel() - no confirmation step, per
	// this issue's AC. Guarded by World->IsGameWorld() (see MainMenuWidget.cpp) so
	// Automation's CreateNewMap() Editor Worlds never hang on a real level load,
	// mirroring ULevelSequenceSubsystem::AdvanceToNextLevel()'s identical guard.
	UFUNCTION()
	void HandleLevelSelected(FName MapName);

	// Toggles which of MasteryResetButton vs. (MasteryResetConfirmButton,
	// MasteryResetCancelButton) is visible, based on bMasteryResetConfirmPending.
	// Called once at the end of BuildWidgetTree() to establish the initial
	// RESET-only state, and again after every click handler below.
	void RefreshMasteryResetVisibility();

	// Bound to MasteryResetButton->OnClicked. Arms the confirm step - does not
	// touch UCrowdMasteryTotalSubsystem.
	UFUNCTION()
	void HandleMasteryResetClicked();

	// Bound to MasteryResetCancelButton->OnClicked. Disarms the confirm step -
	// does not touch UCrowdMasteryTotalSubsystem. Leaving the total untouched on
	// cancel is this issue's own AC.
	UFUNCTION()
	void HandleMasteryResetCancelClicked();

	// Bound to MasteryResetConfirmButton->OnClicked. The only path that calls
	// ResetAccumulatedTotal() - disarms the confirm step either way.
	UFUNCTION()
	void HandleMasteryResetConfirmClicked();

	// Mirrors UPostRunSummaryWidget::ResolveLevelClearTimeSubsystem() exactly -
	// see MainMenuWidget.cpp for the identical lazy-cache/warn-once shape.
	UCrowdMasteryTotalSubsystem* ResolveMasteryTotalSubsystem();

	UPROPERTY()
	TObjectPtr<UBorder> RootBorder;

	UPROPERTY()
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY()
	TObjectPtr<UButton> QuitButton;

	UPROPERTY()
	TObjectPtr<UTextBlock> QuitButtonLabel;

	// Reserved, explicitly-sized, empty region for the future Crowd Mastery display
	// widget - see SetMasteryDisplayContent(). A USizeBox (not UNamedSlot - this project
	// has no Widget Blueprint assets, so UNamedSlot's inheritance-override machinery has
	// no consumer) so it occupies real layout space today even with no content.
	UPROPERTY()
	TObjectPtr<USizeBox> MasteryDisplayAnchor;

	// Reset control for the Crowd Mastery total (docs/prd-crowd-mastery-persistence.md
	// REQ-3, issue #329) - a RESET button that swaps to a CONFIRM RESET/CANCEL pair on
	// click, since resetting the total is destructive and this codebase has no
	// modal/popup widget machinery to build a real confirm dialog on (see
	// MainMenuWidget.cpp for the rejected alternatives). All three buttons are
	// siblings in this one row; only one state is visible at a time.
	UPROPERTY()
	TObjectPtr<UHorizontalBox> MasteryResetBox;

	UPROPERTY()
	TObjectPtr<UButton> MasteryResetButton;

	UPROPERTY()
	TObjectPtr<UTextBlock> MasteryResetButtonLabel;

	UPROPERTY()
	TObjectPtr<UButton> MasteryResetConfirmButton;

	UPROPERTY()
	TObjectPtr<UTextBlock> MasteryResetConfirmButtonLabel;

	UPROPERTY()
	TObjectPtr<UButton> MasteryResetCancelButton;

	UPROPERTY()
	TObjectPtr<UTextBlock> MasteryResetCancelButtonLabel;

	// Test-observability seam (mirrors LastSelectedLevelMapName's precedent) - true
	// while the CONFIRM/CANCEL row is showing instead of the RESET button.
	UPROPERTY()
	bool bMasteryResetConfirmPending = false;

	// Lazy cache + warn-once, mirroring UPostRunSummaryWidget::
	// CachedLevelClearTimeSubsystem/bHasWarnedMissingLevelClearTimeSubsystem exactly.
	UPROPERTY()
	TObjectPtr<UCrowdMasteryTotalSubsystem> CachedMasteryTotalSubsystem;

	bool bHasWarnedMissingMasteryTotalSubsystem = false;

	// Container for the data-driven level-select list (issue #325) - one
	// UMainMenuLevelButtonWidget child per shipped level, populated by
	// PopulateLevelSelectButtons(). Sits between the title and the mastery-display
	// anchor in Layout.
	UPROPERTY()
	TObjectPtr<UVerticalBox> LevelSelectBox;

	// Parallel to LevelSelectBox's children - kept as a member (not a
	// PopulateLevelSelectButtons() local) so tests can assert count/contents
	// directly, mirroring UAbilityCooldownTrayWidget::SlotIconBorders's precedent.
	UPROPERTY()
	TArray<TObjectPtr<UMainMenuLevelButtonWidget>> LevelSelectButtons;

	// Test-observability seam (mirrors ULevelSequenceSubsystem::
	// LastAdvanceAttemptedMapName) - records the last button activation's target map
	// even when the IsGameWorld() guard in HandleLevelSelected() prevents the real
	// OpenLevel() travel from running (Automation Editor Worlds).
	UPROPERTY()
	FName LastSelectedLevelMapName = NAME_None;

	static constexpr float MasteryDisplayAnchorWidthPx = 320.0f;
	static constexpr float MasteryDisplayAnchorHeightPx = 96.0f;
};
