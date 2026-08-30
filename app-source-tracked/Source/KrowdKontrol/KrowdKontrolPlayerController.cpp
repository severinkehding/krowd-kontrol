// Fill out your copyright notice in the Description page of Project Settings.

#include "KrowdKontrolPlayerController.h"
#include "AbilityCooldownTrayWidget.h"
#include "EnergyMeterWidget.h"
#include "OnScreenPromptWidget.h"
#include "QuestTrackerWidget.h"
#include "BriefingCardWidget.h"
#include "PostRunSummaryWidget.h"
#include "AbilityUnlockComponent.h"
#include "AbilityUnlockLevelSubsystem.h"
#include "LevelBriefingSubsystem.h"
#include "AbilityUnlockPromptComponent.h"
#include "AbilityLockoutComponent.h"
#include "AbilityCooldownComponent.h"
#include "PunishmentDebugMenuWidget.h"
#include "SpeedReductionPunishmentComponent.h"
#include "OvercrowdDetectionComponent.h"
#include "PlayerEnergyComponent.h"
#include "Blueprint/UserWidget.h"
#include "PlaceholderTargetZoneActor.h"
#include "EngineUtils.h"
#include "LevelFailComponent.h"
#include "LevelClearTimeSubsystem.h"
#include "LevelLifecycleSubsystem.h"
#include "BossBase.h"
#include "CrowdMasteryTotalSubsystem.h"
#include "TargetZone.h"
#include "RoomActor.h"
#include "Components/BoxComponent.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "InputCoreTypes.h"

void AKrowdKontrolPlayerController::BeginPlay()
{
	Super::BeginPlay();
	// In-game cursor (issue #262, PRD "Cursor & Aiming Foundation" REQ-1) - every
	// ability in this PRD is aimed at the mouse cursor, so it must be visible
	// during real gameplay. Independent of DefaultInput.ini's
	// DefaultViewportMouseCaptureMode=CapturePermanently_IncludingInitialMouseDown /
	// DefaultViewportMouseLockMode=LockOnCapture settings - those govern whether
	// the OS cursor is confined/locked to the viewport, not whether a cursor is
	// rendered at all. (Restored by the operator 2026-08-23 after a concurrent fix
	// run clobbered this block out of the shared workspace - see PR #266's notes.)
	bShowMouseCursor = true;
	CreateHUDWidgets();
	RefreshTargetZoneBeacons();
	RetryPendingBriefing();
	// If the pawn was already possessed before BeginPlay ran (order isn't guaranteed
	// relative to AutoPossessPlayer), wire it now instead of waiting for a
	// possession that already happened.
	if (APawn* CurrentPawn = GetPawn())
	{
		WireWidgetsToPawn(CurrentPawn);
		ApplyBossCheckpointIfRequested(CurrentPawn);
		RetryPendingAbilityUnlock(CurrentPawn);
		ApplyStarterSkillEffects(CurrentPawn);
	}
}

void AKrowdKontrolPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	WireWidgetsToPawn(InPawn);
	ApplyBossCheckpointIfRequested(InPawn);
	RetryPendingAbilityUnlock(InPawn);
	ApplyStarterSkillEffects(InPawn);
}

