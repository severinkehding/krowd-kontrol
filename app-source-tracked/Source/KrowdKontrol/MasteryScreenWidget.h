#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MasterySkillBubbleWidget.h"
#include "MasteryScreenWidget.generated.h"

class UBorder;
class UTextBlock;
class UButton;
class UCanvasPanel;
class UCrowdMasteryTotalSubsystem;

// Issue #373 (scaffolding) + issue #374 (docs/prd-mastery-skill-tree.md REQ-2's
// tree render/click-to-unlock half): a dedicated screen for the Crowd Mastery
// skill tree, reachable from a new MASTERY button on UMainMenuWidget. Shows the
// player's unspent points (GetAccumulatedTotal() - GetSpentPoints()), renders
// every MasteryTreeTable node/bubble via PopulateTreeContent(), and a BACK
// control that broadcasts OnBackRequested rather than touching any subsystem
// state itself - UMainMenuWidget owns the actual visibility swap. Builds its
// tree in C++, same no-Widget-Blueprint lineage as
// UMainMenuWidget/UPostRunSummaryWidget.
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnMasteryScreenBackRequested);

UCLASS()
class KROWDKONTROL_API UMasteryScreenWidget : public UUserWidget
{
	GENERATED_BODY()

	friend class FKrowdKontrolMasteryScreenWidgetTest;
	friend class FKrowdKontrolMainMenuMasteryScreenTest;
	friend class FKrowdKontrolReservedGameplayColoursTest;
	friend class FKrowdKontrolMasteryTreeContentTest;
	friend class FKrowdKontrolMainMenuMasteryResetTest;

public:
	// Fires when BACK is clicked. Touches no subsystem state - UMainMenuWidget is the
	// sole owner of what "going back" means (restoring its own RootBorder visibility).
	UPROPERTY(BlueprintAssignable, Category = "Mastery")
	FOnMasteryScreenBackRequested OnBackRequested;

	// Re-reads UCrowdMasteryTotalSubsystem::GetAccumulatedTotal() and reformats
	// PointsText. Called from BuildWidgetTree() at construction time and again from
	// NativeConstruct() below every time this widget is (re-)added to the viewport -
	// mirrors UMainMenuWidget::RefreshMasteryDisplayText()'s identical "refresh on
	// every show" contract (PR #350 review).
	UFUNCTION(BlueprintCallable, Category = "Mastery")
	void RefreshPointsDisplayText();

	UFUNCTION(BlueprintPure, Category = "Mastery")
	FText GetPointsDisplayText() const;

	// Re-derives both the points display and every bubble's visual state in one call
	// - used by UMainMenuWidget after a full respec (issue #380,
	// docs/prd-mastery-skill-tree.md REQ-5) so an already-open tree screen reflects
	// the cleared unlocks/points immediately, without requiring BACK + re-open.
	UFUNCTION(BlueprintCallable, Category = "Mastery")
	void RefreshAfterRespec();

protected:
	virtual void NativeOnInitialized() override;
	virtual bool Initialize() override;

	// Fires every time this widget is (re-)added to the viewport (AddToViewport()) -
	// see RefreshPointsDisplayText()'s comment for why this is relied on instead of
	// assuming a fresh instance is created on every open.
	virtual void NativeConstruct() override;

private:
	void BuildWidgetTree();
	void EnsureWidgetTreeBuilt();

	// Bound to BackButton->OnClicked. Only ever broadcasts OnBackRequested - never
	// resets, deposits, or otherwise mutates UCrowdMasteryTotalSubsystem, per this
	// issue's "no side effects" AC.
	UFUNCTION()
	void HandleBackClicked();

	// Issue #374, docs/prd-mastery-skill-tree.md REQ-2: builds every node/bubble
	// widget from MasteryTreeTable and calls RefreshBubbleStates() once at the end.
	void PopulateTreeContent();

	// Re-derives every bubble's visual state from the subsystem (unlocked / prereq
	// / affordability) - called after PopulateTreeContent() and again after every
	// successful spend, never caches state client-side.
	void RefreshBubbleStates();

	// Bound to every UMasterySkillBubbleWidget::OnBubbleClicked instance.
	UFUNCTION()
	void HandleBubbleClicked(FName BubbleId);

	// Extracted from RefreshPointsDisplayText()'s original body (issue #373) so
	// PopulateTreeContent()/RefreshBubbleStates()/HandleBubbleClicked() share the
	// same resolve-and-cache-once/warn-once logic instead of duplicating it.
	UCrowdMasteryTotalSubsystem* ResolveMasteryTotalSubsystem();

	UPROPERTY()
	TObjectPtr<UBorder> RootBorder;

	UPROPERTY()
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY()
	TObjectPtr<UTextBlock> PointsText;

	UPROPERTY()
	TObjectPtr<UButton> BackButton;

	UPROPERTY()
	TObjectPtr<UTextBlock> BackButtonLabel;

	// Issue #374: the tree's canvas panel, populated by PopulateTreeContent(). So
	// tests can assert count/contents, mirroring MainMenuWidget.h's
	// LevelSelectButtons array declaration comment style.
	UPROPERTY()
	TObjectPtr<UCanvasPanel> TreeCanvas;

	UPROPERTY()
	TArray<TObjectPtr<UMasterySkillBubbleWidget>> BubbleWidgets;

	UPROPERTY()
	TMap<FName, TObjectPtr<UMasterySkillBubbleWidget>> BubbleWidgetsByBubbleId;

	// Lazy cache of the GameInstance-scoped mastery-total subsystem - single read
	// path (no reset/deposit flow on this widget, unlike UMainMenuWidget's split
	// cache/warn-once pair), so one cache and one warn-once flag below is correct.
	UPROPERTY()
	TObjectPtr<UCrowdMasteryTotalSubsystem> CachedMasteryTotalSubsystem;

	bool bHasWarnedMissingMasteryTotalSubsystem = false;
};
