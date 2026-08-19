// Fill out your copyright notice in the Description page of Project Settings.

#include "KrowdKontrolPlayerController.h"
#include "AbilityCooldownTrayWidget.h"
#include "EnergyMeterWidget.h"
#include "OnScreenPromptWidget.h"
#include "AbilityUnlockComponent.h"
#include "PlayerEnergyComponent.h"
#include "Blueprint/UserWidget.h"
#include "PlaceholderTargetZoneActor.h"
#include "EngineUtils.h"
#include "LevelFailComponent.h"
#include "LevelClearTimeSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

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
	}
}

void AKrowdKontrolPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	WireWidgetsToPawn(InPawn);
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