void AKrowdKontrolPlayerController::CreateHUDWidgets()
{
	if (!AbilityTrayWidget)
	{
		AbilityTrayWidget = CreateWidget<UAbilityCooldownTrayWidget>(this, UAbilityCooldownTrayWidget::StaticClass());
		if (AbilityTrayWidget)
		{
			AbilityTrayWidget->AddToViewport();
		}
	}
	if (!EnergyMeterWidgetInstance)
	{
		EnergyMeterWidgetInstance = CreateWidget<UEnergyMeterWidget>(this, UEnergyMeterWidget::StaticClass());
		if (EnergyMeterWidgetInstance)
		{
			EnergyMeterWidgetInstance->AddToViewport();
		}
	}
	if (!OnScreenPromptWidgetInstance)
	{
		OnScreenPromptWidgetInstance = CreateWidget<UOnScreenPromptWidget>(this, UOnScreenPromptWidget::StaticClass());
		if (OnScreenPromptWidgetInstance)
		{
			OnScreenPromptWidgetInstance->AddToViewport();
		}
	}
	if (!QuestTrackerWidgetInstance)
	{
		QuestTrackerWidgetInstance = CreateWidget<UQuestTrackerWidget>(this, UQuestTrackerWidget::StaticClass());
		if (QuestTrackerWidgetInstance)
		{
			QuestTrackerWidgetInstance->AddToViewport();
		}
	}
	if (!BriefingCardWidgetInstance)
	{
		BriefingCardWidgetInstance = CreateWidget<UBriefingCardWidget>(this, UBriefingCardWidget::StaticClass());
		if (BriefingCardWidgetInstance)
		{
			BriefingCardWidgetInstance->AddToViewport();
		}
	}
	if (!PostRunSummaryWidgetInstance)
	{
		// Deliberately no AddToViewport() here, unlike every sibling block above - this
		// widget stays off-screen until it self-adds itself from its own OnLevelClear
		// handler (see PostRunSummaryWidget.h's PostRunSummaryWidgetInstance comment and
		// UPostRunSummaryWidget::HandleLevelClear()).
		PostRunSummaryWidgetInstance = CreateWidget<UPostRunSummaryWidget>(this, UPostRunSummaryWidget::StaticClass());
	}
	if (!PunishmentDebugMenuWidgetInstance)
	{
		PunishmentDebugMenuWidgetInstance = CreateWidget<UPunishmentDebugMenuWidget>(this, UPunishmentDebugMenuWidget::StaticClass());
		if (PunishmentDebugMenuWidgetInstance)
		{
			// Unlike PostRunSummaryWidgetInstance, this one IS added to the viewport
			// immediately - it starts Collapsed (set in its own BuildWidgetTree()) and
			// is shown/hidden in place by ToggleMenuVisibility(), not deferred until a
			// later event.
			PunishmentDebugMenuWidgetInstance->AddToViewport();
		}
	}
	// Covers the race where ULevelBriefingSubsystem::HandleLevelBegin() already
	// called ShowLevelBriefing() - buffering the row because BriefingCardWidgetInstance
	// didn't exist yet - before CreateHUDWidgets() ran (same shape as the
	// OnScreenPromptWidgetInstance flush above, issue #235).
	if (bHasPendingLevelBriefing && BriefingCardWidgetInstance)
	{
		bHasPendingLevelBriefing = false;
		BriefingCardWidgetInstance->ShowBriefing(PendingLevelBriefingRow);
	}
}

void AKrowdKontrolPlayerController::ShowLevelBriefing(const FLevelBriefingRow& Row)
{
	if (!BriefingCardWidgetInstance)
	{
		bHasPendingLevelBriefing = true;
		PendingLevelBriefingRow = Row;
		return;
	}
	BriefingCardWidgetInstance->ShowBriefing(Row);
}

void AKrowdKontrolPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	// bConsumeInput MUST be false here: the controller's InputComponent is processed
	// BEFORE the possessed pawn's in APlayerController::BuildInputStack's ordering, so a
	// consuming AnyKey binding at this level eats every key press (WASD axes included -
	// a consumed key contributes nothing to its axes that frame) before
	// AFlatCamera3DPrototypePawn ever sees it. This is exactly the all-input-dead
	// regression both 2026-08-24/26 operator playtests hit (present since PR #272
	// introduced this binding).
	FInputKeyBinding& BriefingDismissBinding = InputComponent->BindKey(EKeys::AnyKey, IE_Pressed, this, &AKrowdKontrolPlayerController::HandleBriefingDismissInput);
	BriefingDismissBinding.bConsumeInput = false;
	// bExecuteWhenPaused MUST be true: UBriefingCardWidget::ShowBriefing() pauses the
	// world, and UE substitutes an empty delegate for any binding without this flag
	// while paused (PlayerInput.cpp's bGamePaused check) - so without it the dismiss
	// binding can never fire during the one window it exists for, and the card only
	// ever closed via its 8s auto-dismiss timer (PR #309 code review, finding 1).
	// Safe against input leaking: bGamePaused is snapshotted per frame, so the
	// unpausing keypress still contributes nothing to that frame's pawn axes.
	BriefingDismissBinding.bExecuteWhenPaused = true;
	InputComponent->BindKey(EKeys::F1, IE_Pressed, this, &AKrowdKontrolPlayerController::HandleToggleDebugMenu);
}

void AKrowdKontrolPlayerController::HandleBriefingDismissInput()
{
	if (BriefingCardWidgetInstance && BriefingCardWidgetInstance->IsBriefingVisible())
	{
		BriefingCardWidgetInstance->DismissBriefing();
	}
}

void AKrowdKontrolPlayerController::HandleToggleDebugMenu()
{
	if (PunishmentDebugMenuWidgetInstance)
	{
		PunishmentDebugMenuWidgetInstance->ToggleMenuVisibility();
	}
}

