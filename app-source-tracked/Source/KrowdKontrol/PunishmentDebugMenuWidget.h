#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PunishmentDebugMenuWidget.generated.h"

class UCheckBox;
class UAbilityLockoutComponent;
class USpeedReductionPunishmentComponent;
class UOvercrowdDetectionComponent;

// Per-punishment debug/accessibility menu (issue #26): three independent checkboxes,
// one per punishment (ability-lockout, speed-reduction, Overcrowd), so an Alpha
// playtester can isolate which punishment drives observed behaviour without editing
// config files or restarting the game. This is the menu-UI follow-up
// docs/prd-punishment-system.md REQ-5 explicitly deferred ("CVars are enough at this
// stage"), not a replacement of its CVar mechanism - every checkbox both flips its
// punishment's existing kk.Punishment.*Enabled CVar (the same CVars
// UAbilityLockoutComponent/USpeedReductionPunishmentComponent/UOvercrowdDetectionComponent
// already gate their real activation on) and, only on checked->unchecked, calls that
// punishment's existing/new instant-end method (EndAllLockouts() /
// EndSpeedReduction() / ForceEndPanicOverload()) so an already-active effect ends
// immediately rather than merely being prevented from re-triggering.
//
// Hidden by default, toggled by F1 via AKrowdKontrolPlayerController::HandleToggleDebugMenu().
// First C++ UCheckBox usage in this codebase - everything else mirrors
// UAbilityCooldownTrayWidget's no-Widget-Blueprint, WidgetTree->ConstructWidget<T>() shape.
UCLASS()
class KROWDKONTROL_API UPunishmentDebugMenuWidget : public UUserWidget
{
	GENERATED_BODY()

	friend class FKrowdKontrolPunishmentDebugMenuWidgetTest;

public:
	UFUNCTION(BlueprintCallable, Category = "Punishment Debug Menu")
	void BindPunishmentComponents(
		UAbilityLockoutComponent* InLockoutComponent,
		USpeedReductionPunishmentComponent* InSpeedReductionComponent,
		UOvercrowdDetectionComponent* InOvercrowdComponent);

	UFUNCTION(BlueprintCallable, Category = "Punishment Debug Menu")
	void ToggleMenuVisibility();

	UFUNCTION(BlueprintPure, Category = "Punishment Debug Menu")
	bool IsMenuVisible() const;

	UFUNCTION(BlueprintPure, Category = "Punishment Debug Menu")
	bool IsLockoutToggleChecked() const;

	UFUNCTION(BlueprintPure, Category = "Punishment Debug Menu")
	bool IsSpeedReductionToggleChecked() const;

	UFUNCTION(BlueprintPure, Category = "Punishment Debug Menu")
	bool IsOvercrowdToggleChecked() const;

protected:
	virtual void NativeOnInitialized() override;
	virtual bool Initialize() override;

private:
	void BuildWidgetTree();
	void EnsureWidgetTreeBuilt();

	UFUNCTION()
	void HandleLockoutCheckStateChanged(bool bIsChecked);

	UFUNCTION()
	void HandleSpeedReductionCheckStateChanged(bool bIsChecked);

	UFUNCTION()
	void HandleOvercrowdCheckStateChanged(bool bIsChecked);

	UPROPERTY()
	TObjectPtr<UCheckBox> LockoutCheckBox;

	UPROPERTY()
	TObjectPtr<UCheckBox> SpeedReductionCheckBox;

	UPROPERTY()
	TObjectPtr<UCheckBox> OvercrowdCheckBox;

	UPROPERTY()
	TWeakObjectPtr<UAbilityLockoutComponent> BoundLockoutComponent;

	UPROPERTY()
	TWeakObjectPtr<USpeedReductionPunishmentComponent> BoundSpeedReductionComponent;

	UPROPERTY()
	TWeakObjectPtr<UOvercrowdDetectionComponent> BoundOvercrowdComponent;
};
