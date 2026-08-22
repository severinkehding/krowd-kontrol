// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "LevelBriefingData.h"
#include "KrowdKontrolPlayerController.generated.h"

class UAbilityCooldownTrayWidget;
class UEnergyMeterWidget;
class UOnScreenPromptWidget;
class UBriefingCardWidget;
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
	// precedent - GetGameInstance() is null in CreateNewMap() test Worlds, so each test
	// injects a directly-constructed ULevelClearTimeSubsystem instead of resolving one.
	// Used by this controller's own level-failed test (issue #171) and, since issue #172,
	// by the level-restart test that drives the same HandleLevelFailed() path onward into
	// RequestLevelRestart().
	friend class FKrowdKontrolLevelFailedTest;
	friend class FKrowdKontrolLevelRestartTest;
	friend class FKrowdKontrolBossCheckpointRestartTest;

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

	// Pre-level briefing card (issue #246). Populated on
	// ULevelLifecycleSubsystem::OnLevelBegin via ShowLevelBriefing() below.
	UPROPERTY(BlueprintReadOnly, Category = "HUD")
	TObjectPtr<UBriefingCardWidget> BriefingCardWidgetInstance;

	// Called by ULevelBriefingSubsystem::HandleLevelBegin. If BriefingCardWidgetInstance
	// doesn't exist yet (OnLevelBegin fires before CreateHUDWidgets(), same race issue
	// #235 fixed for OnScreenPromptWidgetInstance), buffers Row and CreateHUDWidgets()
	// flushes it once the widget is live.
	UFUNCTION(BlueprintCallable, Category = "HUD")
	void ShowLevelBriefing(const FLevelBriefingRow& Row);

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

	// Set true the moment HandleLevelFailed() requests a level restart (issue #172,
	// PRD "Run Lifecycle & Progression Signals" REQ-4) - before the real map reload
	// (which only happens in an actual game world, never inside an Automation test
	// World - see RequestLevelRestart()'s comment). This is what the Automation
	// Framework test asserts, since the real reload itself can't be observed from
	// inside a single in-process test World.
	UFUNCTION(BlueprintPure, Category = "Level Restart")
	bool WasRestartRequested() const { return bRestartRequested; }

protected:
	virtual void BeginPlay() override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void SetupInputComponent() override;