void AKrowdKontrolPlayerController::WireWidgetsToPawn(APawn* InPawn)
{
	if (!InPawn)
	{
		return;
	}
	if (AbilityTrayWidget)
	{
		AbilityTrayWidget->BindAbilityUnlockComponent(InPawn->FindComponentByClass<UAbilityUnlockComponent>());
		AbilityTrayWidget->BindAbilityLockoutComponent(InPawn->FindComponentByClass<UAbilityLockoutComponent>());
		AbilityTrayWidget->BindAbilityCooldownComponent(InPawn->FindComponentByClass<UAbilityCooldownComponent>());
	}
	if (QuestTrackerWidgetInstance)
	{
		QuestTrackerWidgetInstance->BindAbilityUnlockComponent(InPawn->FindComponentByClass<UAbilityUnlockComponent>());
	}
	if (EnergyMeterWidgetInstance)
	{
		EnergyMeterWidgetInstance->BindToEnergyComponent(InPawn->FindComponentByClass<UPlayerEnergyComponent>());
	}
	if (PunishmentDebugMenuWidgetInstance)
	{
		PunishmentDebugMenuWidgetInstance->BindPunishmentComponents(
			InPawn->FindComponentByClass<UAbilityLockoutComponent>(),
			InPawn->FindComponentByClass<USpeedReductionPunishmentComponent>(),
			InPawn->FindComponentByClass<UOvercrowdDetectionComponent>());
	}
	if (OnScreenPromptWidgetInstance)
	{
		// Covers the race where UAbilityUnlockLevelSubsystem::HandleLevelBegin() already
		// broadcast OnAbilityUnlocked - and so UAbilityUnlockPromptComponent already
		// tried and failed to resolve a widget - before CreateHUDWidgets() ran. This
		// call runs after CreateHUDWidgets() every time WireWidgetsToPawn() does (from
		// both BeginPlay() and OnPossess()), so it's guaranteed to see a live widget at
		// least once regardless of which of OnLevelBegin/BeginPlay/OnPossess fired first.
		if (UAbilityUnlockPromptComponent* PromptComponent = InPawn->FindComponentByClass<UAbilityUnlockPromptComponent>())
		{
			PromptComponent->FlushPendingPrompts();
		}
	}
	if (ULevelFailComponent* PreviouslyWired = WiredLevelFailComponent.Get())
	{
		PreviouslyWired->OnLevelFailed.RemoveDynamic(this, &AKrowdKontrolPlayerController::HandleLevelFailed);
	}
	if (ULevelFailComponent* LevelFailComp = InPawn->FindComponentByClass<ULevelFailComponent>())
	{
		LevelFailComp->OnLevelFailed.AddUniqueDynamic(this, &AKrowdKontrolPlayerController::HandleLevelFailed);
		WiredLevelFailComponent = LevelFailComp;
	}
}

void AKrowdKontrolPlayerController::HandleLevelFailed()
{
	if (APawn* ControlledPawn = GetPawn())
	{
		ControlledPawn->DisableInput(this);
	}

	if (ULevelClearTimeSubsystem* Subsystem = ResolveLevelClearTimeSubsystem())
	{
		if (UWorld* World = GetWorld())
		{
			Subsystem->DiscardLevelTimer(FName(*World->GetMapName()));
		}
	}

	RequestLevelRestart();
}

void AKrowdKontrolPlayerController::RequestLevelRestart(bool bFreshRun)
{
	bLastRestartWasFreshRun = bFreshRun;
	if (!bFreshRun)
	{
		bRestartRequested = true;
	}

	UWorld* World = GetWorld();
	// Real map travel only makes sense in an actual game world (PIE or packaged) -
	// never in the Editor-type Worlds FAutomationEditorCommonUtils::CreateNewMap()
	// returns for KrowdKontrol.Unit.* tests, where OpenLevel would try to travel a
	// World that was never loaded from a real map package, hanging the Automation
	// run (Epic Developer Community forums report this hang for in-process map
	// loads inside Automation tests; see issue #172). bRestartRequested above is
	// what the Automation Framework test asserts instead; the real reload is
	// verified manually in PIE (see this issue's PR body).
	// A fresh voluntary rerun always targets the level's own start - the
	// boss-checkpoint restore ComputeRestartOptions() computes is a defeat-restart
	// affordance only (issue #342), so bFreshRun skips it entirely rather than
	// resetting the underlying HasReachedBossCheckpoint() latch (which stays latched
	// for the rest of this World's lifetime by design). Computed unconditionally (not
	// just inside the IsGameWorld() guard below) so GetLastComputedRestartOptions() -
	// this issue's Automation test seam - can prove this ternary picks the right branch
	// even from a CreateNewMap() World that never reaches the real OpenLevel() call.
	LastComputedRestartOptions = bFreshRun ? FString() : ComputeRestartOptions();
	if (World && World->IsGameWorld())
	{
		UGameplayStatics::OpenLevel(this, ComputeRestartLevelName(), false, LastComputedRestartOptions);
	}
}

