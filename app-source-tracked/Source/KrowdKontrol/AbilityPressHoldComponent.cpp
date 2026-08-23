#include "AbilityPressHoldComponent.h"
#include "AbilityCastComponent.h"
#include "AbilityTargetingIndicatorComponent.h"
#include "AbilityData.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

UAbilityPressHoldComponent::UAbilityPressHoldComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	bAbilityKeyHeld.Init(false, NumAbilitySlots);
	bAbilityHoldPreviewActive.Init(false, NumAbilitySlots);
	PressFlashTimerHandles.SetNum(NumAbilitySlots);
	HoldThresholdTimerHandles.SetNum(NumAbilitySlots);
}

void UAbilityPressHoldComponent::HandleAbilityKeyPressed(EAbilitySlot Ability, bool bHasCursorTargetLocation, FVector CursorTargetLocation)
{
	const int32 Index = static_cast<int32>(Ability);
	if (!bAbilityKeyHeld.IsValidIndex(Index))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UAbilityPressHoldComponent::HandleAbilityKeyPressed: index %d invalid on '%s'."),
			Index, *GetNameSafe(this));
		return;
	}

	FAbilityIndicatorShapeSpec ShapeSpec;
	const EAbilityTargetType TargetType = AbilityData::Get(Ability).TargetType;
	if (bHasCursorTargetLocation && TargetType == EAbilityTargetType::Line)
	{
		ShapeSpec.Kind = EAbilityIndicatorShapeKind::Line;
		FVector OwnerLocation = FVector::ZeroVector;
		if (AActor* Owner = GetOwner())
		{
			OwnerLocation = Owner->GetActorLocation();
		}
		else
		{
			UE_LOG(LogTemp, Warning,
				TEXT("UAbilityPressHoldComponent::HandleAbilityKeyPressed: no Owner on '%s' - Line indicator will show at world origin."),
				*GetNameSafe(this));
		}
		ShapeSpec.Origin = OwnerLocation;
		const FVector LineEnd = (CastComponent ? CastComponent->GetLineEndLocation(Ability, CursorTargetLocation) : CursorTargetLocation);
		// Degenerate only if LineEnd == OwnerLocation, which can only happen if
		// ComputeLineEndLocation's FallbackDirection (the owner's forward vector) is
		// itself zero - not expected for a spawned actor with a valid rotation.
		ShapeSpec.FacingRotation = (LineEnd - OwnerLocation).Rotation();
		ShapeSpec.RangeUnits = FVector::Dist(OwnerLocation, LineEnd);
	}
	else if (bHasCursorTargetLocation && TargetType == EAbilityTargetType::Cone)
	{
		ShapeSpec.Kind = EAbilityIndicatorShapeKind::Cone;
		FVector OwnerLocation = FVector::ZeroVector;
		if (AActor* Owner = GetOwner())
		{
			OwnerLocation = Owner->GetActorLocation();
		}
		else
		{
			UE_LOG(LogTemp, Warning,
				TEXT("UAbilityPressHoldComponent::HandleAbilityKeyPressed: no Owner on '%s' - Cone indicator will show at world origin."),
				*GetNameSafe(this));
		}
		ShapeSpec.Origin = OwnerLocation;
		const FVector ConeDirection = (CastComponent ? CastComponent->GetConeDirection(CursorTargetLocation) : (CursorTargetLocation - OwnerLocation));
		ShapeSpec.FacingRotation = ConeDirection.Rotation();
		ShapeSpec.RangeUnits = (CastComponent ? CastComponent->GetConeRangeUnits(Ability) : 1200.0f);
		ShapeSpec.ConeFullAngleDegrees = (CastComponent ? CastComponent->ConeFullAngleDegrees : 75.0f);
	}
	else if (bHasCursorTargetLocation)
	{
		ShapeSpec.Kind = EAbilityIndicatorShapeKind::CircleAtCursor;
		// Clamped through the same GetClampedThrowLocation the actual cast below uses,
		// so the preview circle never shows a landing point beyond where the throw can
		// really reach (issue #257 pass-1 code review finding).
		ShapeSpec.Origin = (CastComponent ? CastComponent->GetClampedThrowLocation(Ability, CursorTargetLocation) : CursorTargetLocation);
		ShapeSpec.RangeUnits = (CastComponent ? CastComponent->ThrownCircleLandingRadiusUnits : 400.0f);
	}
	else
	{
		ShapeSpec.Kind = EAbilityIndicatorShapeKind::CircleAtActor;
		if (AActor* Owner = GetOwner())
		{
			ShapeSpec.Origin = Owner->GetActorLocation();
		}
		else
		{
			UE_LOG(LogTemp, Warning,
				TEXT("UAbilityPressHoldComponent::HandleAbilityKeyPressed: no Owner on '%s' - indicator will show at world origin."),
				*GetNameSafe(this));
		}
		ShapeSpec.RangeUnits = (CastComponent ? CastComponent->CastRangeUnits : 300.0f);
	}

	// Show(), not Flash() - see the class-level GOTCHA in AbilityPressHoldComponent.h /
	// this function's own comment below for why.
	if (IndicatorComponent)
	{
		IndicatorComponent->Show(ShapeSpec, AbilityData::Get(Ability).Colour);
	}

	// Existing cast logic, called unconditionally on every press, exactly as
	// CastStunAbility() etc. did before this component existed - now branching to the
	// cursor-aimed thrown-ability path when a target location was supplied (issue #257),
	// and further to the cursor-aimed line-cast path for Line-target abilities (Root,
	// issue #255).
	if (CastComponent)
	{
		if (bHasCursorTargetLocation && TargetType == EAbilityTargetType::Line)
		{
			CastComponent->TryCastLineAbilityTowardLocation(Ability, CursorTargetLocation);
		}
		else if (bHasCursorTargetLocation && TargetType == EAbilityTargetType::Cone)
		{
			CastComponent->TryCastConeAbilityTowardLocation(Ability, CursorTargetLocation);
		}
		else if (bHasCursorTargetLocation)
		{
			CastComponent->TryCastThrownAbilityAtLocation(Ability, CursorTargetLocation);
		}
		else
		{
			CastComponent->TryCastAbility(Ability);
		}
	}

	bAbilityKeyHeld[Index] = true;
	bAbilityHoldPreviewActive[Index] = false;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			PressFlashTimerHandles[Index],
			FTimerDelegate::CreateUObject(this, &UAbilityPressHoldComponent::HandlePressFlashComplete, Ability),
			PressFlashDurationSeconds,
			false);

		World->GetTimerManager().SetTimer(
			HoldThresholdTimerHandles[Index],
			FTimerDelegate::CreateUObject(this, &UAbilityPressHoldComponent::BeginHoldPreview, Ability),
			HoldThresholdSeconds,
			false);
	}
}

