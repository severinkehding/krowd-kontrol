#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "EnergyMeterWidget.generated.h"

class UBorder;
class UProgressBar;
class UTextBlock;
class UCanvasPanel;
class UPlayerEnergyComponent;

// Energy meter HUD widget (PRD 13 REQ-1): a persistent, top-left-corner-anchored
// bar/gauge showing the player's energy level, so the player can gauge how much
// punishment they can still absorb without looking away from the crowd. Top-left is
// the corner diagonally opposite UAbilityCooldownTrayWidget's bottom-right anchor
// (issue #66), per PRD 13 REQ-2's "opposite" requirement. This widget builds its own
// UI tree in C++ (no Widget Blueprint asset, mirroring UPostRunSummaryWidget's issue
// #74 precedent) and seeds itself with a placeholder value on construction so it's
// self-demonstrating; BindToEnergyComponent() is the wiring point that swaps in a
// real UPlayerEnergyComponent (issue #78) for live updates.
UCLASS()
class KROWDKONTROL_API UEnergyMeterWidget : public UUserWidget
{
	GENERATED_BODY()

	friend class FKrowdKontrolEnergyMeterWidgetTest;
	friend class FKrowdKontrolReservedGameplayColoursTest;

public:
	// The low-level wiring point: clamps CurrentEnergy to [0, MaxEnergy] and updates
	// both the fill bar and the numeric readout.
	UFUNCTION(BlueprintCallable, Category = "Energy Meter")
	void SetEnergy(float CurrentEnergy, float MaxEnergy);

	// Higher-level extension point: unsubscribes from any previously-bound component,
	// subscribes to EnergyComponent's OnEnergyChanged for live updates, and syncs
	// immediately to its current state.
	UFUNCTION(BlueprintCallable, Category = "Energy Meter")
	void BindToEnergyComponent(UPlayerEnergyComponent* EnergyComponent);

	// Read-only accessors for what's currently displayed - used by the Automation
	// Framework test, also generally useful to anything that wants to confirm the
	// meter's state without re-deriving formatting.
	UFUNCTION(BlueprintPure, Category = "Energy Meter")
	float GetDisplayedFraction() const;

	UFUNCTION(BlueprintPure, Category = "Energy Meter")
	FText GetEnergyDisplayText() const;

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

private:
	void BuildWidgetTree();

	// Builds the widget tree exactly once, regardless of which of
	// NativeOnInitialized()/Initialize() fires first - mirrors
	// UAbilityCooldownTrayWidget::EnsureWidgetTreeBuilt().
	void EnsureWidgetTreeBuilt();

	// Bound to BoundEnergyComponent's OnEnergyChanged via AddUniqueDynamic - must be a
	// real UFUNCTION() for that to compile, same as any BlueprintAssignable delegate
	// binding.
	UFUNCTION()
	void HandleEnergyChanged(float NewEnergy);

	UPROPERTY()
	TObjectPtr<UBorder> BackgroundBorder;

	UPROPERTY()
	TObjectPtr<UProgressBar> FillBar;

	UPROPERTY()
	TObjectPtr<UTextBlock> ValueText;

	UPROPERTY()
	TObjectPtr<UPlayerEnergyComponent> BoundEnergyComponent;

	// Placeholder values only - real energy tracking lives in UPlayerEnergyComponent
	// (issue #78), consumed via BindToEnergyComponent().
	static constexpr float PlaceholderCurrentEnergy = 72.0f;
	static constexpr float PlaceholderMaxEnergy = 100.0f;

	// Fixed pixel footprint from the viewport's top-left corner. A bare UProgressBar
	// has no useful intrinsic desired size the way a text-labelled UBorder does, so
	// this widget uses an explicit size instead of the tray's AutoSize(true) approach.
	static constexpr float MeterMarginPx = 24.0f;
	static constexpr float MeterWidthPx = 220.0f;
	static constexpr float MeterHeightPx = 28.0f;
};