FName AKrowdKontrolPlayerController::ComputeRestartLevelName() const
{
	const UWorld* World = GetWorld();
	return World ? StripPIEPrefixFromMapName(World->GetMapName()) : NAME_None;
}

FName AKrowdKontrolPlayerController::StripPIEPrefixFromMapName(const FString& MapName)
{
	return FName(*UWorld::RemovePIEPrefix(MapName));
}

FString AKrowdKontrolPlayerController::ComputeRestartOptions() const
{
	const UWorld* World = GetWorld();
	const ULevelLifecycleSubsystem* LifecycleSubsystem = World ? World->GetSubsystem<ULevelLifecycleSubsystem>() : nullptr;
	if (!LifecycleSubsystem || !LifecycleSubsystem->HasReachedBossCheckpoint())
	{
		return FString();
	}
	return TEXT("BossCheckpoint");
}

void AKrowdKontrolPlayerController::ApplyBossCheckpointIfRequested(APawn* InPawn)
{
	if (!InPawn || bBossCheckpointApplied)
	{
		return;
	}
	UWorld* World = GetWorld();
	if (!World || !World->URL.HasOption(TEXT("BossCheckpoint")))
	{
		return;
	}
	bBossCheckpointApplied = true;
	for (TActorIterator<ABossBase> It(World); It; ++It)
	{
		InPawn->SetActorLocation(It->GetActorLocation());
		return;
	}
	UE_LOG(LogTemp, Warning,
		TEXT("AKrowdKontrolPlayerController: reload requested a BossCheckpoint teleport ")
		TEXT("but no ABossBase actor exists in the reloaded world '%s' - pawn left at default spawn."),
		*World->GetMapName());
}

void AKrowdKontrolPlayerController::RetryPendingAbilityUnlock(APawn* InPawn)
{
	if (UAbilityUnlockLevelSubsystem* UnlockSubsystem = GetWorld() ? GetWorld()->GetSubsystem<UAbilityUnlockLevelSubsystem>() : nullptr)
	{
		UnlockSubsystem->RetryPendingUnlockForPawn(InPawn);
	}
}

void AKrowdKontrolPlayerController::RetryPendingBriefing()
{
	if (ULevelBriefingSubsystem* BriefingSubsystem = GetWorld() ? GetWorld()->GetSubsystem<ULevelBriefingSubsystem>() : nullptr)
	{
		BriefingSubsystem->RetryPendingBriefingForController(this);
	}
}

ULevelClearTimeSubsystem* AKrowdKontrolPlayerController::ResolveLevelClearTimeSubsystem()
{
	if (CachedLevelClearTimeSubsystem)
	{
		return CachedLevelClearTimeSubsystem;
	}
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		CachedLevelClearTimeSubsystem = GameInstance->GetSubsystem<ULevelClearTimeSubsystem>();
	}
	if (!CachedLevelClearTimeSubsystem && !bHasWarnedMissingLevelClearTimeSubsystem)
	{
		bHasWarnedMissingLevelClearTimeSubsystem = true;
		UE_LOG(LogTemp, Warning,
			TEXT("AKrowdKontrolPlayerController: no ULevelClearTimeSubsystem available - ")
			TEXT("a level-failed run's in-progress timer cannot be discarded."));
	}
	return CachedLevelClearTimeSubsystem;
}

namespace
{
	const FName EffectHook_AbilityCooldownReduction(TEXT("AbilityCooldownReduction"));
	const FName EffectHook_EnergyMaxIncrease(TEXT("EnergyMaxIncrease"));
	const FName EffectHook_MovementSpeedBonus(TEXT("MovementSpeedBonus"));
	const FName EffectHook_PenZoneRadiusBonus(TEXT("PenZoneRadiusBonus"));

	constexpr float StarterCooldownReductionMultiplier = 0.8f;   // 20% shorter cooldowns
	constexpr float StarterEnergyMaxBonusMultiplier = 1.25f;     // +25% energy ceiling
	constexpr float StarterMoveSpeedBonusMultiplier = 1.15f;     // +15% top speed
	constexpr float StarterPenZoneRadiusBonusMultiplier = 1.2f;  // +20% pen-zone catch radius
}