void UAbilityPressHoldComponent::HandleAbilityKeyReleased(EAbilitySlot Ability)
{
	const int32 Index = static_cast<int32>(Ability);
	if (!bAbilityKeyHeld.IsValidIndex(Index))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UAbilityPressHoldComponent::HandleAbilityKeyReleased: index %d invalid on '%s'."),
			Index, *GetNameSafe(this));
		return;
	}

	bAbilityKeyHeld[Index] = false;

	if (UWorld* World = GetWorld())
	{
		// Prevents a late BeginHoldPreview from firing after release.
		World->GetTimerManager().ClearTimer(HoldThresholdTimerHandles[Index]);

		if (bAbilityHoldPreviewActive[Index])
		{
			World->GetTimerManager().ClearTimer(PressFlashTimerHandles[Index]);
		}
	}

	if (bAbilityHoldPreviewActive[Index])
	{
		if (IndicatorComponent)
		{
			IndicatorComponent->Hide();
		}
		bAbilityHoldPreviewActive[Index] = false;
	}
	// Else: still just a press-flash in flight (or already auto-hidden) - leave
	// PressFlashTimerHandles[Index] running so a fast tap still gets its full
	// PressFlashDurationSeconds of visible flash regardless of exactly when the key
	// physically came up, matching UAbilityTargetingIndicatorComponent::Flash()'s own
	// fixed-duration-independent-of-input semantics.
}

void UAbilityPressHoldComponent::HandlePressFlashComplete(EAbilitySlot Ability)
{
	const int32 Index = static_cast<int32>(Ability);
	if (!bAbilityHoldPreviewActive.IsValidIndex(Index))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UAbilityPressHoldComponent::HandlePressFlashComplete: index %d invalid on '%s'."),
			Index, *GetNameSafe(this));
		return;
	}

	// No-op if hold-preview already took over - the GOTCHA below is why this component
	// owns this timer locally instead of calling Indicator->Flash().
	if (!bAbilityHoldPreviewActive[Index] && IndicatorComponent)
	{
		IndicatorComponent->Hide();
	}
}

void UAbilityPressHoldComponent::BeginHoldPreview(EAbilitySlot Ability)
{
	const int32 Index = static_cast<int32>(Ability);
	if (!bAbilityKeyHeld.IsValidIndex(Index))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UAbilityPressHoldComponent::BeginHoldPreview: index %d invalid on '%s'."),
			Index, *GetNameSafe(this));
		return;
	}

	// Indicator is already visible (shown at press time via Show()), so no further
	// Show()/Flash() call is needed here - this just flips the flag HandlePressFlashComplete
	// and HandleAbilityKeyReleased both check.
	//
	// GOTCHA (the reason HandleAbilityKeyPressed calls Show() instead of
	// Indicator->Flash()): UAbilityTargetingIndicatorComponent::Flash() internally calls
	// Show() *and* arms its own FlashTimerHandle to auto-Hide() after its own duration
	// parameter. If this component called Flash() at press-time and then later tried to
	// keep the indicator visible for a hold-preview, the indicator's own internal
	// FlashTimerHandle would still fire and hide it out from under the hold-preview,
	// since Show() never clears a previously-armed FlashTimerHandle. Calling
	// IndicatorComponent->Show() directly (never Flash()) and owning both "when to hide"
	// timers locally in this component is what avoids that double-timer conflict - do
	// not "simplify" this by switching back to Flash().
	if (bAbilityKeyHeld[Index])
	{
		bAbilityHoldPreviewActive[Index] = true;
	}
}

void UAbilityPressHoldComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		FTimerManager& TimerManager = World->GetTimerManager();
		for (int32 Index = 0; Index < NumAbilitySlots; ++Index)
		{
			TimerManager.ClearTimer(PressFlashTimerHandles[Index]);
			TimerManager.ClearTimer(HoldThresholdTimerHandles[Index]);
		}
	}
	Super::EndPlay(EndPlayReason);
}
