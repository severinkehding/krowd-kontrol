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

	UAbilityCooldownComponent* CooldownComponent = ResolvePassedCastGates(Ability, TEXT("TryCastAbility"));
	if (!CooldownComponent)
	{
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

int32 UAbilityCastComponent::TryCastThrownAbilityAtLocation(EAbilitySlot Ability, const FVector& DesiredTargetLocation)
{
	UE_LOG(LogTemp, Log, TEXT("UAbilityCastComponent::TryCastThrownAbilityAtLocation: entry, Ability=%s"),
		*UEnum::GetValueAsString(Ability));

	UAbilityCooldownComponent* CooldownComponent = ResolvePassedCastGates(Ability, TEXT("TryCastThrownAbilityAtLocation"));
	if (!CooldownComponent)
	{
		return -1;
	}

	const FVector ClampedLocation = GetClampedThrowLocation(Ability, DesiredTargetLocation);

	if (!CooldownComponent->TryStartCooldown(Ability))
	{
		UE_LOG(LogTemp, Log, TEXT("UAbilityCastComponent::TryCastThrownAbilityAtLocation: exit, Ability=%s TryStartCooldown failed unexpectedly"),
			*UEnum::GetValueAsString(Ability));
		return -1;
	}

	const float RadiusSquared = FMath::Square(ThrownCircleLandingRadiusUnits);
	const int32 AffectedCount = ApplyControlToEnemiesInShape(Ability, [&ClampedLocation, RadiusSquared](const FVector& EnemyLocation)
	{
		return FVector::DistSquared(EnemyLocation, ClampedLocation) <= RadiusSquared;
	});

	UE_LOG(LogTemp, Log, TEXT("UAbilityCastComponent::TryCastThrownAbilityAtLocation: exit, Ability=%s landed at clamped location, AffectedCount=%d"),
		*UEnum::GetValueAsString(Ability), AffectedCount);
	return AffectedCount;
}

FVector UAbilityCastComponent::ComputeClampedThrowLocation(const FVector& OwnerLocation, const FVector& DesiredTargetLocation, float MaxRangeUnits)
{
	const FVector Delta = DesiredTargetLocation - OwnerLocation;
	if (Delta.SizeSquared() <= FMath::Square(MaxRangeUnits))
	{
		return DesiredTargetLocation;
	}
	return OwnerLocation + Delta.GetSafeNormal() * MaxRangeUnits;
}

FVector UAbilityCastComponent::GetClampedThrowLocation(EAbilitySlot Ability, const FVector& DesiredTargetLocation) const
{
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return DesiredTargetLocation;
	}
	return ComputeClampedThrowLocation(
		Owner->GetActorLocation(), DesiredTargetLocation, GetThrowRangeUnitsForTier(AbilityData::Get(Ability).Range));
}

FVector UAbilityCastComponent::ComputeLineEndLocation(const FVector& OwnerLocation, const FVector& DesiredTargetLocation, float LineRangeUnits, const FVector& FallbackDirection)
{
	const FVector Delta = DesiredTargetLocation - OwnerLocation;
	// Same real-gameplay-units dead zone as ComputeFacingRotation (PR #279 review) -
	// KINDA_SMALL_NUMBER/GetSafeNormal's default tolerance is ~0.1mm, functionally no
	// guard against sub-pixel cursor noise near the owner's own location.
	constexpr float DirectionDeadZoneRadiusUnits = 10.0f;
	const FVector Direction = Delta.SizeSquared() > FMath::Square(DirectionDeadZoneRadiusUnits)
		? Delta.GetSafeNormal()
		: FallbackDirection.GetSafeNormal();
	return OwnerLocation + Direction * LineRangeUnits;
}

FVector UAbilityCastComponent::GetLineEndLocation(EAbilitySlot Ability, const FVector& DesiredTargetLocation) const
{
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return DesiredTargetLocation;
	}
	return ComputeLineEndLocation(
		Owner->GetActorLocation(), DesiredTargetLocation,
		GetThrowRangeUnitsForTier(AbilityData::Get(Ability).Range), Owner->GetActorForwardVector());
}

