#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MainMenuWidget.generated.h"

class UBorder;
class UTextBlock;
class UButton;
class USizeBox;
class UVerticalBox;
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

public:
	// Fills the reserved mastery-display anchor (docs/prd-crowd-mastery-persistence.md)
	// without any relayout - a later PRD's widget is the intended caller. No-op if
	// Content is null or the anchor hasn't been built yet.
	UFUNCTION(BlueprintCallable, Category = "Main Menu")
	void SetMasteryDisplayContent(UWidget* Content);

	UFUNCTION(BlueprintPure, Category = "Main Menu")
	FText GetTitleDisplayText() const;

	UFUNCTION(BlueprintPure, Category = "Main Menu")
	FText GetMasteryDisplayText() const;

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

	// Reads the current total from UCrowdMasteryTotalSubsystem and formats it into
	// MasteryDisplayText. Called once from BuildWidgetTree() - see MainMenuWidget.cpp
	// for why a fresh read at construction already satisfies "refreshes whenever the
	// menu is shown" in this codebase (the widget is rebuilt from scratch on every
	// L_MainMenu visit).
	void RefreshMasteryDisplayText();

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

	// The Crowd Mastery total display (issue #328, docs/prd-crowd-mastery-persistence.md
	// REQ-2) - built and slotted into MasteryDisplayAnchor via SetMasteryDisplayContent()
	// inside BuildWidgetTree() itself, then populated by RefreshMasteryDisplayText().
	UPROPERTY()
	TObjectPtr<UTextBlock> MasteryDisplayText;

	// Lazy cache of the GameInstance-scoped mastery-total subsystem, read by
	// RefreshMasteryDisplayText(). A missing GameInstance during construction is an
	// unremarkable, expected case (every KrowdKontrol.Unit.* test hits it) and stays
	// unlogged - but a present GameInstance with no resolvable subsystem is a real
	// failure and is warned once via bHasWarnedMissingMasteryTotalSubsystemOnDisplay
	// below, mirroring UPostRunSummaryWidget::CachedLevelClearTimeSubsystem's sibling
	// warn-once pattern.
	UPROPERTY()
	TObjectPtr<UCrowdMasteryTotalSubsystem> CachedMasteryTotalSubsystem;

	// Warn-once flag for RefreshMasteryDisplayText()'s cold-path subsystem-resolution
	// failure (GameInstance present, UCrowdMasteryTotalSubsystem not resolvable) -
	// distinguishes a real bug from the unremarkable "no GameInstance yet" test case,
	// which stays silent. Kept local rather than reusing the mastery-reset flow's
	// resolver/flag to avoid coupling this PR to that flow's own test file.
	UPROPERTY()
	bool bHasWarnedMissingMasteryTotalSubsystemOnDisplay = false;

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
