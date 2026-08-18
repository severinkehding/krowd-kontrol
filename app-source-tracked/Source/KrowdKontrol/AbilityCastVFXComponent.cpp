#include "AbilityCastVFXComponent.h"
#include "Components/PointLightComponent.h"
#include "AbilityData.h"
#include "EnemyBase.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "GameFramework/Actor.h"

UAbilityCastVFXComponent::UAbilityCastVFXComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UAbilityCastVFXComponent::BeginPlay()
{
	Super::BeginPlay();
	InitializeCastVFX();
}

void UAbilityCastVFXComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CastFlashTimerHandle);
	}
	Super::EndPlay(EndPlayReason);
}

void UAbilityCastVFXComponent::InitializeCastVFX()
{
	if (bHasInitializedCastVFX)
	{
		return;
	}

	AActor* Owner = GetOwner();
	if (!Owner || !Owner->GetRootComponent())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UAbilityCastVFXComponent: InitializeCastVFX on '%s' found no Owner root component - cast flash not created."),
			*GetNameSafe(Owner));
		// bHasInitializedCastVFX deliberately NOT set here - retryable, same as
		// UEnemyTypeIndicatorComponent::InitializeMarkerVisual().
		return;
	}
	bHasInitializedCastVFX = true;

	// Deliberately not attached to Owner's root component: HandleAbilityCastApplied
	// always repositions this light explicitly with SetWorldLocation() at the cast
	// target, and it must stay world-fixed there even if the caster (player pawn)
	// moves during the flash window - attaching it would drag the flash along with
	// the player instead (PR #155 review finding).
	CastFlashLightComponent = NewObject<UPointLightComponent>(Owner, TEXT("AbilityCastFlashLightComponent"));
	CastFlashLightComponent->RegisterComponent();
	CastFlashLightComponent->SetIntensity(0.0f); // off until a cast lands
	CastFlashLightComponent->SetAttenuationRadius(CastFlashAttenuationRadius);
}

void UAbilityCastVFXComponent::HandleAbilityCastApplied(EAbilitySlot Ability, AEnemyBase* TargetEnemy)
{
	if (!CastFlashLightComponent)
	{
		// InitializeCastVFX() hasn't succeeded yet (no root component, or BeginPlay
		// hasn't run) - nothing to flash.
		return;
	}

	// AbilityData is the single source of truth for an ability's locked colour
	// (MISSION.md Hard Invariant 3) - never a local FLinearColor literal here.
	CastFlashLightComponent->SetLightColor(AbilityData::Get(Ability).Colour);

	if (TargetEnemy)
	{
		CastFlashLightComponent->SetWorldLocation(TargetEnemy->GetActorLocation());
	}

	CastFlashLightComponent->SetIntensity(CastFlashIntensity);

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			CastFlashTimerHandle, this, &UAbilityCastVFXComponent::ClearCastFlash, CastFlashDurationSeconds, false);
	}
}

void UAbilityCastVFXComponent::ClearCastFlash()
{
	if (CastFlashLightComponent)
	{
		CastFlashLightComponent->SetIntensity(0.0f);
	}
}
