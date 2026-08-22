#include "AbilityCastComponent.h"
#include "AbilityUnlockComponent.h"
#include "AbilityCooldownComponent.h"
#include "AbilityLockoutComponent.h"
#include "EnemyBase.h"
#include "CrowdMasterySubsystem.h"
#include "KrowdKontrolPlayerController.h"
#include "BriefingCardWidget.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"

UAbilityCastComponent::UAbilityCastComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UAbilityCastComponent::BeginPlay()
{
	Super::BeginPlay();

	// Issue #174 AC1: routes every real successful cast into the Crowd Mastery
	// running-max sample, the same production wiring UCrowdMasterySubsystem.h's
	// class comment already flagged as the missing piece.
	if (UWorld* World = GetWorld())
	{
		if (UCrowdMasterySubsystem* CrowdMasterySubsystem = World->GetSubsystem<UCrowdMasterySubsystem>())
		{
			OnAbilityCastApplied.AddDynamic(CrowdMasterySubsystem, &UCrowdMasterySubsystem::HandleAbilityCastApplied);
		}
	}
}

bool UAbilityCastComponent::TryCastAbility(EAbilitySlot Ability)
{
	// Entry/exit logging so a live-PIE pass can tell "input never reached this
	// function" apart from "reached it but was gated" - see issue #138 E2E follow-up.
	UE_LOG(LogTemp, Log, TEXT("UAbilityCastComponent::TryCastAbility: entry, Ability=%s"),
		*UEnum::GetValueAsString(Ability));

	// Issue #246: the pre-level briefing card pauses the world while shown, but
	// APlayerController ticks/processes input even while paused (engine default, so
	// pause menus stay interactive) - so the briefing's EKeys::AnyKey dismiss-bind
	// and an ability-cast key can both fire from the same keypress without this gate.
	if (const UWorld* World = GetWorld())
	{
		if (World->IsPaused())
		{
			UE_LOG(LogTemp, Log, TEXT("UAbilityCastComponent::TryCastAbility: exit, Ability=%s world is paused (briefing/safe-state active)"),
				*UEnum::GetValueAsString(Ability));
			return false;
		}
	}

	// Belt-and-suspenders for the same-keypress race the comment above describes: the
	// dismiss bind lives on the controller's InputComponent, this cast bind lives on
	// the pawn's, so whether World->IsPaused() above still reads true for this event
	// depends on UE5's per-frame input-stack processing order between the two -
	// unpinned by this codebase. Checking the widget's own IsBriefingVisible() state
	// directly, via the owning pawn's controller, closes that gap regardless of
	// ordering: DismissBriefing() flips this to false as part of the exact same call
	// that unpauses the world, so the two checks can never disagree with each other.
	if (const APawn* OwnerPawn = Cast<APawn>(GetOwner()))
	{
		if (const AKrowdKontrolPlayerController* Controller = Cast<AKrowdKontrolPlayerController>(OwnerPawn->GetController()))
		{
			if (Controller->BriefingCardWidgetInstance && Controller->BriefingCardWidgetInstance->IsBriefingVisible())
			{
				UE_LOG(LogTemp, Log, TEXT("UAbilityCastComponent::TryCastAbility: exit, Ability=%s briefing card still visible"),
					*UEnum::GetValueAsString(Ability));
				return false;
			}
		}
	}

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		UE_LOG(LogTemp, Log, TEXT("UAbilityCastComponent::TryCastAbility: exit, no Owner"));
		return false;
	}

	UAbilityUnlockComponent* UnlockComponent = Owner->FindComponentByClass<UAbilityUnlockComponent>();
	if (!UnlockComponent || !UnlockComponent->IsAbilityUnlocked(Ability))
	{
		UE_LOG(LogTemp, Log, TEXT("UAbilityCastComponent::TryCastAbility: exit, Ability=%s not unlocked"),
			*UEnum::GetValueAsString(Ability));
		return false;
	}

	UAbilityCooldownComponent* CooldownComponent = Owner->FindComponentByClass<UAbilityCooldownComponent>();
	if (!CooldownComponent || CooldownComponent->IsOnCooldown(Ability))
	{
		UE_LOG(LogTemp, Log, TEXT("UAbilityCastComponent::TryCastAbility: exit, Ability=%s on cooldown"),
			*UEnum::GetValueAsString(Ability));
		return false;
	}

	// Unlike Unlock/Cooldown above, a missing UAbilityLockoutComponent is NOT a gate
	// failure - it's optional (many existing tests construct a bare
	// UAbilityCastComponent with no lockout component attached at all). Presence-and-
	// locked blocks; absence does not.
	UAbilityLockoutComponent* LockoutComponent = Owner->FindComponentByClass<UAbilityLockoutComponent>();
	if (LockoutComponent && LockoutComponent->IsAbilityLocked(Ability))
	{
		UE_LOG(LogTemp, Log, TEXT("UAbilityCastComponent::TryCastAbility: exit, Ability=%s locked out (punishment)"),
			*UEnum::GetValueAsString(Ability));
		return false;
	}

	AEnemyBase* Target = FindNearestValidTarget();
	if (!Target)
	{
		UE_LOG(LogTemp, Log, TEXT("UAbilityCastComponent::TryCastAbility: exit, Ability=%s no eligible target in range"),
			*UEnum::GetValueAsString(Ability));
		return false;
	}

	if (!CooldownComponent->TryStartCooldown(Ability))
	{
		// Defensive only - IsOnCooldown() was already checked above and nothing else
		// can start a cooldown between the two calls in this single-threaded path.
		UE_LOG(LogTemp, Log, TEXT("UAbilityCastComponent::TryCastAbility: exit, Ability=%s TryStartCooldown failed unexpectedly"),
			*UEnum::GetValueAsString(Ability));
		return false;
	}

	Target->ReceiveControl(Ability);
	OnAbilityCastApplied.Broadcast(Ability, Target);
	UE_LOG(LogTemp, Log, TEXT("UAbilityCastComponent::TryCastAbility: exit, Ability=%s applied to Target=%s"),
		*UEnum::GetValueAsString(Ability), *GetNameSafe(Target));
	return true;
}

AEnemyBase* UAbilityCastComponent::FindNearestValidTarget() const
{
	const AActor* Owner = GetOwner();
	if (!Owner || !GetWorld())
	{
		return nullptr;
	}

	const FVector OwnerLocation = Owner->GetActorLocation();
	const float RangeSquared = FMath::Square(CastRangeUnits);

	AEnemyBase* Nearest = nullptr;
	float NearestDistSquared = TNumericLimits<float>::Max();
	for (TActorIterator<AEnemyBase> It(GetWorld()); It; ++It)
	{
		const EEnemyState State = It->GetEnemyState();
		if (State != EEnemyState::Alert && State != EEnemyState::Attack)
		{
			continue;
		}
		const float DistSquared = FVector::DistSquared(It->GetActorLocation(), OwnerLocation);
		if (DistSquared > RangeSquared || DistSquared >= NearestDistSquared)
		{
			continue;
		}
		Nearest = *It;
		NearestDistSquared = DistSquared;
	}
	return Nearest;
}
