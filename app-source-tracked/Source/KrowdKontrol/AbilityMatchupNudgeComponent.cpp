#include "AbilityMatchupNudgeComponent.h"
#include "OnScreenPromptWidget.h"
#include "KrowdKontrolPlayerController.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

UAbilityMatchupNudgeComponent::UAbilityMatchupNudgeComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UAbilityMatchupNudgeComponent::HandleAbilityMatchupSignal(EAbilitySlot Ability, AEnemyBase* TargetEnemy, bool bWasColourMatched)
{
	if (bHasShownNudge)
	{
		return;
	}

	if (bWasColourMatched)
	{
		ConsecutiveNonMatchedCasts = 0;
		return;
	}

	++ConsecutiveNonMatchedCasts;
	if (ConsecutiveNonMatchedCasts < NonMatchedCastThreshold)
	{
		return;
	}

	// Set before attempting to resolve/show the widget, not after - mirrors
	// UFirstStunBeaconComponent's set-before-attempt precedent so a missing widget
	// doesn't leave this retryable forever.
	bHasShownNudge = true;

	UOnScreenPromptWidget* Widget = ResolvePromptWidget();
	if (!Widget)
	{
		return;
	}

	const FText NudgeMessage = NSLOCTEXT("AbilityMatchupNudgeComponent", "AdditionalHelpNudge", "Try matching the ability colour to the enemy.");
	Widget->ShowPrompt(NudgeMessage);
}

UOnScreenPromptWidget* UAbilityMatchupNudgeComponent::ResolvePromptWidget()
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
			TEXT("UAbilityMatchupNudgeComponent: no OnScreenPromptWidget available - the additional-help nudge cannot be shown."));
	}

	return CachedPromptWidget;
}
