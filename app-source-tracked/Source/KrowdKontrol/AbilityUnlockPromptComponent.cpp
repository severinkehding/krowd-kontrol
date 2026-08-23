#include "AbilityUnlockPromptComponent.h"
#include "OnScreenPromptWidget.h"
#include "KrowdKontrolPlayerController.h"
#include "AbilityData.h"
#include "EnemyType.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

UAbilityUnlockPromptComponent::UAbilityUnlockPromptComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UAbilityUnlockPromptComponent::HandleAbilityUnlocked(EAbilitySlot Ability)
{
	UOnScreenPromptWidget* Widget = ResolvePromptWidget();
	if (!Widget)
	{
		PendingAbilities.Add(Ability);
		return;
	}

	ShowPromptForAbility(Widget, Ability);
}

void UAbilityUnlockPromptComponent::FlushPendingPrompts()
{
	if (PendingAbilities.IsEmpty())
	{
		return;
	}

	UOnScreenPromptWidget* Widget = ResolvePromptWidget();
	if (!Widget)
	{
		return;
	}

	const TArray<EAbilitySlot> AbilitiesToShow = MoveTemp(PendingAbilities);
	PendingAbilities.Reset();
	for (EAbilitySlot Ability : AbilitiesToShow)
	{
		ShowPromptForAbility(Widget, Ability);
	}
}

void UAbilityUnlockPromptComponent::ShowPromptForAbility(UOnScreenPromptWidget* Widget, EAbilitySlot Ability)
{
	const FString& AbilityDisplayName = AbilityData::GetDisplayName(Ability);
	// Assumes EAbilitySlot's declared order (Stun, Sleep, Root, Fear, Snare - see
	// AbilitySlot.h) is contiguous and locked, per that enum's own comment; the on-screen
	// key number is simply "declaration index + 1".
	const int32 KeyNumber = static_cast<int32>(Ability) + 1;
	const EEnemyType Countered = AbilityData::Get(Ability).CounteredEnemyType;
	const FString& EnemyPluralDisplayName = AbilityData::GetEnemyPluralDisplayName(Countered);

	const FString Message = FString::Printf(TEXT("%s — PRESS %d — STRONG VS %s"),
		*AbilityDisplayName, KeyNumber, *EnemyPluralDisplayName);
	Widget->ShowPrompt(FText::FromString(Message));
}

UOnScreenPromptWidget* UAbilityUnlockPromptComponent::ResolvePromptWidget()
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
			TEXT("UAbilityUnlockPromptComponent: no OnScreenPromptWidget available - the ability-unlock prompt cannot be shown."));
	}

	return CachedPromptWidget;
}
