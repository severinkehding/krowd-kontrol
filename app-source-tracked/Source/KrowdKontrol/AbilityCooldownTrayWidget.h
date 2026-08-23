#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AbilitySlot.h"
#include "AbilityCooldownTrayWidget.generated.h"

class UBorder;
class UAbilityUnlockComponent;
class UAbilityLockoutComponent;
class UAbilityCooldownComponent;
class UTextBlock;
class UCanvasPanel;
class UProgressBar;

// Ability/cooldown tray HUD widget (PRD 13 REQ-2): shows all 5 crowd-control ability
// icon slots simultaneously, corner-anchored, each with a cooldown-remaining overlay,
// so the player can check "what's ready" without looking away from the crowd for more
// than a glance. No real ability-cast/cooldown gameplay system exists yet (tracked
// separately, issue #71) - this widget builds its own UI tree in C++ (no Widget
// Blueprint asset, mirroring UPostRunSummaryWidget's issue #74 precedent) and seeds
// each slot with a distinct placeholder cooldown duration on construction, purely to
// prove the visualization works. StartCooldown() is the wiring point a future real
// ability-cast system replaces the placeholder seeding with.
// Distinct, checkable state per ability tile (issue #261) - PunishmentLockout and
// NotYetUnlocked both render with the same locked-style border/label (see
// UpdateSlotVisual), but are backed by independent state and are distinguishable
// via this accessor (PunishmentLockout additionally shows a live numeric
// countdown; NotYetUnlocked never does). Precedence when multiple could apply:
// PunishmentLockout > NotYetUnlocked > Cooldown > Ready.
UENUM(BlueprintType)
enum class EAbilityTileState : uint8
{
	Ready,
	Cooldown,
	PunishmentLockout,
	NotYetUnlocked
};

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

	// The wiring point Punishment 1's real ability-lockout system (PRD 08, issue #178)
	// calls to (un)lock a slot, via BindAbilityLockoutComponent() below binding
	// directly to UAbilityLockoutComponent::OnAbilityLockoutChanged.
	UFUNCTION(BlueprintCallable, Category = "Ability Cooldown Tray")
	void SetSlotLocked(EAbilitySlot AbilitySlot, bool bLocked);

	UFUNCTION(BlueprintPure, Category = "Ability Cooldown Tray")
	float GetSlotRemainingSeconds(EAbilitySlot AbilitySlot) const;

	UFUNCTION(BlueprintPure, Category = "Ability Cooldown Tray")
	bool IsSlotOnCooldown(EAbilitySlot AbilitySlot) const;

	UFUNCTION(BlueprintPure, Category = "Ability Cooldown Tray")
	bool IsSlotLocked(EAbilitySlot AbilitySlot) const;

	// Single-value, provably-distinct state per tile (issue #261 acceptance
	// criterion) - prefer this over combining IsSlotOnCooldown()/IsSlotLocked()
	// individually when the caller needs to tell punishment lockout apart from
	// not-yet-unlocked.
	UFUNCTION(BlueprintPure, Category = "Ability Cooldown Tray")
	EAbilityTileState GetSlotState(EAbilitySlot AbilitySlot) const;

	// Live remaining seconds for the punishment-lockout state specifically (0 if the
	// slot isn't currently punishment-locked) - distinct from GetSlotRemainingSeconds()
	// which reports the underlying cooldown timer, and unaffected by it.
	UFUNCTION(BlueprintPure, Category = "Ability Cooldown Tray")
	float GetSlotPunishmentLockoutRemainingSeconds(EAbilitySlot AbilitySlot) const;

	// Production wiring for issue #69's level-gated unlocks: initializes every slot's
	// locked visual from the component's current unlock state (Stun unlocked, the
	// rest locked at run start) and keeps it live by subscribing to OnAbilityUnlocked,
	// so a slot flips to unlocked the moment its level is reached. The HUD wiring
	// issue (#132) calls this once at widget creation with the player pawn's
	// component; until that lands the Automation test drives it directly.
	UFUNCTION(BlueprintCallable, Category = "Ability Cooldown Tray")
	void BindAbilityUnlockComponent(UAbilityUnlockComponent* UnlockComponent);

	// Production wiring for issue #178's Punishment 1 (real ability lockout on contact
	// damage): binds a dedicated adapter (HandleAbilityLockoutChanged below) to the
	// component's OnAbilityLockoutChanged delegate, and keeps a weak reference to the
	// component so RefreshPunishmentLockoutReadouts() can poll its live remaining time
	// every frame (issue #261 - punishment lockout needs its own numeric readout,
	// distinct from the plain locked-style visual SetSlotLocked drives for
	// not-yet-unlocked). Unlike BindAbilityUnlockComponent above, no per-slot seeding
	// loop is needed - a freshly bound pawn's UAbilityLockoutComponent never has an
	// active lockout yet.
	UFUNCTION(BlueprintCallable, Category = "Ability Cooldown Tray")
	void BindAbilityLockoutComponent(UAbilityLockoutComponent* LockoutComponent);

	// Polls the bound UAbilityLockoutComponent's live remaining-time for every
	// currently punishment-locked slot and refreshes its numeric readout - called
	// every frame from NativeTick() (mirrors AdvanceCooldowns()'s own call site) and
	// directly by the Automation test, which can't drive a live tick loop under the
	// -nullrhi headless run (same reasoning as AdvanceCooldowns()).
	UFUNCTION(BlueprintCallable, Category = "Ability Cooldown Tray")
	void RefreshPunishmentLockoutReadouts();

	// Production wiring for issue #71's real ability-cast/cooldown gating: binds a
	// dedicated adapter (HandleAbilityCooldownChanged below) to the component's
	// OnAbilityCooldownChanged delegate, switching the tray's per-frame update from the
	// self-decrementing placeholder timer to polling this component's live remaining
	// time (see RefreshCooldownReadouts()/NativeTick()). Mirrors
	// BindAbilityLockoutComponent's weak-ref/rebind-safe/AddUniqueDynamic shape. Like
	// that function, a freshly bound pawn's component never has an active cooldown yet,
	// so no per-slot seeding loop is needed here either.
	UFUNCTION(BlueprintCallable, Category = "Ability Cooldown Tray")
	void BindAbilityCooldownComponent(UAbilityCooldownComponent* CooldownComponent);

	// Polls the bound UAbilityCooldownComponent's live remaining-time for every
	// currently-on-cooldown slot and refreshes its fill/numeric readout - called every
	// frame from NativeTick() when bound (mirrors RefreshPunishmentLockoutReadouts()'s
	// own call site) and directly by the Automation test, which can't drive a live tick
	// loop under the -nullrhi headless run.
	UFUNCTION(BlueprintCallable, Category = "Ability Cooldown Tray")
	void RefreshCooldownReadouts();

	// Automation-test-friendly structured accessor for a slot's current fill percent
	// (0-1), avoiding the need for pixel/OCR inspection of the UProgressBar itself.
	UFUNCTION(BlueprintPure, Category = "Ability Cooldown Tray")
	float GetSlotCooldownFillFraction(EAbilitySlot AbilitySlot) const;

	UFUNCTION(BlueprintPure, Category = "Ability Cooldown Tray")
	bool IsSlotReadyFlashActive(EAbilitySlot AbilitySlot) const;

	UFUNCTION(BlueprintPure, Category = "Ability Cooldown Tray")
	float GetSlotReadyFlashRemainingSeconds(EAbilitySlot AbilitySlot) const;

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
	UFUNCTION()
	void HandleAbilityUnlocked(EAbilitySlot Ability);

	// Adapter for OnAbilityLockoutChanged (issue #261) - unlike HandleAbilityUnlocked's
	// one-line SetSlotLocked() forward, this also seeds/clears
	// SlotPunishmentLockoutRemaining from the bound component so the tile's numeric
	// readout has a correct value on the very frame the lockout starts, before
	// RefreshPunishmentLockoutReadouts() next runs.
	UFUNCTION()
	void HandleAbilityLockoutChanged(EAbilitySlot Ability, bool bLocked);

	// Adapter for OnAbilityCooldownChanged (issue #259) - on start, seeds the tray's
	// cooldown state from the real component's current remaining time via
	// StartCooldown(); on expiry, clears the readout/fill and plays the ready-flash.
	UFUNCTION()
	void HandleAbilityCooldownChanged(EAbilitySlot Ability, bool bOnCooldown);

	// Starts (or restarts) a slot's brief brightness pulse - called on real-cooldown
	// expiry (HandleAbilityCooldownChanged) and on the placeholder path's natural
	// expiry (AdvanceCooldowns), so the flash is consistent regardless of bound-vs-
	// placeholder mode.
	void PlayReadyFlash(EAbilitySlot AbilitySlot);

	// Decrements every slot's active ready-flash timer. Called every frame from
	// NativeTick() and directly by the Automation test, which can't drive a live tick
	// loop under the -nullrhi headless run (same reasoning as AdvanceCooldowns()).
	UFUNCTION(BlueprintCallable, Category = "Ability Cooldown Tray")
	void AdvanceReadyFlashTimers(float DeltaSeconds);

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

	// Punishment-lockout-specific state (issue #261) - deliberately separate from
	// SlotLocked (which continues to mean "not-yet-unlocked" only) so the two
	// locked-style states remain independently trackable and distinguishable via
	// GetSlotState(), even though they currently share the same border/label
	// treatment in UpdateSlotVisual().
	UPROPERTY()
	TArray<bool> SlotPunishmentLockoutActive;

	UPROPERTY()
	TArray<float> SlotPunishmentLockoutRemaining;

	// Weak - the tray widget does not own the pawn's lockout component's lifetime.
	// Used by RefreshPunishmentLockoutReadouts() to poll GetRemainingLockoutSeconds()
	// every frame; left unset (IsValid() == false) until BindAbilityLockoutComponent()
	// is called with a non-null component.
	UPROPERTY()
	TWeakObjectPtr<UAbilityLockoutComponent> BoundLockoutComponent;

	// Per-slot vertical-drain fill bar (issue #259) reflecting remaining-cooldown
	// fraction - built alongside the other per-slot arrays in BuildWidgetTree().
	UPROPERTY()
	TArray<TObjectPtr<UProgressBar>> SlotCooldownFillBars;

	// Remaining ready-flash time per slot (issue #259) - >0 while a slot's brief
	// brightness pulse is active, driven down every frame by AdvanceReadyFlashTimers().
	UPROPERTY()
	TArray<float> SlotReadyFlashRemaining;

	// Weak - the tray widget does not own the pawn's cooldown component's lifetime.
	// Used by RefreshCooldownReadouts() to poll GetRemainingCooldownSeconds() every
	// frame; left unset (IsValid() == false) until BindAbilityCooldownComponent() is
	// called with a non-null component. NativeTick() branches on whether this is valid
	// to decide between polling the real component vs. running the placeholder timer.
	UPROPERTY()
	TWeakObjectPtr<UAbilityCooldownComponent> BoundCooldownComponent;

	// Duration of the ready-flash brightness pulse - matches
	// UEnergyMeterWidget::DamageFlashDurationSeconds's value for consistency across HUD
	// widgets.
	static constexpr float ReadyFlashDurationSeconds = 0.15f;

	// Fixed square size, in pixels, for the new fill bar's USizeBox - a bare
	// UProgressBar has no useful intrinsic size in this widget's auto-sized
	// UHorizontalBox layout.
	static constexpr float IconTileSizePx = 48.0f;

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
