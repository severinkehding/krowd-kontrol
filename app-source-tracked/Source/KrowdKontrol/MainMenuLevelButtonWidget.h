#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MainMenuLevelButtonWidget.generated.h"

class UButton;
class UTextBlock;

// Issue #325, docs/prd-main-menu.md REQ-2. One instance per shipped level, built
// and owned by UMainMenuWidget::PopulateLevelSelectButtons(). A small composite
// widget rather than a raw UButton because UButton::OnClicked (FOnButtonClickedEvent)
// carries no parameters - a runtime-sized list of buttons has no way to tell a
// single shared click handler which one fired. This class closes that gap by
// wrapping its own UButton internally and re-broadcasting the click as
// OnLevelSelected(MapName) - a dynamic multicast delegate WITH a payload, mirroring
// ULevelLifecycleSubsystem::FOnLevelBegin's existing FName-payload shape
// (LevelLifecycleSubsystem.h:7). UMainMenuWidget binds ONE shared handler to every
// instance's OnLevelSelected; each instance supplies its own MapName at broadcast
// time, so the shared handler always knows which level was picked.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMainMenuLevelSelected, FName, MapName);

UCLASS()
class KROWDKONTROL_API UMainMenuLevelButtonWidget : public UUserWidget
{
	GENERATED_BODY()

	friend class FKrowdKontrolMainMenuLevelSelectTest;
	friend class FKrowdKontrolReservedGameplayColoursTest;

public:
	// Must be called once, immediately after construction (UMainMenuWidget::
	// PopulateLevelSelectButtons() does this right after ConstructWidget()) - sets
	// both the visible label and the payload OnLevelSelected broadcasts on click.
	// Safe to call before or after the tree is built.
	UFUNCTION(BlueprintCallable, Category = "Main Menu")
	void SetLevelMapName(FName InMapName);

	UFUNCTION(BlueprintPure, Category = "Main Menu")
	FName GetLevelMapName() const { return MapName; }

	UPROPERTY(BlueprintAssignable, Category = "Main Menu")
	FOnMainMenuLevelSelected OnLevelSelected;

protected:
	virtual void NativeOnInitialized() override;
	virtual bool Initialize() override;

private:
	void BuildWidgetTree();
	void EnsureWidgetTreeBuilt();
	void RefreshLabel();

	// Bound to LevelButton->OnClicked. Re-broadcasts as OnLevelSelected(MapName) -
	// see this class's header comment for why a payload-carrying re-broadcast is
	// needed here instead of binding UMainMenuWidget directly to LevelButton.
	UFUNCTION()
	void HandleClicked();

	FName MapName = NAME_None;

	UPROPERTY()
	TObjectPtr<UButton> LevelButton;

	UPROPERTY()
	TObjectPtr<UTextBlock> LevelButtonLabel;
};
