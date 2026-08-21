// Fill out your copyright notice in the Description page of Project Settings.

#include "KrowdKontrolPlayerController.h"
#include "AbilityCooldownTrayWidget.h"
#include "EnergyMeterWidget.h"
#include "OnScreenPromptWidget.h"
#include "AbilityUnlockComponent.h"
#include "AbilityLockoutComponent.h"
#include "PlayerEnergyComponent.h"
#include "Blueprint/UserWidget.h"
#include "PlaceholderTargetZoneActor.h"
#include "EngineUtils.h"
#include "LevelFailComponent.h"
#include "LevelClearTimeSubsystem.h"
#include "LevelLifecycleSubsystem.h"
#include "BossBase.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

void AKrowdKontrolPlayerController::BeginPlay()
{
	Super::BeginPlay();
	CreateHUDWidgets();
	RefreshTargetZoneBeacons();
	// If the pawn was already possessed before BeginPlay ran (order isn't guaranteed
	// relative to AutoPossessPlayer), wire it now instead of waiting for a
	// possession that already happened.
	if (APawn* CurrentPawn = GetPawn())
	{
		WireWidgetsToPawn(CurrentPawn);
		ApplyBossCheckpointIfRequested(CurrentPawn);
	}
}

void AKrowdKontrolPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	WireWidgetsToPawn(InPawn);
	ApplyBossCheckpointIfRequested(InPawn);
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
	}
	if (EnergyMeterWidgetInstance)
	{
		EnergyMeterWidgetInstance->BindToEnergyComponent(InPawn->FindComponentByClass<UPlayerEnergyComponent>());
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

void AKrowdKontrolPlayerController::RequestLevelRestart()
{
	bRestartRequested = true;

	UWorld* World = GetWorld();
	// Real map travel only makes sense in an actual game world (PIE or packaged) -
	// never in the Editor-type Worlds FAutomationEditorCommonUtils::CreateNewMap()
	// returns for KrowdKontrol.Unit.* tests, where OpenLevel would try to travel a
	// World that was never loaded from a real map package, hanging the Automation
	// run (Epic Developer Community forums report this hang for in-process map
	// loads inside Automation tests; see issue #172). bRestartRequested above is
	// what the Automation Framework test asserts instead; the real reload is
	// verified manually in PIE (see this issue's PR body).
	if (World && World->IsGameWorld())
	{
		UGameplayStatics::OpenLevel(this, ComputeRestartLevelName(), false, ComputeRestartOptions());
	}
}

FName AKrowdKontrolPlayerController::ComputeRestartLevelName() const
{
	const UWorld* World = GetWorld();
	return World ? FName(*World->GetMapName()) : NAME_None;
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
