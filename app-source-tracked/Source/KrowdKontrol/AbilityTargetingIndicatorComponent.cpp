#include "AbilityTargetingIndicatorComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "GameFramework/Actor.h"

namespace
{
	// Small, deliberately non-zero so a Line still reads as a narrow wedge rather than
	// a literal zero-width invisible sliver under the material's angle mask (Research:
	// "line = cone with a near-zero angle parameter and a Length param instead of
	// Radius"). Placeholder-quality per MISSION.md Quality Standards - not a locked
	// design value, revisit once the Ability Targeting Shapes PRD's Root line ships.
	constexpr float LineMaskHalfAngleDegrees = 3.0f;
	constexpr float FullCircleMaskHalfAngleDegrees = 180.0f;
}

UAbilityTargetingIndicatorComponent::UAbilityTargetingIndicatorComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UAbilityTargetingIndicatorComponent::BeginPlay()
{
	Super::BeginPlay();
	InitializeIndicatorVisual();
}

void UAbilityTargetingIndicatorComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FlashTimerHandle);
	}
	Super::EndPlay(EndPlayReason);
}

void UAbilityTargetingIndicatorComponent::InitializeIndicatorVisual()
{
	if (bHasInitializedIndicatorVisual)
	{
		return;
	}

	AActor* Owner = GetOwner();
	if (!Owner || !Owner->GetRootComponent())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UAbilityTargetingIndicatorComponent: InitializeIndicatorVisual on '%s' found no Owner root component - indicator not created."),
			*GetNameSafe(Owner));
		// bHasInitializedIndicatorVisual deliberately NOT set here - retryable, same as
		// UEnemyTypeIndicatorComponent::InitializeMarkerVisual().
		return;
	}
	bHasInitializedIndicatorVisual = true;

	IndicatorMeshComponent = NewObject<UStaticMeshComponent>(Owner, TEXT("AbilityIndicatorMeshComponent"));
	// No SetupAttachment/AttachToComponent call - free-floating, world-fixed by
	// explicit SetWorldLocation/SetWorldRotation/SetWorldScale3D in ApplyShapeTransform,
	// same idiom as AbilityCastVFXComponent::CastFlashLightComponent.
	IndicatorMeshComponent->RegisterComponent();
	IndicatorMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	IndicatorMeshComponent->SetVisibility(false);

	// NOTE (Task 3 GOTCHA, confirmed live 2026-08-22): ConstructorHelpers::FObjectFinder
	// is constructor-only - calling it from here (InitializeIndicatorVisual(), not the
	// constructor, deliberately, since it must be deferred until an Owner with a root
	// component exists) is a genuine fatal error, not just an unreliable footgun:
	// "FObjectFinders can't be used outside of constructors to find ..." crashed the
	// Editor outright when first tried. TSoftObjectPtr::LoadSynchronous() has no such
	// constructor-only restriction and is the documented fallback this task's plan
	// anticipated for exactly this outcome.
	static const TSoftObjectPtr<UStaticMesh> PlaneMeshSoftPtr(FSoftObjectPath(TEXT("/Engine/BasicShapes/Plane.Plane")));
	if (UStaticMesh* PlaneMesh = PlaneMeshSoftPtr.LoadSynchronous())
	{
		IndicatorMeshComponent->SetStaticMesh(PlaneMesh);
	}

	static const TSoftObjectPtr<UMaterialInterface> BaseMaterialSoftPtr(
		FSoftObjectPath(TEXT("/Game/_Placeholder/Abilities/M_AbilityIndicator.M_AbilityIndicator")));
	if (UMaterialInterface* BaseMaterial = BaseMaterialSoftPtr.LoadSynchronous())
	{
		IndicatorMaterialInstance = UMaterialInstanceDynamic::Create(BaseMaterial, this);
		if (IndicatorMaterialInstance)
		{
			IndicatorMeshComponent->SetMaterial(0, IndicatorMaterialInstance);
		}
	}
	// If either soft load fails (Task 1's material wasn't created in this environment -
	// see that task's GOTCHA), IndicatorMeshComponent/IndicatorMaterialInstance stay
	// partially/fully null; Show()/Hide()/Flash() below all null-check before touching
	// them, so component state (bIsVisible/CurrentShapeSpec/CurrentColour) - and thus
	// this issue's acceptance criteria - stays fully correct and testable regardless.
}