namespace
{
	// Pure point-to-segment distance-squared math - hand-rolled rather than a call
	// to an engine math utility this module has never depended on before, matching
	// this codebase's existing "small, directly-testable, hand-rolled vector math"
	// precedent (IntersectRayWithGroundPlane, ComputeFacingRotation).
	float ComputeDistanceSquaredFromSegment(const FVector& Point, const FVector& SegmentStart, const FVector& SegmentEnd)
	{
		const FVector SegmentVector = SegmentEnd - SegmentStart;
		const float SegmentLengthSquared = SegmentVector.SizeSquared();
		if (SegmentLengthSquared <= KINDA_SMALL_NUMBER)
		{
			return FVector::DistSquared(Point, SegmentStart);
		}
		const float T = FMath::Clamp(FVector::DotProduct(Point - SegmentStart, SegmentVector) / SegmentLengthSquared, 0.0f, 1.0f);
		const FVector ClosestPoint = SegmentStart + T * SegmentVector;
		return FVector::DistSquared(Point, ClosestPoint);
	}
}

int32 UAbilityCastComponent::TryCastLineAbilityTowardLocation(EAbilitySlot Ability, const FVector& DesiredTargetLocation)
{
	UE_LOG(LogTemp, Log, TEXT("UAbilityCastComponent::TryCastLineAbilityTowardLocation: entry, Ability=%s"),
		*UEnum::GetValueAsString(Ability));

	UAbilityCooldownComponent* CooldownComponent = ResolvePassedCastGates(Ability, TEXT("TryCastLineAbilityTowardLocation"));
	if (!CooldownComponent)
	{
		return -1;
	}

	const AActor* Owner = GetOwner(); // non-null - ResolvePassedCastGates already checked
	const FVector OwnerLocation = Owner->GetActorLocation();
	const FVector LineEnd = ComputeLineEndLocation(
		OwnerLocation, DesiredTargetLocation, GetThrowRangeUnitsForTier(AbilityData::Get(Ability).Range), Owner->GetActorForwardVector());

	if (!CooldownComponent->TryStartCooldown(Ability))
	{
		UE_LOG(LogTemp, Log, TEXT("UAbilityCastComponent::TryCastLineAbilityTowardLocation: exit, Ability=%s TryStartCooldown failed unexpectedly"),
			*UEnum::GetValueAsString(Ability));
		return -1;
	}

	const float WidthSquared = FMath::Square(LineHitWidthUnits);
	const int32 AffectedCount = ApplyControlToEnemiesInShape(Ability, [&OwnerLocation, &LineEnd, WidthSquared](const FVector& EnemyLocation)
	{
		return ComputeDistanceSquaredFromSegment(EnemyLocation, OwnerLocation, LineEnd) <= WidthSquared;
	});

	UE_LOG(LogTemp, Log, TEXT("UAbilityCastComponent::TryCastLineAbilityTowardLocation: exit, Ability=%s AffectedCount=%d"),
		*UEnum::GetValueAsString(Ability), AffectedCount);
	return AffectedCount;
}

FVector UAbilityCastComponent::ComputeConeDirection(const FVector& OwnerLocation, const FVector& DesiredTargetLocation, const FVector& FallbackDirection)
{
	const FVector Delta = DesiredTargetLocation - OwnerLocation;
	// Same real-gameplay-units dead zone as ComputeLineEndLocation/ComputeFacingRotation
	// (PR #279 review) - KINDA_SMALL_NUMBER/GetSafeNormal's default tolerance is
	// ~0.1mm, functionally no guard against sub-pixel cursor noise near the owner.
	constexpr float DirectionDeadZoneRadiusUnits = 10.0f;
	return Delta.SizeSquared() > FMath::Square(DirectionDeadZoneRadiusUnits)
		? Delta.GetSafeNormal()
		: FallbackDirection.GetSafeNormal();
}

bool UAbilityCastComponent::IsPointInCone(const FVector& Point, const FVector& ApexLocation, const FVector& ConeDirection, float HalfAngleDegrees, float RangeUnits)
{
	const FVector ToPoint = Point - ApexLocation;
	if (ToPoint.SizeSquared2D() > FMath::Square(RangeUnits))
	{
		return false;
	}
	// 2D (X/Y) angle test, matching this codebase's flat-top-down convention (see
	// ComputeFacingRotation's own SizeSquared2D usage) - a Point exactly at ApexLocation
	// normalizes to a zero vector, whose dot product with anything is 0, which is < any
	// realistic CosHalfAngle (cos of half of a sub-360 angle is > 0) - so it's excluded,
	// a deliberate degenerate-case decision, not an oversight.
	const FVector ToPointDirection2D = FVector(ToPoint.X, ToPoint.Y, 0.0f).GetSafeNormal();
	const FVector ConeDirection2D = FVector(ConeDirection.X, ConeDirection.Y, 0.0f).GetSafeNormal();
	const float CosHalfAngle = FMath::Cos(FMath::DegreesToRadians(HalfAngleDegrees));
	return FVector::DotProduct(ToPointDirection2D, ConeDirection2D) >= CosHalfAngle;
}

