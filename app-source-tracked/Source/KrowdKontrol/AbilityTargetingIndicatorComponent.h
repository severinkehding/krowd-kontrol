#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AbilityTargetingIndicatorComponent.generated.h"

class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;

// The four shape kinds REQ-3 (docs/prd-cursor-aiming.md) names. CircleAtActor and
// CircleAtCursor render identically (both produce a full-circle mask at Origin) -
// the distinction is purely which world point the *caller* feeds as Origin (the
// owning actor's location vs. a future cursor-to-world-space projection on
// AFlatCamera3DPrototypePawn, not yet implemented - see the input-wiring follow-on
// issue), not a rendering difference; kept as two named values so callers stay
// self-documenting rather than passing a bare "Circle" and a comment.
UENUM(BlueprintType)
enum class EAbilityIndicatorShapeKind : uint8
{
	CircleAtActor,
	CircleAtCursor,
	Cone,
	Line
};

// Parameterizes one shape instance. Origin/FacingRotation are always plain
// caller-supplied world-space values (issue #264 does not derive them) - a future
// cursor-to-world-space projection on AFlatCamera3DPrototypePawn, not yet
// implemented, will supply the "cursor world position" half for a future caller.
USTRUCT(BlueprintType)
struct FAbilityIndicatorShapeSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ability Indicator")
	EAbilityIndicatorShapeKind Kind = EAbilityIndicatorShapeKind::CircleAtActor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ability Indicator")
	FVector Origin = FVector::ZeroVector;

	// Facing direction for Cone/Line; ignored for the two Circle kinds.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ability Indicator")
	FRotator FacingRotation = FRotator::ZeroRotator;

	// Circle radius (Circle kinds) or cone/line range (Cone/Line kinds), world units.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ability Indicator", meta = (ClampMin = "0.0"))
	float RangeUnits = 0.0f;

	// Cone FULL angle in degrees (e.g. 75 for Snare per docs/prd-ability-shapes.md) -
	// Cone kind only; ignored for Circle/Line kinds, which the component maps to its
	// own fixed effective angles (see .cpp).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ability Indicator", meta = (ClampMin = "0.0", ClampMax = "360.0"))
	float ConeFullAngleDegrees = 0.0f;
};

// REQ-3 (docs/prd-cursor-aiming.md) rendering primitive: any ability can drive this
// component with a shape spec + colour to show/flash a translucent ground indicator,
// without touching rendering details itself. Deliberately owns no ability/cast/
// cooldown logic and is not wired onto any pawn by this issue - see the "Wire
// press/hold indicator semantics to all five ability keys" issue for that.
//
// IndicatorMeshComponent is a free-floating (never-attached) child, driven purely by
// explicit SetWorldLocation/SetWorldRotation/SetWorldScale3D - same "world-fixed, not
// owner-relative" idiom as UAbilityCastVFXComponent::CastFlashLightComponent, and for
// the same reason: the shape's world position must not silently drag with whatever
// actor happens to own this component.
//
// InitializeIndicatorVisual() creates IndicatorMeshComponent under GetOwner() with a
// fixed name ("AbilityIndicatorMeshComponent") rather than a generated unique one, so
// only one UAbilityTargetingIndicatorComponent instance per Owner is supported - a
// second instance on the same Owner would collide with the first one's mesh component
// name. Give each ability/caller its own Owner (or its own component-holding actor) if
// more than one indicator is needed at once.
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class KROWDKONTROL_API UAbilityTargetingIndicatorComponent : public UActorComponent
{
	GENERATED_BODY()

	friend class FKrowdKontrolAbilityTargetingIndicatorComponentTest;

public:
	UAbilityTargetingIndicatorComponent();

	// Lazily creates and attaches IndicatorMeshComponent + its dynamic material
	// instance. Called automatically from BeginPlay(); exposed publicly (and
	// idempotent) so the Automation test can drive it deterministically without a
	// full actor BeginPlay lifecycle - mirrors UEnemyTypeIndicatorComponent::
	// InitializeMarkerVisual() / UAbilityCastVFXComponent::InitializeCastVFX().
	UFUNCTION(BlueprintCallable, Category = "Ability Indicator")
	void InitializeIndicatorVisual();

	// Shows ShapeSpec in Colour (always AbilityData::Get(Ability).Colour at the real
	// call site - MISSION.md Hard Invariant 3, never a local FLinearColor literal).
	// Safe to call repeatedly (e.g. every frame while a key is held) - just updates
	// the existing mesh/material rather than recreating anything.
	UFUNCTION(BlueprintCallable, Category = "Ability Indicator")
	void Show(const FAbilityIndicatorShapeSpec& ShapeSpec, FLinearColor Colour);

	UFUNCTION(BlueprintCallable, Category = "Ability Indicator")
	void Hide();

	// Show()s ShapeSpec/Colour, then auto-Hide()s after FlashDurationSeconds - the
	// "press" half of the shared press/hold semantics (Cursor & Aiming PRD decision
	// 3). Timer-driven, mirroring UAbilityCastVFXComponent's CastFlashTimerHandle.
	UFUNCTION(BlueprintCallable, Category = "Ability Indicator")
	void Flash(const FAbilityIndicatorShapeSpec& ShapeSpec, FLinearColor Colour, float FlashDurationSeconds = 0.15f);

	// Reflected state - the Automation test (and any future Unreal MCP-driven E2E
	// holdout, which per project memory can only read reflected UPROPERTY state, not
	// GPU pixels) asserts against these directly rather than any rendered output.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ability Indicator")
	bool bIsVisible = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ability Indicator")
	FAbilityIndicatorShapeSpec CurrentShapeSpec;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ability Indicator")
	FLinearColor CurrentColour = FLinearColor::Black;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ability Indicator")
	TObjectPtr<UStaticMeshComponent> IndicatorMeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ability Indicator")
	TObjectPtr<UMaterialInstanceDynamic> IndicatorMaterialInstance;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	// Effective mask half-angle for Kind, in degrees - CircleAtActor/CircleAtCursor
	// always 180 (unmasked full circle), Cone uses ShapeSpec.ConeFullAngleDegrees / 2,
	// Line uses a small fixed constant (LineMaskHalfAngleDegrees) regardless of what
	// ShapeSpec.ConeFullAngleDegrees holds - see .cpp for the constant and rationale.
	static float GetEffectiveMaskHalfAngleDegrees(const FAbilityIndicatorShapeSpec& ShapeSpec);

	void ApplyShapeTransform(const FAbilityIndicatorShapeSpec& ShapeSpec);
	void ApplyMaterialParameters(const FAbilityIndicatorShapeSpec& ShapeSpec, FLinearColor Colour);
	void ClearFlash();

	FTimerHandle FlashTimerHandle;
	bool bHasInitializedIndicatorVisual = false;
};