void UAbilityTargetingIndicatorComponent::Show(const FAbilityIndicatorShapeSpec& ShapeSpec, FLinearColor Colour)
{
	InitializeIndicatorVisual();

	bIsVisible = true;
	CurrentShapeSpec = ShapeSpec;
	CurrentColour = Colour;

	if (IndicatorMeshComponent)
	{
		IndicatorMeshComponent->SetVisibility(true);
		ApplyShapeTransform(ShapeSpec);
	}
	ApplyMaterialParameters(ShapeSpec, Colour); // no-ops safely if IndicatorMaterialInstance is null
}

void UAbilityTargetingIndicatorComponent::Hide()
{
	bIsVisible = false;
	if (IndicatorMeshComponent)
	{
		IndicatorMeshComponent->SetVisibility(false);
	}
}

void UAbilityTargetingIndicatorComponent::Flash(const FAbilityIndicatorShapeSpec& ShapeSpec, FLinearColor Colour, float FlashDurationSeconds)
{
	Show(ShapeSpec, Colour);
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			FlashTimerHandle, this, &UAbilityTargetingIndicatorComponent::ClearFlash, FlashDurationSeconds, false);
	}
}

void UAbilityTargetingIndicatorComponent::ClearFlash()
{
	Hide();
}

float UAbilityTargetingIndicatorComponent::GetEffectiveMaskHalfAngleDegrees(const FAbilityIndicatorShapeSpec& ShapeSpec)
{
	switch (ShapeSpec.Kind)
	{
	case EAbilityIndicatorShapeKind::Cone:
		return ShapeSpec.ConeFullAngleDegrees * 0.5f;
	case EAbilityIndicatorShapeKind::Line:
		return LineMaskHalfAngleDegrees;
	case EAbilityIndicatorShapeKind::CircleAtActor:
	case EAbilityIndicatorShapeKind::CircleAtCursor:
	default:
		return FullCircleMaskHalfAngleDegrees;
	}
}

void UAbilityTargetingIndicatorComponent::ApplyShapeTransform(const FAbilityIndicatorShapeSpec& ShapeSpec)
{
	// RangeUnits drives world-space scale of the (UV 0..1) plane mesh; the material's
	// angle mask (Task 1) does the shape-kind-dependent clipping in UV space. A small
	// fixed Z offset avoids z-fighting against the ground mesh, per Research
	// recommendation 4 (Translucency Sort Priority tuning deferred until this project
	// has other translucent ground-layer effects to sort against).
	constexpr float GroundZOffsetUnits = 2.0f;
	IndicatorMeshComponent->SetWorldLocation(ShapeSpec.Origin + FVector(0.0f, 0.0f, GroundZOffsetUnits));
	IndicatorMeshComponent->SetWorldRotation(ShapeSpec.FacingRotation);
	const float Diameter = FMath::Max(ShapeSpec.RangeUnits, 0.0f) * 2.0f;
	IndicatorMeshComponent->SetWorldScale3D(FVector(Diameter, Diameter, 1.0f));
}

void UAbilityTargetingIndicatorComponent::ApplyMaterialParameters(const FAbilityIndicatorShapeSpec& ShapeSpec, FLinearColor Colour)
{
	if (!IndicatorMaterialInstance)
	{
		return;
	}
	IndicatorMaterialInstance->SetVectorParameterValue(TEXT("Colour"), Colour);
	IndicatorMaterialInstance->SetScalarParameterValue(TEXT("ConeHalfAngleDegrees"), GetEffectiveMaskHalfAngleDegrees(ShapeSpec));
}
