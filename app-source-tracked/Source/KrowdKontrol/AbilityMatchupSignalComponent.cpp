#include "AbilityMatchupSignalComponent.h"
#include "EnemyBase.h"
#include "EnemyTypeIndicatorComponent.h"
#include "AbilityData.h"

UAbilityMatchupSignalComponent::UAbilityMatchupSignalComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UAbilityMatchupSignalComponent::HandleAbilityCastApplied(EAbilitySlot Ability, AEnemyBase* TargetEnemy)
{
	if (!TargetEnemy)
	{
		return;
	}

	UEnemyTypeIndicatorComponent* Indicator = TargetEnemy->FindComponentByClass<UEnemyTypeIndicatorComponent>();
	if (!Indicator)
	{
		if (!bHasWarnedMissingEnemyTypeIndicator)
		{
			bHasWarnedMissingEnemyTypeIndicator = true;
			UE_LOG(LogTemp, Warning,
				TEXT("UAbilityMatchupSignalComponent: target '%s' has no UEnemyTypeIndicatorComponent - ")
				TEXT("cannot classify the ability matchup, no signal broadcast."),
				*GetNameSafe(TargetEnemy));
		}
		return;
	}

	const FAbilityData& Data = AbilityData::Get(Ability);
	const bool bWasColourMatched = !Data.bIsColourNeutral && Data.CounteredEnemyType == Indicator->EnemyType;

	OnAbilityMatchupSignal.Broadcast(Ability, TargetEnemy, bWasColourMatched);
}
