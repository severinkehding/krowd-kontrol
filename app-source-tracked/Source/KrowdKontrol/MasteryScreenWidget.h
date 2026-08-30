#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MasteryScreenWidget.generated.h"

class UBorder;
class UTextBlock;
class UButton;
class UCrowdMasteryTotalSubsystem;

// Issue #373, docs/prd-mastery-skill-tree.md REQ-2's scaffolding half: a dedicated
// screen for the Crowd Mastery skill tree to grow into, reachable from a new MASTERY
// button on UMainMenuWidget. Shows the player's current unspent points (issue #380:
// UCrowdMasteryTotalSubsystem::GetAccumulatedTotal() - GetSpentPoints(), see
// RefreshPointsDisplayText()) and a BACK control that broadcasts OnBackRequested
// rather than touching any subsystem state itself - UMainMenuWidget owns the actual
// visibility swap. Builds its tree in C++, same no-Widget-Blueprint lineage as
// UMainMenuWidget/UPostRunSummaryWidget. Deliberately has no node/bubble tree content
// yet - that is a separate follow-up issue, see this issue's own body and
// docs/prd-mastery-skill-tree.md.
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnMasteryScreenBackRequested);

UCLASS()
class KROWDKONTROL_API UMasteryScreenWidget : public UUserWidget
{
	GENERATED_BODY()

	friend class FKrowdKontrolMasteryScreenWidgetTest;
	friend class FKrowdKontrolMainMenuMasteryScreenTest;
	friend class FKrowdKontrolReservedGameplayColoursTest;
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

	// Re-derives the points display - used by UMainMenuWidget after a full respec
	// (issue #380, docs/prd-mastery-skill-tree.md REQ-5) so an already-open tree
	// screen reflects the cleared points immediately, without requiring BACK +
	// re-open. Scoped to points only - no bubble/tree content exists in this
	// codebase yet (issue #374, in progress).
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

	// Lazy cache of the GameInstance-scoped mastery-total subsystem - single read
	// path (no reset/deposit flow on this widget, unlike UMainMenuWidget's split
	// cache/warn-once pair), so one cache and one warn-once flag below is correct.
	UPROPERTY()
	TObjectPtr<UCrowdMasteryTotalSubsystem> CachedMasteryTotalSubsystem;

	bool bHasWarnedMissingMasteryTotalSubsystem = false;
};
