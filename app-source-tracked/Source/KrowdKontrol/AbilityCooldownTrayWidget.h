#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AbilitySlot.h"
#include "AbilityCooldownTrayWidget.generated.h"

class UBorder;
class UTextBlock;
class UCanvasPanel;

// Ability/cooldown tray HUD widget (PRD 13 REQ-2): shows all 5 crowd-control ability
// icon slots simultaneously, corner-anchored, each with a cooldown-remaining overlay,
// so the player can check "what's ready" without looking away from the crowd for more
// than a glance. No real ability-cast/cooldown gameplay system exists yet (tracked
// separately, issue #71) - this widget builds its own UI tree in C++ (no Widget
// Blueprint asset, mirroring UPostRunSummaryWidget's issue #74 precedent) and seeds
// each slot with a distinct placeholder cooldown duration on construction, purely to
// prove the visualization works. StartCooldown() is the wiring point a future real
// ability-cast system replaces the placeholder seeding with.
UCLASS()
class KROWDKONTROL_API UAbilityCooldownTrayWidget : public UUserWidget
{
	GENERATED_BODY()

	friend class FKrowdKontrolAbilityCooldownTrayWidgetTest;
	friend class FKrowdKontrolReservedGameplayColoursTest;

public:
	static constexpr int32 NumAbilitySlots = static_cast<int32>(EAbilitySlot::Count);

	// The wiring point a future real ability-cast system (issue #71) calls to
	// (re)trigger a slot's cooldown. Also used internally by the placeholder seeding.
	// NOTE: parameter is named AbilitySlot, not Slot - UWidget already declares a
	// public member named Slot (its owning UPanelSlot), and UHT rejects a UFUNCTION
	// parameter that shadows an inherited member.
	UFUNCTION(BlueprintCallable, Category = "Ability Cooldown Tray")
	void StartCooldown(EAbilitySlot AbilitySlot, float DurationSeconds);

	// Decrements every active slot's remaining cooldown time and updates its overlay.
	// Called every frame from NativeTick() once this widget is in a live viewport, and
	// called directly by the Automation test (which can't drive NativeTick under the
	// -nullrhi headless run).
	UFUNCTION(BlueprintCallable, Category = "Ability Cooldown Tray")
	void AdvanceCooldowns(float DeltaSeconds);

	// The wiring point a future Punishment 1 lockout system (PRD 08, not yet built)
	// calls to (un)lock a slot. No real lockout gameplay logic exists yet - this is a
	// placeholder driver only, mirroring StartCooldown()'s own precedent above.
	UFUNCTION(BlueprintCallable, Category = "Ability Cooldown Tray")
	void SetSlotLocked(EAbilitySlot AbilitySlot, bool bLocked);

	UFUNCTION(BlueprintPure, Category = "Ability Cooldown Tray")
	float GetSlotRemainingSeconds(EAbilitySlot AbilitySlot) const;

	UFUNCTION(BlueprintPure, Category = "Ability Cooldown Tray")
	bool IsSlotOnCooldown(EAbilitySlot AbilitySlot) const;

	UFUNCTION(BlueprintPure, Category = "Ability Cooldown Tray")
	bool IsSlotLocked(EAbilitySlot AbilitySlot) const;

	// Read-only accessor for what's currently displayed - used by the Automation
	// Framework test, also generally useful to anything that wants to confirm the
	// tray's state without re-deriving formatting.
	UFUNCTION(BlueprintPure, Category = "Ability Cooldown Tray")
	FText GetSlotCooldownDisplayText(EAbilitySlot AbilitySlot) const;

	UFUNCTION(BlueprintPure, Category = "Ability Cooldown Tray")
	FText GetSlotIconLabelText(EAbilitySlot AbilitySlot) const;

	UFUNCTION(BlueprintPure, Category = "Ability Cooldown Tray")
	FLinearColor GetSlotIconTintColour(EAbilitySlot AbilitySlot) const;

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
	// UPostRunSummaryWidget::EnsureWidgetTreeBuilt().
	void EnsureWidgetTreeBuilt();

	// Starts all 5 slots on cooldown with distinct placeholder durations, so all 5
	// begin "triggering" simultaneously but independently timed.
	void SeedPlaceholderCooldowns();

	// Pushes a slot's current remaining-time state to its UTextBlock/visibility.
	void UpdateSlotVisual(EAbilitySlot AbilitySlot);

	UPROPERTY()
	TArray<TObjectPtr<UBorder>> SlotIconBorders;

	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> SlotCooldownTexts;

	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> SlotIconLabels;

	// Runtime state, not designer config - hence no EditDefaultsOnly/EditAnywhere, and
	// private so no code path - Blueprint or C++ - can mutate it except through
	// StartCooldown()/AdvanceCooldowns().
	UPROPERTY()
	TArray<float> SlotCooldownRemaining;

	UPROPERTY()
	TArray<float> SlotCooldownDuration;

	// Runtime state, not designer config - hence no EditDefaultsOnly/EditAnywhere, and
	// private so no code path - Blueprint or C++ - can mutate it except through
	// SetSlotLocked(). Punishment 1's ability lockout (PRD 08, not yet built) is meant
	// to look structurally distinct from an ordinary cooldown, not just be a longer
	// one - see AbilityCooldownComponent.h for why that separation stays out of this
	// widget's cooldown-tracking arrays entirely.
	UPROPERTY()
	TArray<bool> SlotLocked;

	// Distinct placeholder durations (Stun/Sleep/Root/Fear/Snare) - not real ability
	// balance data (issue #71 owns that) - chosen distinct so each slot's countdown and
	// clearing can be independently verified.
	static constexpr float PlaceholderCooldownDurations[NumAbilitySlots] = { 2.0f, 3.0f, 4.0f, 5.0f, 6.0f };

	// Inward margin, in pixels, from the viewport's bottom-right corner. Bottom-right
	// is diagonally opposite the energy meter's top-left anchor (issue #64, landed),
	// per PRD 13 REQ-2's "opposite" requirement. See issue #66's plan Risks section
	// for why bottom-right (not top-right) was picked before the meter existed.
	static constexpr float TrayMarginPx = 24.0f;
};
