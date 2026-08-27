#include "TeachingPromptComponent.h"
#include "OnScreenPromptWidget.h"
#include "KrowdKontrolPlayerController.h"
#include "EnemyBase.h"
#include "ThreatState.h"
#include "RoomActor.h"
#include "TargetZone.h"
#include "AbilityUnlockLevelSubsystem.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

UTeachingPromptComponent::UTeachingPromptComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UTeachingPromptComponent::BeginPlay()
{
	Super::BeginPlay();

	UWorld* World = GetWorld();
	bIsLevel1 = World && UAbilityUnlockLevelSubsystem::ParseLevelIndexFromMapName(FName(*World->GetMapName())) == 1;
	// pass-1 validation feedback, PR #306: the E2E holdout could not confirm any of the
	// four prompts fired in a live PIE session. This log line lets a follow-up E2E pass
	// confirm via GetLogEntries whether the map was even resolved as Level 1 - ruling
	// that in or out doesn't require catching a prompt's ~2s on-screen window.
	UE_LOG(LogTemp, Log, TEXT("UTeachingPromptComponent: BeginPlay on map '%s' - bIsLevel1=%s"),
		World ? *World->GetMapName() : TEXT("<no world>"), bIsLevel1 ? TEXT("true") : TEXT("false"));
	if (!bIsLevel1)
	{
		SetComponentTickEnabled(false);
		return;
	}

	for (TActorIterator<ARoomActor> It(World); It; ++It)
	{
		It->OnRoomClearedStateChanged.AddUniqueDynamic(this, &UTeachingPromptComponent::HandleAnyRoomClearedStateChanged);
	}
}

void UTeachingPromptComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bIsLevel1)
	{
		return;
	}

	if (!bHasFiredStunPrompt)
	{
		CheckStunPromptFireCondition();
	}
	if (bHasFiredControlPrompt && !bHasDismissedControlPrompt)
	{
		CheckControlPromptDismissCondition();
	}
	if (bHasFiredControlPrompt && !bHasFiredDropPrompt)
	{
		CheckDropPromptFireCondition();
	}

	if (bHasDismissedStunPrompt && bHasDismissedControlPrompt && bHasDismissedDropPrompt && bHasFiredRoomClearPrompt)
	{
		SetComponentTickEnabled(false);
	}
}

void UTeachingPromptComponent::HandleAbilityCastApplied(EAbilitySlot Ability, AEnemyBase* TargetEnemy)
{
	if (!bIsLevel1)
	{
		return;
	}

	if (Ability == EAbilitySlot::Stun && bHasFiredStunPrompt && !bHasDismissedStunPrompt)
	{
		bHasDismissedStunPrompt = true;
	}

	if (!bHasFiredControlPrompt)
	{
		bHasFiredControlPrompt = true;
		FirstControlledEnemy = TargetEnemy;
		if (TargetEnemy)
		{
			TargetEnemy->OnEnemyBanked.AddDynamic(this, &UTeachingPromptComponent::HandleFirstControlledEnemyBanked);
		}

		if (UOnScreenPromptWidget* Widget = ResolvePromptWidget())
		{
			Widget->ShowPrompt(NSLOCTEXT("TeachingPromptComponent", "ControlPrompt", "IT FOLLOWS YOU — WALK"));
		}
	}
}