private:
	// Constructs all HUD widgets and adds them to the viewport. Idempotent - a repeat
	// call (e.g. accidental double BeginPlay) is a no-op if the widgets already exist.
	void CreateHUDWidgets();

	// Bound to EKeys::AnyKey in SetupInputComponent() (issue #246): dismisses the
	// briefing card on any player input, if one is currently showing. No-ops
	// otherwise, so a stray keypress with no briefing up is harmless.
	UFUNCTION()
	void HandleBriefingDismissInput();

	// Set true when ShowLevelBriefing() arrives before BriefingCardWidgetInstance
	// exists (OnLevelBegin racing CreateHUDWidgets(), same shape as
	// UAbilityUnlockPromptComponent's FlushPendingPrompts() race, issue #235).
	// CreateHUDWidgets() flushes this once the widget is live.
	bool bHasPendingLevelBriefing = false;
	FLevelBriefingRow PendingLevelBriefingRow;

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
	// Automation test worlds, so the Automation Framework test injects a
	// directly-constructed instance into CachedLevelClearTimeSubsystem via the
	// friendship above instead of going through this resolver.
	ULevelClearTimeSubsystem* ResolveLevelClearTimeSubsystem();

	UPROPERTY()
	TObjectPtr<ULevelClearTimeSubsystem> CachedLevelClearTimeSubsystem;

	bool bHasWarnedMissingLevelClearTimeSubsystem = false;

	// The LevelFailComponent WireWidgetsToPawn last bound HandleLevelFailed to.
	// WireWidgetsToPawn runs on both BeginPlay and OnPossess, so a repossession
	// (not built yet - REQ-4's restart flow will be the first caller) could otherwise
	// leave a previous pawn's component still bound, letting a stale broadcast act on
	// whatever pawn GetPawn() currently returns instead of the one that actually failed.
	UPROPERTY()
	TWeakObjectPtr<ULevelFailComponent> WiredLevelFailComponent;

	// Called at the end of HandleLevelFailed() (issue #172, PRD REQ-4). Sets
	// bRestartRequested and, only in a real game world, reloads the current map via
	// UGameplayStatics::OpenLevel.
	void RequestLevelRestart();

	// Extracted from RequestLevelRestart() so the level-restart Automation test can
	// assert the reload target is computed correctly without ever calling the real,
	// Automation-World-hanging UGameplayStatics::OpenLevel() (issue #172 test-coverage
	// follow-up). Mirrors EnemyBase.h's TickCheckDetection/TickChaseMovement pattern of
	// extracting an otherwise-untestable private behavior into a friend-testable seam.
	FName ComputeRestartLevelName() const;

	// Strips PIE session name-mangling (e.g. "UEDPIE_0_L_Level01" -> "L_Level01") from
	// a map name via UWorld::RemovePIEPrefix() (issue #223). Extracted as its own static
	// helper - rather than inlined into ComputeRestartLevelName() - purely so the
	// Automation test can feed it a synthetic PIE-mangled string directly: this project's
	// CreateNewMap() test Worlds are never PIE sessions, so there is no other way to
	// exercise the stripping behavior in-process. A no-op on already-bare names (the
	// CreateNewMap() case), so the existing "restart targets the current map by name"
	// assertion in KrowdKontrolLevelRestartTest.cpp is unaffected.
	static FName StripPIEPrefixFromMapName(const FString& MapName);

	// Computes the OpenLevel Options string for the pending restart (issue #173, PRD
	// "Run Lifecycle & Progression Signals" REQ-4 boss-checkpoint sub-requirement).
	// Returns "BossCheckpoint" if this world's ULevelLifecycleSubsystem has latched
	// HasReachedBossCheckpoint(), else an empty string - the only cross-OpenLevel-reload
	// signal available, since UTickableWorldSubsystem state (including the checkpoint
	// flag itself) does not survive a real map reload. Extracted the same way
	// ComputeRestartLevelName() was, so the Automation test can assert the computed
	// value without invoking the real, Automation-World-hanging OpenLevel().
	FString ComputeRestartOptions() const;

	// Teleports InPawn to the first ABossBase actor's placed location if this world was
	// reloaded with the "BossCheckpoint" OpenLevel option set (issue #173). No-ops if
	// the option is absent (the normal case - only ComputeRestartOptions() above ever
	// sets it) or if the world has no ABossBase actor. Called from both BeginPlay()'s
	// already-possessed branch and OnPossess(), mirroring WireWidgetsToPawn()'s own
	// dual-call-site shape, since AutoPossessPlayer's timing relative to BeginPlay isn't
	// guaranteed (see BeginPlay()'s own comment). Assumes one ABossBase per level (true
	// of every level today, MISSION.md's "4 total boss encounters") - does not
	// cross-check the chosen actor's state against the one that latched the checkpoint
	// in RefreshBossCheckpointState(). Guarded by bBossCheckpointApplied below so a
	// later re-possession in the same reloaded world can't re-teleport. Revisit if a
	// level ever places more than one ABossBase actor.
	void ApplyBossCheckpointIfRequested(APawn* InPawn);

	// Forwards to UAbilityUnlockLevelSubsystem::RetryPendingUnlockForPawn(), in case
	// this level's OnLevelBegin already fired before InPawn was possessed (same
	// AutoPossessPlayer-timing hazard ApplyBossCheckpointIfRequested's comment above
	// describes). Called from both BeginPlay()'s already-possessed branch and
	// OnPossess(), mirroring the same dual-call-site shape. No-ops if no unlock is
	// pending or no UAbilityUnlockLevelSubsystem exists.
	void RetryPendingAbilityUnlock(APawn* InPawn);

	// Never reset back to false once set - moot in the real game-world path, since a
	// successful OpenLevel() destroys this controller along with the rest of the old
	// World. Left true for the (Automation-World-only) lifetime of a controller that
	// never actually reloads.
	bool bRestartRequested = false;

	// One-shot guard for ApplyBossCheckpointIfRequested(), same never-reset-once-set
	// idiom as bRestartRequested above. The "BossCheckpoint" FURL option it reads
	// persists for the World's whole lifetime (unlike this flag), so without this a
	// later re-possession of the same controller in the same reloaded world (no call
	// site does this today) would re-teleport the pawn and could double-log the
	// missing-boss warning.
	bool bBossCheckpointApplied = false;
};
