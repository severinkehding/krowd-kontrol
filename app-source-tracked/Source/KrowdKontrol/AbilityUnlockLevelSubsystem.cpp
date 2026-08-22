#include "AbilityUnlockLevelSubsystem.h"
#include "LevelLifecycleSubsystem.h"
#include "AbilityUnlockComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"

void UAbilityUnlockLevelSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	// Force ULevelLifecycleSubsystem to construct/Initialize() before we look it up
	// below, mirroring UCrowdMasterySubsystem::Initialize()'s identical
	// InitializeDependency() call - FSubsystemCollectionBase does not otherwise
	// guarantee sibling construction order between Initialize() calls.
	Collection.InitializeDependency<ULevelLifecycleSubsystem>();
	if (UWorld* World = GetWorld())
	{
		if (ULevelLifecycleSubsystem* LifecycleSubsystem = World->GetSubsystem<ULevelLifecycleSubsystem>())
		{
			LifecycleSubsystem->OnLevelBegin.AddDynamic(this, &UAbilityUnlockLevelSubsystem::HandleLevelBegin);
		}
	}
}

void UAbilityUnlockLevelSubsystem::HandleLevelBegin(FName MapName)
{
	UWorld* World = GetWorld();
	const APlayerController* PlayerController = World ? World->GetFirstPlayerController() : nullptr;
	const APawn* Pawn = PlayerController ? PlayerController->GetPawn() : nullptr;
	UAbilityUnlockComponent* UnlockComponent = Pawn ? Pawn->FindComponentByClass<UAbilityUnlockComponent>() : nullptr;
	if (!UnlockComponent)
	{
		if (!bHasWarnedMissingAbilityUnlockComponent)
		{
			bHasWarnedMissingAbilityUnlockComponent = true;
			UE_LOG(LogTemp, Warning,
				TEXT("UAbilityUnlockLevelSubsystem: no possessed pawn with a UAbilityUnlockComponent found for map '%s' - ability unlocks will not progress."),
				*MapName.ToString());
		}
		return;
	}
	UnlockComponent->NotifyLevelReached(ParseLevelIndexFromMapName(MapName));
}

int32 UAbilityUnlockLevelSubsystem::ParseLevelIndexFromMapName(FName MapName)
{
	static const FString Prefix = TEXT("L_Level");
	const FString BareMapName = UWorld::RemovePIEPrefix(MapName.ToString());
	if (BareMapName.StartsWith(Prefix))
	{
		const FString Suffix = BareMapName.RightChop(Prefix.Len());
		if (Suffix.IsNumeric())
		{
			return FCString::Atoi(*Suffix);
		}
	}
	return 1;
}