void AKrowdKontrolPlayerController::ApplyStarterSkillEffects(APawn* InPawn)
{
	if (!InPawn || bStarterSkillEffectsApplied)
	{
		return;
	}

	UCrowdMasteryTotalSubsystem* MasterySubsystem = ResolveCrowdMasteryTotalSubsystem();
	if (!MasterySubsystem)
	{
		return;
	}
	bStarterSkillEffectsApplied = true;

	const TArray<FName> UnlockedHookIds = MasterySubsystem->GetUnlockedEffectHookIds();

	if (UnlockedHookIds.Contains(EffectHook_AbilityCooldownReduction))
	{
		if (UAbilityCooldownComponent* CooldownComponent = InPawn->FindComponentByClass<UAbilityCooldownComponent>())
		{
			for (float& Duration : CooldownComponent->AbilityCooldownDurations)
			{
				Duration *= StarterCooldownReductionMultiplier;
			}
		}
	}
	if (UnlockedHookIds.Contains(EffectHook_EnergyMaxIncrease))
	{
		if (UPlayerEnergyComponent* EnergyComponent = InPawn->FindComponentByClass<UPlayerEnergyComponent>())
		{
			EnergyComponent->MaxEnergy *= StarterEnergyMaxBonusMultiplier;
		}
	}
	if (UnlockedHookIds.Contains(EffectHook_MovementSpeedBonus))
	{
		if (UFloatingPawnMovement* Movement = InPawn->FindComponentByClass<UFloatingPawnMovement>())
		{
			Movement->MaxSpeed *= StarterMoveSpeedBonusMultiplier;
		}
	}
	if (UnlockedHookIds.Contains(EffectHook_PenZoneRadiusBonus))
	{
		if (UWorld* World = GetWorld())
		{
			for (TActorIterator<ATargetZone> It(World); It; ++It)
			{
				if (UBoxComponent* Box = It->ZoneCollisionComponent)
				{
					Box->SetBoxExtent(Box->GetUnscaledBoxExtent() * StarterPenZoneRadiusBonusMultiplier);
				}
			}
			// Re-bake any already-shown banking-radius ring so it keeps reflecting the
			// real extent (TargetZone.h:32-36 honesty invariant, issue #365/#393) -
			// EnsureBankingZonesWired() is idempotent and safe to call more than once
			// (RoomActor.cpp), and is the same refresh entry point ARoomActor::BeginPlay()
			// itself uses to pair a zone with its ring marker.
			for (TActorIterator<ARoomActor> RoomIt(World); RoomIt; ++RoomIt)
			{
				RoomIt->EnsureBankingZonesWired();
			}
		}
	}
}

UCrowdMasteryTotalSubsystem* AKrowdKontrolPlayerController::ResolveCrowdMasteryTotalSubsystem()
{
	if (CachedCrowdMasteryTotalSubsystem)
	{
		return CachedCrowdMasteryTotalSubsystem;
	}
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		CachedCrowdMasteryTotalSubsystem = GameInstance->GetSubsystem<UCrowdMasteryTotalSubsystem>();
	}
	if (!CachedCrowdMasteryTotalSubsystem && !bHasWarnedMissingCrowdMasteryTotalSubsystem)
	{
		bHasWarnedMissingCrowdMasteryTotalSubsystem = true;
		UE_LOG(LogTemp, Warning,
			TEXT("AKrowdKontrolPlayerController: no UCrowdMasteryTotalSubsystem available - ")
			TEXT("starter skill effects cannot be applied."));
	}
	return CachedCrowdMasteryTotalSubsystem;
}

int32 AKrowdKontrolPlayerController::RefreshTargetZoneBeacons()
{
	TargetZoneBeacons.Reset();
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<APlaceholderTargetZoneActor> It(World); It; ++It)
		{
			TargetZoneBeacons.Add(*It);
		}
	}
	return TargetZoneBeacons.Num();
}

void AKrowdKontrolPlayerController::Cheat_ZeroPlayerEnergy()
{
	APawn* ControlledPawn = GetPawn();
	UPlayerEnergyComponent* Energy = ControlledPawn ? ControlledPawn->FindComponentByClass<UPlayerEnergyComponent>() : nullptr;
	if (!Energy)
	{
		return;
	}

	// ApplyContactDamage clamps each call to MaxDamagePerHit, so fully draining a higher
	// MaxEnergy can take several calls - looping through the sole legal mutator instead
	// of adding a setter. Breaks on a zero-progress call (rather than a fixed iteration
	// count) so a misconfigured MaxDamagePerHit of 0 can't spin this forever.
	while (Energy->GetCurrentEnergy() > 0.0f)
	{
		if (Energy->ApplyContactDamage(Energy->GetCurrentEnergy(), this) <= 0.0f)
		{
			break;
		}
	}
}
