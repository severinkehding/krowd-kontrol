#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AbilitySlot.h"
#include "AbilityTooltipWidget.generated.h"

class UBorder;
class UTextBlock;

// Ability tray hover tooltip (issue #260, PRD 13 REQ-2): a small self-built (no
// Widget Blueprint asset) UUserWidget attached via UWidget::SetToolTip() to each of
// UAbilityCooldownTrayWidget's 5 slot icon borders. Renders the ability's name,
// canonical key binding, one-line effect description, duration, range/shape, and
// colour-matched (or colour-neutral) countered enemy type with its reserved-colour
// swatch - all sourced from AbilityData via a single SetAbility() call. Standard UMG
// hover detection does the showing/hiding; no custom mouse-enter/leave code here.
// Mirrors UOnScreenPromptWidget's lifecycle boilerplate exactly - no NativeTick()
// override, since tooltip content is static once SetAbility() is called.
UCLASS()
class KROWDKONTROL_API UAbilityTooltipWidget : public UUserWidget
{
	GENERATED_BODY()

	friend class FKrowdKontrolAbilityTooltipWidgetTest;
	friend class FKrowdKontrolReservedGameplayColoursTest;

public:
	// Populates every text row and the swatch from AbilityData::Get(AbilitySlot).
	// Called once by UAbilityCooldownTrayWidget::BuildWidgetTree() right after this
	// widget is constructed.
	UFUNCTION(BlueprintCallable, Category = "Ability Tooltip")
	void SetAbility(EAbilitySlot AbilitySlot);

	UFUNCTION(BlueprintPure, Category = "Ability Tooltip")
	FText GetAbilityNameText() const;

	UFUNCTION(BlueprintPure, Category = "Ability Tooltip")
	FText GetKeyBindingText() const;

	UFUNCTION(BlueprintPure, Category = "Ability Tooltip")
	FText GetDescriptionText() const;

	UFUNCTION(BlueprintPure, Category = "Ability Tooltip")
	FText GetDurationText() const;

	UFUNCTION(BlueprintPure, Category = "Ability Tooltip")
	FText GetRangeShapeText() const;

	UFUNCTION(BlueprintPure, Category = "Ability Tooltip")
	FText GetEnemyTypeText() const;

	UFUNCTION(BlueprintPure, Category = "Ability Tooltip")
	FLinearColor GetSwatchColour() const;

protected:
	// Fires synchronously from CreateWidget(), before any Slate/viewport realization -
	// matters for the -nullrhi headless Automation run this project's tests use (see
	// UPostRunSummaryWidget's NativeOnInitialized() precedent, issue #74).
	virtual void NativeOnInitialized() override;

	// Safety net mirroring UOnScreenPromptWidget::Initialize() - guarantees
	// WidgetTree->RootWidget exists before this widget's first TakeWidget() call even
	// when CreateWidget() is called without an owning player/controller.
	virtual bool Initialize() override;

private:
	void BuildWidgetTree();

	// Builds the widget tree exactly once, regardless of which of
	// NativeOnInitialized()/Initialize() fires first - mirrors
	// UOnScreenPromptWidget::EnsureWidgetTreeBuilt().
	void EnsureWidgetTreeBuilt();

	UPROPERTY()
	TObjectPtr<UBorder> RootBorder;

	UPROPERTY()
	TObjectPtr<UTextBlock> NameText;

	UPROPERTY()
	TObjectPtr<UTextBlock> KeyBindText;

	UPROPERTY()
	TObjectPtr<UTextBlock> DescriptionText;

	UPROPERTY()
	TObjectPtr<UTextBlock> DurationText;

	UPROPERTY()
	TObjectPtr<UTextBlock> RangeShapeText;

	UPROPERTY()
	TObjectPtr<UTextBlock> EnemyTypeText;

	UPROPERTY()
	TObjectPtr<UBorder> SwatchBorder;
};
