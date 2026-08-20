#include "AbilityCastComponent.h"
#include "AbilityUnlockComponent.h"
#include "AbilityCooldownComponent.h"
#include "AbilityLockoutComponent.h"
#include "EnemyBase.h"
#include "EngineUtils.h"

UAbilityCastComponent::UAbilityCastComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UAbilityCastComponent::TryCastAbility(EAbilitySlot Ability)
{
	// Entry/exit logging so a live-PIE pass can tell "input never reached this
	// function" apart from "reached it but was gated" - see issue #138 E2E follow-up.
	UE_LOG(LogTemp, Log, TEXT("UAbilityCastComponent::TryCastAbility: entry, Ability=%s"),
		*UEnum::GetValueAsString(Ability));

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
