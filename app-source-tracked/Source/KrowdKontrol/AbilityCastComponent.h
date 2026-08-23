#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AbilitySlot.h"
#include "AbilityData.h"
#include "AbilityCastComponent.generated.h"

class AEnemyBase;
class UAbilityCooldownComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAbilityCastApplied, EAbilitySlot, Ability, AEnemyBase*, TargetEnemy);

// The single production entry point that finally calls AEnemyBase::ReceiveControl()
// (issue #12's hook, previously called only from tests) - see MISSION.md, PRD 02, and
// the at-least-8 previously-rejected issues (#65/#67/#59/#48/#50/#52/#121/#29) blocked
// on this exact gap. TryCastAbility(EAbilitySlot) gates the attempt through
// UAbilityUnlockComponent::IsAbilityUnlocked (locked abilities can't cast) and
// UAbilityCooldownComponent::TryStartCooldown (the documented, sole legal cast-gating
// point - see AbilityCooldownComponent.h), does minimal automatic targeting (nearest
// hot - Alert or Attack - enemy within CastRangeUnits, mirroring
// UOvercrowdDetectionComponent::CountHotUncontrolledEnemiesNearby's scan shape), and
// applies control via AEnemyBase::ReceiveControl(). Broadcasts OnAbilityCastApplied
// exactly once per successful cast so other systems (Gizmo bark trigger #59,
// instrumentation #37 - neither wired up by this issue) can subscribe later.
//
// Attached to the player pawn via CreateDefaultSubobject in
// AFlatCamera3DPrototypePawn's constructor, alongside AbilityUnlockComponent and
// AbilityCooldownComponent - not Blueprint-wired like UOvercrowdDetectionComponent,
// since a cast input needs a guaranteed-present component on the one playable pawn.
// Reads GetOwner()'s location directly, same as UOvercrowdDetectionComponent does.
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class KROWDKONTROL_API UAbilityCastComponent : public UActorComponent
{
	GENERATED_BODY()

	// Automation Framework test access. FindNearestValidTarget's targeting logic
	// (nearest-of-two, out-of-range exclusion, wrong-state exclusion) is verified
	// indirectly through TryCastAbility's return value and the target's resulting
	// GetEnemyState() - see KrowdKontrolAbilityCastComponentTest.cpp cases (e)-(g).
	friend class FKrowdKontrolAbilityCastComponentTest;

public:
	UAbilityCastComponent();

	// Flat range applied to all 5 abilities alike - per-ability range (mapping
	// EAbilityRange Short/Medium/Long to distinct concrete distances) is explicitly
	// out of this issue's scope; see AbilityData.h's EAbilityRange for the future
	// extension point.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability Cast", meta = (ClampMin = "0.0"))
	float CastRangeUnits = 1500.0f;

	// Gate order: IsAbilityUnlocked -> IsOnCooldown (read-only) -> IsAbilityLocked
	// (read-only, optional - see AbilityLockoutComponent.h; absence of the component is
	// NOT a gate failure, unlike Unlock/Cooldown) -> nearest-hot-enemy-in-range search
	// -> TryStartCooldown -> ReceiveControl -> broadcast. A whiff (no valid target in
	// range) does NOT consume the cooldown - TryStartCooldown is only called once a
	// target is already confirmed. Returns false and changes nothing if any gate fails
	// or no target is found.
	UFUNCTION(BlueprintCallable, Category = "Ability Cast")
	bool TryCastAbility(EAbilitySlot Ability);

	// Concrete throw distances per EAbilityRange tier, consulted by both
	// TryCastThrownAbilityAtLocation (ThrownCircle abilities' cursor-clamp) and
	// TryCastLineAbilityTowardLocation (Line abilities' fixed-length cast) - unlike
	// AbilityData's Colour/Duration/TargetType, the OG GDD's ability table only locks
	// the tier LABEL per ability (Short/Medium/Long), not concrete world units, so
	// these are placeholder-tunable EditDefaultsOnly values (same "open playtesting
	// question" rationale CastRangeUnits above already documents), not a second
	// AbilityData-owned locked constant.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability Cast|Thrown Range", meta = (ClampMin = "0.0"))
	float ShortThrowRangeUnits = 800.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability Cast|Thrown Range", meta = (ClampMin = "0.0"))
	float MediumThrowRangeUnits = 1200.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability Cast|Thrown Range", meta = (ClampMin = "0.0"))
	float LongThrowRangeUnits = 2000.0f;

	// Landing-circle AoE radius shared by every ThrownCircle ability (Stun/Sleep) -
	// "4x robot size" per docs/prd-ability-shapes.md, where "robot size" is locked as
	// the placeholder pawn's body diameter (AFlatCamera3DPrototypePawn's unscaled
	// /Engine/BasicShapes/Cube.Cube, ~100 units edge-to-edge) - 400 = 4x100.
	// EditDefaultsOnly for the same placeholder-tunable reason as the ranges above.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability Cast|Thrown Range", meta = (ClampMin = "0.0"))
	float ThrownCircleLandingRadiusUnits = 400.0f;

	// Perpendicular hit-width for Line-target abilities (Root, issue #255) - an enemy
	// within this distance of the Owner->line-end segment is considered "in the line's
	// path". Placeholder-tunable EditDefaultsOnly, same "open playtesting question"
	// rationale CastRangeUnits/ThrownCircleLandingRadiusUnits above already document.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability Cast|Line", meta = (ClampMin = "0.0"))
	float LineHitWidthUnits = 150.0f;

	// Cone FULL angle for Cone-target abilities (Snare, issue #254) - 75° per the
	// locked GDD table (docs/prd-ability-shapes.md). Placeholder-tunable
	// EditDefaultsOnly, same rationale LineHitWidthUnits/ThrownCircleLandingRadiusUnits
	// above document, even though this one value is currently GDD-locked rather than an
	// open playtesting question - kept EditDefaultsOnly for consistency with its
	// siblings and so a designer never needs a recompile to preview a variant.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability Cast|Cone", meta = (ClampMin = "0.0", ClampMax = "360.0"))
	float ConeFullAngleDegrees = 75.0f;

	// Cursor-aimed multi-target counterpart to TryCastAbility(), for
	// EAbilityTargetType::ThrownCircle abilities (Stun/Sleep - see AbilityData.h).
	// Generic and reusable - built so a future Stun implementation (issue #256) can
	// call this directly with zero duplication - but only wired into the real input
	// path for Sleep by this issue; see FlatCamera3DPrototypePawn::CastSleepAbility().
	// DesiredTargetLocation is clamped (see ComputeClampedThrowLocation) to this
	// actor's location + the tier distance matching AbilityData::Get(Ability).Range.
	// On landing, ReceiveControl(Ability) is called on every AEnemyBase within
	// ThrownCircleLandingRadiusUnits of the clamped point; OnAbilityCastApplied
	// broadcasts once per enemy that was actually Alert/Attack immediately before the
	// call (i.e. once per enemy that transitioned into Controlled - NOT once per
	// enemy merely visited, so an already-Controlled enemy woken early by this call
	// does not also broadcast as if freshly targeted).
	// Same gate order as TryCastAbility (paused -> briefing -> Owner exists ->
	// unlocked -> on-cooldown -> lockout), but unlike TryCastAbility the cooldown is
	// ALWAYS consumed once every gate passes, whether or not any enemy ends up inside
	// the landing circle - a thrown bomb commits the moment it's thrown, unlike
	// TryCastAbility's auto-nearest-target search (which only commits once a target
	// is actually found - see that method's own "a whiff never consumes the cooldown"
	// contract, deliberately NOT reused here).
	// Returns the number of enemies actually affected (0 is a valid, cooldown-
	// consuming throw that hit nothing); returns -1 if any gate failed and nothing
	// was changed.
	UFUNCTION(BlueprintCallable, Category = "Ability Cast")
	int32 TryCastThrownAbilityAtLocation(EAbilitySlot Ability, const FVector& DesiredTargetLocation);

	// Pure range-tier clamp math (mirrors AFlatCamera3DPrototypePawn::
	// IntersectRayWithGroundPlane's "public static, no live-state dependency" shape,
	// for the same direct-Automation-testability reason). Returns DesiredTargetLocation
	// unchanged if it's already within MaxRangeUnits of OwnerLocation, otherwise
	// OwnerLocation + (direction to DesiredTargetLocation) * MaxRangeUnits.
	static FVector ComputeClampedThrowLocation(const FVector& OwnerLocation, const FVector& DesiredTargetLocation, float MaxRangeUnits);

	// Instance-level convenience wrapper around ComputeClampedThrowLocation, using
	// this component's own GetOwner() location and GetThrowRangeUnitsForTier(Ability)
	// - the exact clamp TryCastThrownAbilityAtLocation applies internally. Exposed so
	// a caller previewing a thrown-ability aim before committing to the cast (e.g.
	// UAbilityPressHoldComponent's cursor-target indicator) can render the same
	// clamped landing point the cast will actually use, instead of the raw unclamped
	// cursor location. Returns DesiredTargetLocation unchanged if GetOwner() is null.
	UFUNCTION(BlueprintCallable, Category = "Ability Cast")
	FVector GetClampedThrowLocation(EAbilitySlot Ability, const FVector& DesiredTargetLocation) const;

	// Pure line-endpoint math for Line-target abilities (Root, issue #255). Unlike
	// ComputeClampedThrowLocation's "clamp to at most MaxRangeUnits" semantics (used
	// for ThrownCircle's click-to-land targeting), a Line ability's shot always
	// extends the full LineRangeUnits in the aimed direction - the AC says "extending
	// to the Long range tier", not "up to". FallbackDirection is used when
	// DesiredTargetLocation is coincident with OwnerLocation (direction otherwise
	// undefined) - callers pass the owner's current forward vector, mirroring
	// ComputeFacingRotation's own degenerate-cursor guard for the same edge case.
	static FVector ComputeLineEndLocation(const FVector& OwnerLocation, const FVector& DesiredTargetLocation, float LineRangeUnits, const FVector& FallbackDirection);

	// Instance-level convenience wrapper around ComputeLineEndLocation, using this
	// component's own GetOwner() location/forward vector and
	// GetThrowRangeUnitsForTier(AbilityData::Get(Ability).Range) - the exact endpoint
	// TryCastLineAbilityTowardLocation will use. Exposed so UAbilityPressHoldComponent's
	// Line indicator preview matches the real cast exactly. Returns DesiredTargetLocation
	// unchanged if GetOwner() is null.
	UFUNCTION(BlueprintCallable, Category = "Ability Cast")
	FVector GetLineEndLocation(EAbilitySlot Ability, const FVector& DesiredTargetLocation) const;

	// Cursor-aimed line-cast counterpart to TryCastThrownAbilityAtLocation(), for
	// EAbilityTargetType::Line abilities (Root - see AbilityData.h). DesiredTargetLocation
	// sets the aim direction only; the line always extends the full tier distance
	// (GetThrowRangeUnitsForTier(AbilityData::Get(Ability).Range)) from this actor's
	// location - see ComputeLineEndLocation's own doc comment for why this differs from
	// the ThrownCircle clamp. Every AEnemyBase within LineHitWidthUnits of the
	// Owner->line-end segment is affected (issue #255: Root pierces, it does not stop at
	// the first hit - see the plan/PR body for the design rationale). Same gate order and
	// "always consumes the cooldown once gates pass" contract as
	// TryCastThrownAbilityAtLocation (a fired line commits the moment it's fired).
	// Returns the number of enemies actually affected (0 is a valid, cooldown-consuming
	// cast that hit nothing); returns -1 if any gate failed and nothing was changed.
	UFUNCTION(BlueprintCallable, Category = "Ability Cast")
	int32 TryCastLineAbilityTowardLocation(EAbilitySlot Ability, const FVector& DesiredTargetLocation);

	// Pure direction-only math for Cone-target abilities (Snare - see AbilityData.h).
	// Unlike ComputeLineEndLocation (which returns a fixed endpoint), a cone's shape is
	// defined by (apex, direction, half-angle, range) - IsPointInCone below tests
	// directly against those, so only the aim direction needs computing here. Same
	// dead-zone/FallbackDirection guard as ComputeLineEndLocation, for the same
	// degenerate-cursor-at-owner-location case.
	static FVector ComputeConeDirection(const FVector& OwnerLocation, const FVector& DesiredTargetLocation, const FVector& FallbackDirection);

	// True if Point is within RangeUnits of ApexLocation AND within HalfAngleDegrees of
	// ConeDirection, tested on the X/Y plane only (matches this codebase's existing
	// flat-top-down convention - see ComputeFacingRotation's own SizeSquared2D usage).
	// A Point exactly coincident with ApexLocation (zero-length ToPoint, undefined
	// angle) is treated as OUTSIDE the cone - a deliberate, documented edge case, same
	// spirit as ComputeLineEndLocation's own degenerate-direction guard.
	static bool IsPointInCone(const FVector& Point, const FVector& ApexLocation, const FVector& ConeDirection, float HalfAngleDegrees, float RangeUnits);

	// Instance-level convenience wrapper around ComputeConeDirection, using this
	// component's own GetOwner() location/forward vector - the exact direction the real
	// cast will use. Exposed so UAbilityPressHoldComponent's Cone indicator preview
	// matches the real cast exactly. Returns DesiredTargetLocation unchanged if
	// GetOwner() is null.
	UFUNCTION(BlueprintCallable, Category = "Ability Cast")
	FVector GetConeDirection(const FVector& DesiredTargetLocation) const;

	// Instance-level convenience wrapper around GetThrowRangeUnitsForTier (private) -
	// exposes the resolved range for Ability's tier so UAbilityPressHoldComponent's Cone
	// indicator preview can show the same range the real cast will use, same rationale
	// GetLineEndLocation/GetClampedThrowLocation already document for their own callers.
	UFUNCTION(BlueprintCallable, Category = "Ability Cast")
	float GetConeRangeUnits(EAbilitySlot Ability) const;

	// Cursor-aimed cone-cast counterpart to TryCastLineAbilityTowardLocation(), for
	// EAbilityTargetType::Cone abilities (Snare - see AbilityData.h). Every AEnemyBase
	// within ConeFullAngleDegrees/Range of the Owner, in the direction of
	// DesiredTargetLocation, is affected (multi-target, same as Line/ThrownCircle).
	// Same gate order and "always consumes the cooldown once gates pass" contract as
	// TryCastLineAbilityTowardLocation (a fired cone commits the moment it's cast).
	// Returns the number of enemies actually affected (0 is a valid, cooldown-consuming
	// cast that hit nothing); returns -1 if any gate failed and nothing was changed.
	UFUNCTION(BlueprintCallable, Category = "Ability Cast")
	int32 TryCastConeAbilityTowardLocation(EAbilitySlot Ability, const FVector& DesiredTargetLocation);

	// Fires exactly once per successful TryCastAbility call, after ReceiveControl has
	// already been applied to TargetEnemy.
	UPROPERTY(BlueprintAssignable, Category = "Ability Cast")
	FOnAbilityCastApplied OnAbilityCastApplied;

protected:
	// Binds OnAbilityCastApplied to UCrowdMasterySubsystem::HandleAbilityCastApplied
	// (issue #174 AC1) here, not the constructor - GetWorld() has no valid subsystem
	// collection to resolve against until the owning actor has begun play, same
	// "bound in BeginPlay, not the constructor" precedent ADualZoneBoss::BeginPlay
	// establishes for a delegate binding that depends on world state.
	virtual void BeginPlay() override;

private:
	// Nearest AEnemyBase within CastRangeUnits of GetOwner() whose GetEnemyState() is
	// Alert or Attack (Idle/Controlled/Banked are never valid targets) - mirrors
	// UOvercrowdDetectionComponent::CountHotUncontrolledEnemiesNearby's
	// TActorIterator<AEnemyBase> + Alert/Attack filter + DistSquared shape. Returns
	// nullptr if no such enemy exists.
	AEnemyBase* FindNearestValidTarget() const;

	// Shared pre-target gate chain (paused -> briefing -> Owner exists -> unlocked ->
	// on-cooldown -> lockout) both TryCastAbility and TryCastThrownAbilityAtLocation
	// run before doing their own targeting work. Returns the resolved
	// UAbilityCooldownComponent* on success (both callers need it for
	// TryStartCooldown) or nullptr on any gate failure. CallerLogContext tags the
	// UE_LOG lines so a live-PIE log pull can tell which entry point they came from.
	UAbilityCooldownComponent* ResolvePassedCastGates(EAbilitySlot Ability, const TCHAR* CallerLogContext) const;

	// Maps an EAbilityRange tier to the matching ShortThrowRangeUnits /
	// MediumThrowRangeUnits / LongThrowRangeUnits property.
	float GetThrowRangeUnitsForTier(EAbilityRange Range) const;

	// Shared AoE-sweep body for TryCastThrownAbilityAtLocation and
	// TryCastLineAbilityTowardLocation - both call ReceiveControl(Ability) on every
	// AEnemyBase for which IsEnemyInShape(EnemyLocation) returns true, counting and
	// broadcasting only the ones that were Alert/Attack immediately beforehand. The two
	// callers differ only in shape (circle-around-a-point vs strip-around-a-segment),
	// passed in as IsEnemyInShape.
	int32 ApplyControlToEnemiesInShape(EAbilitySlot Ability, TFunctionRef<bool(const FVector& EnemyLocation)> IsEnemyInShape);
};