FVector UAbilityCastComponent::GetConeDirection(const FVector& DesiredTargetLocation) const
{
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return DesiredTargetLocation;
	}
	return ComputeConeDirection(Owner->GetActorLocation(), DesiredTargetLocation, Owner->GetActorForwardVector());
}

float UAbilityCastComponent::GetConeRangeUnits(EAbilitySlot Ability) const
{
	return GetThrowRangeUnitsForTier(AbilityData::Get(Ability).Range);
}

int32 UAbilityCastComponent::TryCastConeAbilityTowardLocation(EAbilitySlot Ability, const FVector& DesiredTargetLocation)
{
	UE_LOG(LogTemp, Log, TEXT("UAbilityCastComponent::TryCastConeAbilityTowardLocation: entry, Ability=%s"),
		*UEnum::GetValueAsString(Ability));

	UAbilityCooldownComponent* CooldownComponent = ResolvePassedCastGates(Ability, TEXT("TryCastConeAbilityTowardLocation"));
	if (!CooldownComponent)
	{
		return -1;
	}

	const AActor* Owner = GetOwner(); // non-null - ResolvePassedCastGates already checked
	const FVector OwnerLocation = Owner->GetActorLocation();
	const FVector ConeDirection = ComputeConeDirection(OwnerLocation, DesiredTargetLocation, Owner->GetActorForwardVector());
	const float RangeUnits = GetThrowRangeUnitsForTier(AbilityData::Get(Ability).Range);
	const float HalfAngleDegrees = ConeFullAngleDegrees / 2.0f;

	if (!CooldownComponent->TryStartCooldown(Ability))
	{
		UE_LOG(LogTemp, Log, TEXT("UAbilityCastComponent::TryCastConeAbilityTowardLocation: exit, Ability=%s TryStartCooldown failed unexpectedly"),
			*UEnum::GetValueAsString(Ability));
		return -1;
	}

	const int32 AffectedCount = ApplyControlToEnemiesInShape(Ability, [&OwnerLocation, &ConeDirection, HalfAngleDegrees, RangeUnits](const FVector& EnemyLocation)
	{
		return IsPointInCone(EnemyLocation, OwnerLocation, ConeDirection, HalfAngleDegrees, RangeUnits);
	});

	UE_LOG(LogTemp, Log, TEXT("UAbilityCastComponent::TryCastConeAbilityTowardLocation: exit, Ability=%s AffectedCount=%d"),
		*UEnum::GetValueAsString(Ability), AffectedCount);
	return AffectedCount;
}

int32 UAbilityCastComponent::TryCastSelfCircleAbility(EAbilitySlot Ability)
{
	UE_LOG(LogTemp, Log, TEXT("UAbilityCastComponent::TryCastSelfCircleAbility: entry, Ability=%s"),
		*UEnum::GetValueAsString(Ability));

	UAbilityCooldownComponent* CooldownComponent = ResolvePassedCastGates(Ability, TEXT("TryCastSelfCircleAbility"));
	if (!CooldownComponent)
	{
		return -1;
	}

	const AActor* Owner = GetOwner(); // non-null - ResolvePassedCastGates already checked
	const FVector OwnerLocation = Owner->GetActorLocation();

	if (!CooldownComponent->TryStartCooldown(Ability))
	{
		UE_LOG(LogTemp, Log, TEXT("UAbilityCastComponent::TryCastSelfCircleAbility: exit, Ability=%s TryStartCooldown failed unexpectedly"),
			*UEnum::GetValueAsString(Ability));
		return -1;
	}

	const float RadiusSquared = FMath::Square(SelfCircleRadiusUnits);
	const int32 AffectedCount = ApplyControlToEnemiesInShape(Ability, [&OwnerLocation, RadiusSquared](const FVector& EnemyLocation)
	{
		return FVector::DistSquared(EnemyLocation, OwnerLocation) <= RadiusSquared;
	});

	UE_LOG(LogTemp, Log, TEXT("UAbilityCastComponent::TryCastSelfCircleAbility: exit, Ability=%s AffectedCount=%d"),
		*UEnum::GetValueAsString(Ability), AffectedCount);
	return AffectedCount;
}

