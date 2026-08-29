#include "ControlledDurationIndicatorComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "EnemyBase.h"

UControlledDurationIndicatorComponent::UControlledDurationIndicatorComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UControlledDurationIndicatorComponent::BeginPlay()
{
	Super::BeginPlay();
	InitializeIndicatorVisual();
}

void UControlledDurationIndicatorComponent::InitializeIndicatorVisual()
{
	if (bHasInitializedIndicatorVisual)
	{
		return;
	}

	AActor* Owner = GetOwner();
	// CRITICAL GOTCHA (verified against engine semantics, not the mirrored component's
	// own code): UActorComponent::RegisterComponent() requires GetOwner()->GetWorld()
	// to be non-null internally (RegisterComponentWithWorld needs a real UWorld). This
	// method is reachable from Show(), which AEnemyBase::ReceiveControl() calls
	// unconditionally - and dozens of existing tests (KrowdKontrolEnemyBaseTest.cpp and
	// others) call ReceiveControl() on a bare NewObject<AEnemyBaseTestActor>() with NO
	// UWorld at all. Skipping this check (as the mirrored
	// AbilityTargetingIndicatorComponent's own InitializeIndicatorVisual() does, since
	// it's never called from a path this hot) would crash every one of those tests the
	// moment Show() is wired into ReceiveControl(). Retryable
	// (bHasInitializedIndicatorVisual NOT set here), same shape as the no-root-component
	// branch below.
	if (!Owner || !Owner->GetRootComponent() || !Owner->GetWorld())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UControlledDurationIndicatorComponent: InitializeIndicatorVisual on '%s' found no Owner root component or World - indicator mesh not created (reflected state still updates normally)."),
			*GetNameSafe(Owner));
		return;
	}
	bHasInitializedIndicatorVisual = true;

	FillMeshComponent = NewObject<UStaticMeshComponent>(Owner, TEXT("ControlledDurationFillMeshComponent"));
	FillMeshComponent->SetupAttachment(Owner->GetRootComponent());
	FillMeshComponent->RegisterComponent();
	FillMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FillMeshComponent->SetRelativeLocation(FVector(0.0f, 0.0f, BarHeightOffset));
	// Must default to hidden (mirrors AbilityTargetingIndicatorComponent.cpp) - a
	// freshly-initialized indicator should never flash visible before Show() is called.
	FillMeshComponent->SetVisibility(false);

	static const TSoftObjectPtr<UStaticMesh> PlaneMeshSoftPtr(FSoftObjectPath(TEXT("/Engine/BasicShapes/Plane.Plane")));
	if (UStaticMesh* PlaneMesh = PlaneMeshSoftPtr.LoadSynchronous())
	{
		FillMeshComponent->SetStaticMesh(PlaneMesh);
	}
	else
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UControlledDurationIndicatorComponent: failed to load plane mesh '%s' on '%s' - indicator will have no visible mesh."),
			*PlaneMeshSoftPtr.ToString(), *GetNameSafe(Owner));
	}

	static const TSoftObjectPtr<UMaterialInterface> BaseMaterialSoftPtr(
		FSoftObjectPath(TEXT("/Game/_Placeholder/Abilities/M_AbilityIndicator.M_AbilityIndicator")));
	if (UMaterialInterface* BaseMaterial = BaseMaterialSoftPtr.LoadSynchronous())
	{
		FillMaterialInstance = UMaterialInstanceDynamic::Create(BaseMaterial, this);
		if (FillMaterialInstance)
		{
			FillMeshComponent->SetMaterial(0, FillMaterialInstance);
		}
		else
		{
			UE_LOG(LogTemp, Warning,
				TEXT("UControlledDurationIndicatorComponent: UMaterialInstanceDynamic::Create() returned null for '%s' on '%s' - indicator will have no material."),
				*BaseMaterialSoftPtr.ToString(), *GetNameSafe(Owner));
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UControlledDurationIndicatorComponent: failed to load placeholder material '%s' on '%s' - indicator will have no visible colour/fill."),
			*BaseMaterialSoftPtr.ToString(), *GetNameSafe(Owner));
	}

	// If either soft load fails, FillMeshComponent/FillMaterialInstance stay
	// partially/fully null; Show()/Hide()/RefreshFillFraction() all null-check before
	// touching them, so component state (bIsVisible/FillFraction/CurrentColour) - and
	// thus this issue's acceptance criteria - stays fully correct and testable
	// regardless.
	ApplyVisualFillFraction();
	if (FillMaterialInstance)
	{
		FillMaterialInstance->SetVectorParameterValue(TEXT("Colour"), CurrentColour);
	}
}

void UControlledDurationIndicatorComponent::Show(FLinearColor Colour, bool bInIsColourMatchBonused)
{
	InitializeIndicatorVisual();

	bIsVisible = true;
	CurrentColour = Colour;
	bIsColourMatchBonused = bInIsColourMatchBonused;

	if (FillMaterialInstance)
	{
		FillMaterialInstance->SetVectorParameterValue(TEXT("Colour"), Colour);
	}
	if (FillMeshComponent)
	{
		FillMeshComponent->SetVisibility(true);
	}
	RefreshFillFraction(); // Remaining == Total right after ReceiveControl sets them -> 1.0
}

void UControlledDurationIndicatorComponent::Hide()
{
	bIsVisible = false;
	if (FillMeshComponent)
	{
		FillMeshComponent->SetVisibility(false);
	}
}

void UControlledDurationIndicatorComponent::RefreshFillFraction()
{
	const AEnemyBase* OwnerEnemy = Cast<AEnemyBase>(GetOwner());
	if (!OwnerEnemy)
	{
		return;
	}
	const float Total = OwnerEnemy->GetTotalControlledSeconds();
	FillFraction = Total > 0.0f ? OwnerEnemy->GetRemainingControlledSeconds() / Total : 0.0f;
	ApplyVisualFillFraction();
}

void UControlledDurationIndicatorComponent::ApplyVisualFillFraction()
{
	if (!FillMeshComponent)
	{
		return;
	}
	const float ClampedFraction = FMath::Clamp(FillFraction, 0.0f, 1.0f);
	// /Engine/BasicShapes/Plane is 100uu at scale 1.0, so scale = desired size-in-cm /
	// 100 - same convention as RoomActor.cpp/DoorConnectorActor.cpp/
	// AbilityTargetingIndicatorComponent.cpp.
	const float WidthScale = (BarWidthUnits / 100.0f) * ClampedFraction;
	const float DepthScale = (bIsColourMatchBonused ? BarDepthUnitsBonused : BarDepthUnits) / 100.0f;
	FillMeshComponent->SetRelativeScale3D(FVector(WidthScale, DepthScale, 1.0f));
	// Left-anchored drain: as ClampedFraction shrinks from 1 to 0, shift the (shrinking)
	// mesh's centre left so its left edge stays fixed rather than shrinking
	// symmetrically about the enemy's centre.
	const float XOffset = -(BarWidthUnits * 0.5f) * (1.0f - ClampedFraction);
	FillMeshComponent->SetRelativeLocation(FVector(XOffset, 0.0f, BarHeightOffset));
}