void UTeachingPromptComponent::CheckStunPromptFireCondition()
{
	if (bHasFiredStunPrompt)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (TActorIterator<AEnemyBase> It(World); It; ++It)
	{
		if (It->GetThreatState() == EThreatState::Hot)
		{
			// Only latch the one-shot guard once the prompt is actually shown. Unlike
			// the other three prompts (which fire well after gameplay has started, once
			// the widget is certainly resolvable), this one can trigger on the very
			// first tick for an enemy placed right at the player's spawn - burning the
			// guard on a failed ResolvePromptWidget() there would silently and
			// permanently drop the prompt instead of retrying next tick.
			if (UOnScreenPromptWidget* Widget = ResolvePromptWidget())
			{
				bHasFiredStunPrompt = true;
				// pass-1 validation feedback, PR #306: confirms the fire condition and
				// ShowPrompt() call both actually ran in a live PIE session (via
				// GetLogEntries), independent of whether the ~2s on-screen window was
				// caught by visual polling.
				UE_LOG(LogTemp, Log, TEXT("UTeachingPromptComponent: stun prompt shown for '%s' (GetThreatState() == Hot)"),
					*GetNameSafe(*It));
				Widget->ShowPrompt(NSLOCTEXT("TeachingPromptComponent", "StunPrompt", "STUN IT — PRESS 1"));
			}
			return;
		}
	}
}

void UTeachingPromptComponent::CheckControlPromptDismissCondition()
{
	if (!FirstControlledEnemy.IsValid() || FirstControlledEnemy->GetEnemyState() != EEnemyState::Controlled)
	{
		return;
	}

	const APawn* OwningPawn = Cast<APawn>(GetOwner());
	if (OwningPawn && OwningPawn->GetVelocity().SizeSquared() > KINDA_SMALL_NUMBER)
	{
		bHasDismissedControlPrompt = true;
	}
}

void UTeachingPromptComponent::CheckDropPromptFireCondition()
{
	if (bHasFiredDropPrompt || !FirstControlledEnemy.IsValid()
		|| FirstControlledEnemy->GetEnemyState() != EEnemyState::Controlled)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FVector EnemyLocation = FirstControlledEnemy->GetActorLocation();
	const float RangeSquared = FMath::Square(DropZoneProximityUnits);
	for (TActorIterator<ATargetZone> It(World); It; ++It)
	{
		const float DistSquared = FVector::DistSquared(It->GetActorLocation(), EnemyLocation);
		if (DistSquared <= RangeSquared)
		{
			bHasFiredDropPrompt = true;
			if (UOnScreenPromptWidget* Widget = ResolvePromptWidget())
			{
				Widget->ShowPrompt(NSLOCTEXT("TeachingPromptComponent", "DropPrompt", "DROP IT ON THE GLOWING PEN"));
			}
			return;
		}
	}
}

void UTeachingPromptComponent::HandleFirstControlledEnemyBanked()
{
	bHasDismissedDropPrompt = true;
}

void UTeachingPromptComponent::HandleAnyRoomClearedStateChanged()
{
	if (bHasFiredRoomClearPrompt)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (TActorIterator<ARoomActor> It(World); It; ++It)
	{
		if (It->GetOwnedEnemies().Num() > 0 && It->IsRoomCleared())
		{
			bHasFiredRoomClearPrompt = true;
			if (UOnScreenPromptWidget* Widget = ResolvePromptWidget())
			{
				Widget->ShowPrompt(NSLOCTEXT("TeachingPromptComponent", "RoomClearPrompt", "ROOM CLEAR — DOOR OPEN"));
			}
			return;
		}
	}
}

UOnScreenPromptWidget* UTeachingPromptComponent::ResolvePromptWidget()
{
	if (CachedPromptWidget)
	{
		return CachedPromptWidget;
	}

	if (UWorld* World = GetWorld())
	{
		if (AKrowdKontrolPlayerController* Controller = Cast<AKrowdKontrolPlayerController>(World->GetFirstPlayerController()))
		{
			CachedPromptWidget = Controller->OnScreenPromptWidgetInstance;
		}
	}

	if (!CachedPromptWidget && !bHasWarnedMissingPromptWidget)
	{
		bHasWarnedMissingPromptWidget = true;
		UE_LOG(LogTemp, Warning,
			TEXT("UTeachingPromptComponent: no OnScreenPromptWidget available - the teaching prompt cannot be shown."));
	}

	return CachedPromptWidget;
}
