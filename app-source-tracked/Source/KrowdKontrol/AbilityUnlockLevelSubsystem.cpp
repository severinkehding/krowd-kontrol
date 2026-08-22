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
	const int32 LevelIndex = ParseLevelIndexFromMapName(MapName);
	if (!UnlockComponent)
	{
		// AutoPossessPlayer's timing relative to OnLevelBegin isn't guaranteed (same
		// hazard AKrowdKontrolPlayerController::BeginPlay() already documents for
		// itself) - remember this level's index so RetryPendingUnlockForPawn() can
		// still deliver it once a pawn is possessed, since OnLevelBegin only fires once.
		bLevelBeginFiredWithNoPawn = true;
		PendingLevelIndex = LevelIndex;
		if (!bHasWarnedMissingAbilityUnlockComponent)
		{
			bHasWarnedMissingAbilityUnlockComponent = true;
			UE_LOG(LogTemp, Warning,
				TEXT("UAbilityUnlockLevelSubsystem: no possessed pawn with a UAbilityUnlockComponent found for map '%s' - ability unlocks will not progress."),
				*MapName.ToString());
		}
		return;
	}
	UnlockComponent->NotifyLevelReached(LevelIndex);
}

void UAbilityUnlockLevelSubsystem::RetryPendingUnlockForPawn(APawn* PossessedPawn)
{
	if (!bLevelBeginFiredWithNoPawn)
	{
		return;
	}
	UAbilityUnlockComponent* UnlockComponent = PossessedPawn ? PossessedPawn->FindComponentByClass<UAbilityUnlockComponent>() : nullptr;
	if (!UnlockComponent)
	{
		return;
	}
	bLevelBeginFiredWithNoPawn = false;
	UnlockComponent->NotifyLevelReached(PendingLevelIndex);
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
