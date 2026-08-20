// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "KrowdKontrolPlayerController.generated.h"

class UAbilityCooldownTrayWidget;
class UEnergyMeterWidget;
class UOnScreenPromptWidget;
class APlaceholderTargetZoneActor;
class ULevelClearTimeSubsystem;
class ULevelFailComponent;

// Owns and wires the project's persistent HUD widgets (PRD 13) into the viewport.
// Neither playable level (L_FlatCamera3DPrototype, L_Paper2DPrototype) had any
// PlayerController/GameMode driving this before issue #132 - see that issue and
// AbilityCooldownTrayWidget.h's BindAbilityUnlockComponent() comment for why this
// class exists.
UCLASS()
class KROWDKONTROL_API AKrowdKontrolPlayerController : public APlayerController
{
	GENERATED_BODY()

	// Grants the Automation Framework tests direct access to CachedLevelClearTimeSubsystem,
	// mirroring UGizmoFirstContactComponent's FKrowdKontrolGizmoFirstContactComponentTest
	// precedent - GetGameInstance() is null in CreateNewMap() test Worlds, so the tests
	// inject a directly-constructed ULevelClearTimeSubsystem instead of resolving one.
	friend class FKrowdKontrolLevelFailedTest;
	friend class FKrowdKontrolLevelClearTimeWiringTest;

public:
	UPROPERTY(BlueprintReadOnly, Category = "HUD")
	TObjectPtr<UAbilityCooldownTrayWidget> AbilityTrayWidget;

	UPROPERTY(BlueprintReadOnly, Category = "HUD")
	TObjectPtr<UEnergyMeterWidget> EnergyMeterWidgetInstance;

	// Backs the additional-help nudge (issue #40) and any future prompt-driven
	// feature - a real, live UOnScreenPromptWidget instance for such consumers to
	// find and drive. Unlike AbilityTrayWidget/EnergyMeterWidgetInstance above,
	// nothing in WireWidgetsToPawn() binds a pawn component to this widget;
	// consumers reach out to the controller instead (the opposite direction).
	UPROPERTY(BlueprintReadOnly, Category = "HUD")
	TObjectPtr<UOnScreenPromptWidget> OnScreenPromptWidgetInstance;

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

	// QA/E2E hook (issue #183 pass-1 feedback): drains the possessed pawn's
	// UPlayerEnergyComponent to 0 entirely through repeated ApplyContactDamage() calls -
	// never a direct setter - so PlayerEnergyComponent's "ApplyContactDamage is the only
	// permitted mutator" invariant (see its header) stays intact. Lets a live PIE session
	// (via the console or an MCP-driven console-command call) deterministically reach the
	// zero-energy precondition and observe OnLevelFailed/DisableInput fire, instead of
	// depending on which enemy types happen to be in a test scene. UFUNCTION(Exec) can't
	// be wrapped in a UE_BUILD_SHIPPING preprocessor block (UHT rejects UFUNCTION inside
	// non-WITH_EDITORONLY_DATA preprocessor blocks) - same as every other Exec cheat
	// command in Unreal, it stays declared in Shipping and relies on Exec's own
	// runtime-only dispatch (never reachable without an open console) rather than a
	// compile-time guard.
	UFUNCTION(Exec)
	void Cheat_ZeroPlayerEnergy();

protected:
	virtual void BeginPlay() override;
	virtual void OnPossess(APawn* InPawn) override;

private:
	// Constructs all HUD widgets and adds them to the viewport. Idempotent - a repeat
	// call (e.g. accidental double BeginPlay) is a no-op if the widgets already exist.
	void CreateHUDWidgets();

	// Binds the newly-possessed pawn's UAbilityUnlockComponent/UPlayerEnergyComponent
	// (if present - FindComponentByClass returns nullptr otherwise, and both Bind*
	// methods already tolerate nullptr) to the corresponding HUD widget.
	void WireWidgetsToPawn(APawn* InPawn);

	// Bound to each possessed pawn's ULevelFailComponent::OnLevelFailed in
	// WireWidgetsToPawn (issue #171, PRD "Run Lifecycle & Progression Signals" REQ-3).
	// Incapacitates the pawn (input disabled, no death animation/ragdoll per the
	// issue's placeholder-first note) and discards - never records - the level's
	// in-progress clear timer, since a failed run must never become a personal best.
	UFUNCTION()
	void HandleLevelFailed();

	// Resolves (and caches) the current UGameInstance's ULevelClearTimeSubsystem,
	// mirroring UGizmoFirstContactComponent::ResolveNarrativeSubsystem()'s exact
	// pattern - GetGameInstance() is null in this project's CreateNewMap()-based
	// Automation test worlds, so the Automation Framework tests inject a
	// directly-constructed instance into CachedLevelClearTimeSubsystem via the
	// friendship above instead of going through this resolver. Called from both
	// BeginPlay() (issue #170, to wire SubscribeToLevelLifecycle()) and
	// HandleLevelFailed() (issue #171, to discard an in-progress timer) - the two
	// share bHasWarnedMissingLevelClearTimeSubsystem below, so whichever runs first
	// claims the one-shot warning.
	ULevelClearTimeSubsystem* ResolveLevelClearTimeSubsystem();

	UPROPERTY()
	TObjectPtr<ULevelClearTimeSubsystem> CachedLevelClearTimeSubsystem;

	bool bHasWarnedMissingLevelClearTimeSubsystem = false;

	// Warn-once flag for BeginPlay()'s SubscribeToLevelLifecycle() wiring (issue #170)
	// finding no ULevelLifecycleSubsystem on this world - separate from
	// bHasWarnedMissingLevelClearTimeSubsystem above since it covers a different
	// missing dependency (the world subsystem, not the game-instance subsystem).
	bool bHasWarnedMissingLevelLifecycleSubsystem = false;

	// The LevelFailComponent WireWidgetsToPawn last bound HandleLevelFailed to.
	// WireWidgetsToPawn runs on both BeginPlay and OnPossess, so a repossession
	// (not built yet - REQ-4's restart flow will be the first caller) could otherwise
	// leave a previous pawn's component still bound, letting a stale broadcast act on
	// whatever pawn GetPawn() currently returns instead of the one that actually failed.
	UPROPERTY()
	TWeakObjectPtr<ULevelFailComponent> WiredLevelFailComponent;
};