UAbilityCooldownComponent* UAbilityCastComponent::ResolvePassedCastGates(EAbilitySlot Ability, const TCHAR* CallerLogContext) const
{
	// Issue #246: the pre-level briefing card pauses the world while shown, but
	// APlayerController ticks/processes input even while paused (engine default, so
	// pause menus stay interactive) - so the briefing's EKeys::AnyKey dismiss-bind
	// and an ability-cast key can both fire from the same keypress without this gate.
	if (const UWorld* World = GetWorld())
	{
		if (World->IsPaused())
		{
			UE_LOG(LogTemp, Log, TEXT("UAbilityCastComponent::%s: exit, Ability=%s world is paused (briefing/safe-state active)"),
				CallerLogContext, *UEnum::GetValueAsString(Ability));
			return nullptr;
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
				UE_LOG(LogTemp, Log, TEXT("UAbilityCastComponent::%s: exit, Ability=%s briefing card still visible"),
					CallerLogContext, *UEnum::GetValueAsString(Ability));
				return nullptr;
			}
		}
	}

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		UE_LOG(LogTemp, Log, TEXT("UAbilityCastComponent::%s: exit, no Owner"), CallerLogContext);
		return nullptr;
	}

	UAbilityUnlockComponent* UnlockComponent = Owner->FindComponentByClass<UAbilityUnlockComponent>();
	if (!UnlockComponent || !UnlockComponent->IsAbilityUnlocked(Ability))
	{
		UE_LOG(LogTemp, Log, TEXT("UAbilityCastComponent::%s: exit, Ability=%s not unlocked"),
			CallerLogContext, *UEnum::GetValueAsString(Ability));
		return nullptr;
	}

	UAbilityCooldownComponent* CooldownComponent = Owner->FindComponentByClass<UAbilityCooldownComponent>();
	if (!CooldownComponent || CooldownComponent->IsOnCooldown(Ability))
	{
		UE_LOG(LogTemp, Log, TEXT("UAbilityCastComponent::%s: exit, Ability=%s on cooldown"),
			CallerLogContext, *UEnum::GetValueAsString(Ability));
		return nullptr;
	}

	// Unlike Unlock/Cooldown above, a missing UAbilityLockoutComponent is NOT a gate
	// failure - it's optional (many existing tests construct a bare
	// UAbilityCastComponent with no lockout component attached at all). Presence-and-
	// locked blocks; absence does not.
	UAbilityLockoutComponent* LockoutComponent = Owner->FindComponentByClass<UAbilityLockoutComponent>();
	if (LockoutComponent && LockoutComponent->IsAbilityLocked(Ability))
	{
		UE_LOG(LogTemp, Log, TEXT("UAbilityCastComponent::%s: exit, Ability=%s locked out (punishment)"),
			CallerLogContext, *UEnum::GetValueAsString(Ability));
		return nullptr;
	}

	return CooldownComponent;
}

float UAbilityCastComponent::GetThrowRangeUnitsForTier(EAbilityRange Range) const
{
	switch (Range)
	{
	case EAbilityRange::Short:  return ShortThrowRangeUnits;
	case EAbilityRange::Medium: return MediumThrowRangeUnits;
	case EAbilityRange::Long:   return LongThrowRangeUnits;
	default: checkNoEntry(); return ShortThrowRangeUnits;
	}
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

int32 UAbilityCastComponent::ApplyControlToEnemiesInShape(EAbilitySlot Ability, TFunctionRef<bool(const FVector& EnemyLocation)> IsEnemyInShape)
{
	int32 AffectedCount = 0;
	for (TActorIterator<AEnemyBase> It(GetWorld()); It; ++It)
	{
		AEnemyBase* Enemy = *It;
		if (!IsEnemyInShape(Enemy->GetActorLocation()))
		{
			continue;
		}
		const bool bWasFreshlyTargetable = (Enemy->GetEnemyState() == EEnemyState::Alert || Enemy->GetEnemyState() == EEnemyState::Attack);
		Enemy->ReceiveControl(Ability);
		if (bWasFreshlyTargetable)
		{
			++AffectedCount;
			OnAbilityCastApplied.Broadcast(Ability, Enemy);
		}
	}
	return AffectedCount;
}
