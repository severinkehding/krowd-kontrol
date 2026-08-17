// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "KrowdKontrolPlayerController.generated.h"

class UAbilityCooldownTrayWidget;
class UEnergyMeterWidget;
class APlaceholderTargetZoneActor;

// Owns and wires the project's persistent HUD widgets (PRD 13) into the viewport.
// Neither playable level (L_FlatCamera3DPrototype, L_Paper2DPrototype) had any
// PlayerController/GameMode driving this before issue #132 - see that issue and
// AbilityCooldownTrayWidget.h's BindAbilityUnlockComponent() comment for why this
// class exists.
UCLASS()
class KROWDKONTROL_API AKrowdKontrolPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "HUD")
	TObjectPtr<UAbilityCooldownTrayWidget> AbilityTrayWidget;

	UPROPERTY(BlueprintReadOnly, Category = "HUD")
	TObjectPtr<UEnergyMeterWidget> EnergyMeterWidgetInstance;

	// Beacon hook (issue #132's third scoped deliverable, PRD 13 REQ-6): the live
	// world-space target-zone beacons, collected at BeginPlay and re-collectable via
	// RefreshTargetZoneBeacons(). APlaceholderTargetZoneActor is a pure visual with
	// no API of its own (see its header), so the hook is deliberately just this
	// collection point - the wiring surface future target-zone HUD work consumes
	// (off-screen beacon indicators, and issue #80's real ATargetZone banking, both
	// out of scope here). Read-only snapshot; stale entries are pruned on refresh.
	UPROPERTY(BlueprintReadOnly, Category = "HUD")
	TArray<TObjectPtr<APlaceholderTargetZoneActor>> TargetZoneBeacons;

	// Re-scans the world for live APlaceholderTargetZoneActor instances into
	// TargetZoneBeacons. Called from BeginPlay; call again after spawning/removing
	// beacons at runtime. Returns the number found.
	UFUNCTION(BlueprintCallable, Category = "HUD")
	int32 RefreshTargetZoneBeacons();

protected:
	virtual void BeginPlay() override;
	virtual void OnPossess(APawn* InPawn) override;

private:
	// Constructs both HUD widgets and adds them to the viewport. Idempotent - a repeat
	// call (e.g. accidental double BeginPlay) is a no-op if the widgets already exist.
	void CreateHUDWidgets();

	// Binds the newly-possessed pawn's UAbilityUnlockComponent/UPlayerEnergyComponent
	// (if present - FindComponentByClass returns nullptr otherwise, and both Bind*
	// methods already tolerate nullptr) to the corresponding HUD widget.
	void WireWidgetsToPawn(APawn* InPawn);
};
