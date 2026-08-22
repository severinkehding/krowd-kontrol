// Confirms UAbilityTargetingIndicatorComponent (issue #264, REQ-3 of
// docs/prd-cursor-aiming.md) shows/hides/flashes a shape indicator whose reflected
// state (colour, shape params, mesh transform, visibility) matches AbilityData's
// locked colour and the caller-supplied shape spec, for all 5 abilities and all 4
// shape kinds, without ever reading back rendered pixels - see
// holdout_gameplay_component_state_unobservable / holdout_no_read_tool project memory
// for why this project's tests always assert reflected UPROPERTY state instead.
//
// Mirrors KrowdKontrolEnemyTypeIndicatorComponentTest.cpp's NewObject+RegisterComponent
// +explicit-Initialize shape and its missing-root/idempotency regression cases (f)/(g);
// KrowdKontrolAbilityVFXColourTest.cpp's per-slot colour-loop and reserved-colour
// idiom, and its friend-grant "call the timer-driven clear function directly" idiom
// for exercising Flash()'s auto-hide without needing an in-test FTimerManager advance
// (no existing precedent for that in this codebase - see this issue's plan Risk table).
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the other
// KrowdKontrol.Unit.* tests.

#include "Misc/AutomationTest.h"
#include "AbilityTargetingIndicatorComponent.h"
#include "AbilityData.h"
#include "ReservedGameplayColours.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/Material.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	// Shared by cases (a) and (a2) below, which each need their own Owner actor with a
	// registered root component (a fresh UAbilityTargetingIndicatorComponent creates its
	// IndicatorMeshComponent under Owner with a fixed name, so two indicators can't share
	// one Owner).
	AActor* SpawnActorWithRegisteredRoot(UWorld* World)
	{
		AActor* Owner = World->SpawnActor<AActor>();
		if (!Owner)
		{
			return nullptr;
		}
		USceneComponent* Root = NewObject<USceneComponent>(Owner);
		Root->RegisterComponent();
		Owner->SetRootComponent(Root);
		return Owner;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolAbilityTargetingIndicatorComponentTest,
	"KrowdKontrol.Unit.AbilityTargetingIndicatorComponent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolAbilityTargetingIndicatorComponentTest::RunTest(const FString& Parameters)
{
	// InitializeIndicatorVisual() now logs a warning (see error-handling fix) whenever
	// the placeholder M_AbilityIndicator material fails to load - and per this issue's
	// own Known Gaps, that asset was never authored in this environment, so every
	// component instance below that reaches this code path will log it once. Not this
	// test's concern (it's exercised independently of whether the content asset
	// exists - see the forced-MID injection below), so expect any number of
	// occurrences. Occurrences=0 means "any count"; IsRegex=false per the D-012
	// AddExpectedError convention.
	AddExpectedError(TEXT("failed to load placeholder material"), EAutomationExpectedErrorFlags::Contains, 0, false);

	// (a) Construction: spawn an AActor + manual USceneComponent root in a
	// CreateNewMap() World, NewObject + RegisterComponent, then drive
	// InitializeIndicatorVisual() directly rather than relying on BeginPlay timing.
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
	{
		return false;
	}
	AActor* Owner = SpawnActorWithRegisteredRoot(World);
	if (!TestNotNull(TEXT("AActor owner should spawn into the test World"), Owner))
	{
		return false;
	}

	UAbilityTargetingIndicatorComponent* Indicator = NewObject<UAbilityTargetingIndicatorComponent>(Owner);
	Indicator->RegisterComponent();
	Indicator->InitializeIndicatorVisual();

	if (!TestNotNull(TEXT("IndicatorMeshComponent should exist after InitializeIndicatorVisual"),
		ToRawPtr(Indicator->IndicatorMeshComponent)))
	{
		return false;
	}
	// ConstructorHelpers::FObjectFinder-style asset lookups silently no-op on failure
	// (no assert, no log) - without this check, a future asset path change would leave
	// the indicator invisible in-game while every other assertion in this file still
	// passed. Mirrors KrowdKontrolPaper2DPipelineSmokeTest.cpp:147-150's precedent for
	// exactly this asset-load-silently-no-ops class of gap.
	TestNotNull(TEXT("IndicatorMeshComponent should have a static mesh assigned after InitializeIndicatorVisual"),
		ToRawPtr(Indicator->IndicatorMeshComponent->GetStaticMesh()));

	// Idempotency regression: a second InitializeIndicatorVisual() call must not
	// create a duplicate IndicatorMeshComponent. Mirrors
	// KrowdKontrolEnemyTypeIndicatorComponentTest.cpp case (f).
	UStaticMeshComponent* FirstMeshComponent = Indicator->IndicatorMeshComponent;
	Indicator->InitializeIndicatorVisual();
	TestTrue(TEXT("A second InitializeIndicatorVisual() call should not create a duplicate IndicatorMeshComponent"),
		Indicator->IndicatorMeshComponent == FirstMeshComponent);

	// Force material-parameter coverage independent of whether the placeholder
	// M_AbilityIndicator content asset exists in this environment (it may not - see
	// this issue's Known Gaps). Uses the engine's always-available default material as
	// the base so ApplyMaterialParameters() is exercised unconditionally by (b)/(c)
	// below, rather than silently skipping their material-param assertions.
	Indicator->IndicatorMaterialInstance = UMaterialInstanceDynamic::Create(
		UMaterial::GetDefaultMaterial(MD_Surface), Indicator);
	if (!TestNotNull(TEXT("Forced IndicatorMaterialInstance for material-param coverage should be non-null"),
		ToRawPtr(Indicator->IndicatorMaterialInstance)))
	{
		return false;
	}

	// (a2) Lazy auto-init: Show() must self-initialize on a component that never had
	// InitializeIndicatorVisual() called on it directly - this is Show()'s documented
	// entry point for real callers (BeginPlay only calls InitializeIndicatorVisual()
	// automatically; ability-cast code is expected to just call Show()). Uses its own
	// Owner actor, not the (a) Indicator's, since InitializeIndicatorVisual() creates
	// its IndicatorMeshComponent under Owner with a fixed name - a second component on
	// the same Owner would collide with the (a) Indicator's mesh component name.
	AActor* LazyOwner = SpawnActorWithRegisteredRoot(World);
	UAbilityTargetingIndicatorComponent* LazyIndicator = NewObject<UAbilityTargetingIndicatorComponent>(LazyOwner);
	LazyIndicator->RegisterComponent();
	FAbilityIndicatorShapeSpec LazyInitTestSpec;
	LazyInitTestSpec.Kind = EAbilityIndicatorShapeKind::CircleAtActor;
	LazyInitTestSpec.RangeUnits = 100.0f;
	LazyIndicator->Show(LazyInitTestSpec, FLinearColor::White);
	TestNotNull(TEXT("Show() with no prior InitializeIndicatorVisual() call should still self-initialize IndicatorMeshComponent"),
		ToRawPtr(LazyIndicator->IndicatorMeshComponent));

	// (b) Colour-match AC: for each of the 5 EAbilitySlot values, Show() with
	// AbilityData::Get(Slot).Colour must land exactly in CurrentColour (and, if the
	// placeholder material was successfully created, in the MID's own Colour param).
	const TArray<EAbilitySlot> AllSlots = { EAbilitySlot::Stun, EAbilitySlot::Sleep,
		EAbilitySlot::Root, EAbilitySlot::Fear, EAbilitySlot::Snare };
	const TArray<FLinearColor> AllReserved = ReservedGameplayColours::GetAll();

	FAbilityIndicatorShapeSpec ColourTestSpec;
	ColourTestSpec.Kind = EAbilityIndicatorShapeKind::CircleAtActor;
	ColourTestSpec.Origin = FVector(100.0f, 0.0f, 0.0f);
	ColourTestSpec.RangeUnits = 250.0f;

	for (const EAbilitySlot Slot : AllSlots)
	{
		const FLinearColor SlotColour = AbilityData::Get(Slot).Colour;
		Indicator->Show(ColourTestSpec, SlotColour);

		TestTrue(*FString::Printf(TEXT("Indicator colour for slot %d should match its locked AbilityData colour"), static_cast<int32>(Slot)),
			Indicator->CurrentColour.Equals(SlotColour, 0.01f));
		TestTrue(*FString::Printf(TEXT("Indicator colour for slot %d should be one of the 5 reserved gameplay colours"), static_cast<int32>(Slot)),
			AllReserved.ContainsByPredicate([SlotColour](const FLinearColor& Reserved) { return Reserved.Equals(SlotColour, 0.01f); }));

		if (Indicator->IndicatorMaterialInstance)
		{
			FLinearColor MaterialColour;
			if (Indicator->IndicatorMaterialInstance->GetVectorParameterValue(FMaterialParameterInfo(TEXT("Colour")), MaterialColour))
			{
				TestTrue(*FString::Printf(TEXT("MID Colour param for slot %d should match its locked AbilityData colour"), static_cast<int32>(Slot)),
					MaterialColour.Equals(SlotColour, 0.01f));
			}
		}
	}

	// (c) Shape-kind-switching AC: each of the 4 shape kinds, with distinct
	// RangeUnits/ConeFullAngleDegrees/Origin/FacingRotation, must round-trip exactly
	// into CurrentShapeSpec and drive IndicatorMeshComponent's world transform and the
	// mask half-angle correctly.
	struct FShapeCase
	{
		EAbilityIndicatorShapeKind Kind;
		FVector Origin;
		FRotator FacingRotation;
		float RangeUnits;
		float ConeFullAngleDegrees;
		float ExpectedMaskHalfAngleDegrees;
	};

	const TArray<FShapeCase> ShapeCases = {
		{ EAbilityIndicatorShapeKind::CircleAtActor, FVector(0.0f, 0.0f, 0.0f), FRotator(0.0f, 0.0f, 0.0f), 300.0f, 0.0f, 180.0f },
		{ EAbilityIndicatorShapeKind::CircleAtCursor, FVector(500.0f, -200.0f, 0.0f), FRotator(0.0f, 90.0f, 0.0f), 450.0f, 0.0f, 180.0f },
		{ EAbilityIndicatorShapeKind::Cone, FVector(-300.0f, 150.0f, 0.0f), FRotator(0.0f, 45.0f, 0.0f), 800.0f, 75.0f, 37.5f },
		{ EAbilityIndicatorShapeKind::Line, FVector(100.0f, 100.0f, 0.0f), FRotator(0.0f, 180.0f, 0.0f), 1200.0f, 0.0f, 3.0f },
	};

	for (const FShapeCase& Case : ShapeCases)
	{
		FAbilityIndicatorShapeSpec ShapeSpec;
		ShapeSpec.Kind = Case.Kind;
		ShapeSpec.Origin = Case.Origin;
		ShapeSpec.FacingRotation = Case.FacingRotation;
		ShapeSpec.RangeUnits = Case.RangeUnits;
		ShapeSpec.ConeFullAngleDegrees = Case.ConeFullAngleDegrees;

		Indicator->Show(ShapeSpec, FLinearColor::White);

		TestTrue(*FString::Printf(TEXT("CurrentShapeSpec.Kind should round-trip for shape kind %d"), static_cast<int32>(Case.Kind)),
			Indicator->CurrentShapeSpec.Kind == Case.Kind);
		TestTrue(*FString::Printf(TEXT("CurrentShapeSpec.Origin should round-trip for shape kind %d"), static_cast<int32>(Case.Kind)),
			Indicator->CurrentShapeSpec.Origin.Equals(Case.Origin, 0.01f));
		TestTrue(*FString::Printf(TEXT("CurrentShapeSpec.FacingRotation should round-trip for shape kind %d"), static_cast<int32>(Case.Kind)),
			Indicator->CurrentShapeSpec.FacingRotation.Equals(Case.FacingRotation, 0.01f));
		TestEqual(*FString::Printf(TEXT("CurrentShapeSpec.RangeUnits should round-trip for shape kind %d"), static_cast<int32>(Case.Kind)),
			Indicator->CurrentShapeSpec.RangeUnits, Case.RangeUnits);
		TestEqual(*FString::Printf(TEXT("CurrentShapeSpec.ConeFullAngleDegrees should round-trip for shape kind %d"), static_cast<int32>(Case.Kind)),
			Indicator->CurrentShapeSpec.ConeFullAngleDegrees, Case.ConeFullAngleDegrees);

		const FVector ExpectedLocation = Case.Origin + FVector(0.0f, 0.0f, 2.0f);
		TestTrue(*FString::Printf(TEXT("IndicatorMeshComponent world location should match Origin + Z offset for shape kind %d"), static_cast<int32>(Case.Kind)),
			Indicator->IndicatorMeshComponent->GetComponentLocation().Equals(ExpectedLocation, 0.5f));
		TestTrue(*FString::Printf(TEXT("IndicatorMeshComponent world rotation should match FacingRotation for shape kind %d"), static_cast<int32>(Case.Kind)),
			Indicator->IndicatorMeshComponent->GetComponentRotation().Equals(Case.FacingRotation, 0.5f));
		const float ExpectedDiameter = Case.RangeUnits * 2.0f;
		TestTrue(*FString::Printf(TEXT("IndicatorMeshComponent world scale should be 2x RangeUnits for shape kind %d"), static_cast<int32>(Case.Kind)),
			Indicator->IndicatorMeshComponent->GetComponentScale().Equals(FVector(ExpectedDiameter, ExpectedDiameter, 1.0f), 0.5f));

		// GetEffectiveMaskHalfAngleDegrees is private - reachable here via this test
		// class's friend grant on UAbilityTargetingIndicatorComponent, same idiom
		// FKrowdKontrolAbilityVFXColourTest uses for ClearCastFlash().
		TestEqual(*FString::Printf(TEXT("Effective mask half-angle should match for shape kind %d"), static_cast<int32>(Case.Kind)),
			UAbilityTargetingIndicatorComponent::GetEffectiveMaskHalfAngleDegrees(ShapeSpec), Case.ExpectedMaskHalfAngleDegrees);

		if (Indicator->IndicatorMaterialInstance)
		{
			float MaterialMaskAngle = 0.0f;
			if (Indicator->IndicatorMaterialInstance->GetScalarParameterValue(FMaterialParameterInfo(TEXT("ConeHalfAngleDegrees")), MaterialMaskAngle))
			{
				TestEqual(*FString::Printf(TEXT("MID ConeHalfAngleDegrees param should match for shape kind %d"), static_cast<int32>(Case.Kind)),
					MaterialMaskAngle, Case.ExpectedMaskHalfAngleDegrees);
			}
		}
	}

	// (d) Show/Hide AC.
	FAbilityIndicatorShapeSpec ShowHideSpec;
	ShowHideSpec.Kind = EAbilityIndicatorShapeKind::CircleAtActor;
	ShowHideSpec.RangeUnits = 200.0f;
	Indicator->Show(ShowHideSpec, FLinearColor::White);
	TestTrue(TEXT("bIsVisible should be true after Show()"), Indicator->bIsVisible);
	TestTrue(TEXT("IndicatorMeshComponent should be visible after Show()"), Indicator->IndicatorMeshComponent->IsVisible());

	Indicator->Hide();
	TestFalse(TEXT("bIsVisible should be false after Hide()"), Indicator->bIsVisible);
	TestFalse(TEXT("IndicatorMeshComponent should not be visible after Hide()"), Indicator->IndicatorMeshComponent->IsVisible());

	// (e) Flash AC: Flash() sets bIsVisible true immediately (a real armed
	// FTimerHandle proves the auto-hide is scheduled), and the same timer-driven
	// clear path (ClearFlash(), reachable via this test's friend grant, mirroring
	// FKrowdKontrolAbilityVFXColourTest's direct ClearCastFlash() call) hides it.
	FAbilityIndicatorShapeSpec FlashSpec;
	FlashSpec.Kind = EAbilityIndicatorShapeKind::CircleAtActor;
	FlashSpec.RangeUnits = 150.0f;
	Indicator->Flash(FlashSpec, FLinearColor::White, 0.15f);
	TestTrue(TEXT("bIsVisible should be true immediately after Flash()"), Indicator->bIsVisible);
	TestTrue(TEXT("FlashTimerHandle should be armed (valid) immediately after Flash()"), Indicator->FlashTimerHandle.IsValid());
	if (UWorld* TestWorld = Indicator->GetWorld())
	{
		TestTrue(TEXT("FlashTimerHandle should be a pending timer after Flash()"),
			TestWorld->GetTimerManager().IsTimerActive(Indicator->FlashTimerHandle));
	}

	Indicator->ClearFlash();
	TestFalse(TEXT("bIsVisible should be false after the flash timer's ClearFlash() fires"), Indicator->bIsVisible);
	TestFalse(TEXT("IndicatorMeshComponent should not be visible after the flash timer's ClearFlash() fires"),
		Indicator->IndicatorMeshComponent->IsVisible());

	// (e2) EndPlay must clear a pending flash timer, not leave it dangling against a
	// component that may be about to be garbage-collected. UActorComponent::EndPlay()
	// asserts bHasBegunPlay - this test drives InitializeIndicatorVisual() directly
	// rather than through the actor lifecycle, so BeginPlay() must be called first to
	// satisfy that invariant (confirmed via a live Editor run: skipping this step
	// crashes the Editor with "Assertion failed: bHasBegunPlay"). BeginPlay() calling
	// InitializeIndicatorVisual() again is a harmless no-op, per its own idempotency
	// guard already proven above.
	Indicator->BeginPlay();
	Indicator->Flash(FlashSpec, FLinearColor::White, 0.15f);
	Indicator->EndPlay(EEndPlayReason::Destroyed);
	if (UWorld* TestWorld = Indicator->GetWorld())
	{
		TestFalse(TEXT("FlashTimerHandle should no longer be active after EndPlay"),
			TestWorld->GetTimerManager().IsTimerActive(Indicator->FlashTimerHandle));
	}

	// (f) Regression: an owner with no RootComponent must warn, not crash, and must
	// never create IndicatorMeshComponent. Mirrors
	// KrowdKontrolEnemyTypeIndicatorComponentTest.cpp case (g).
	AActor* RootlessOwner = World->SpawnActor<AActor>();
	if (!TestNotNull(TEXT("Rootless owner actor should spawn into the test World"), RootlessOwner))
	{
		return false;
	}
	TestNull(TEXT("Sanity: plain AActor should have no RootComponent by default"), RootlessOwner->GetRootComponent());

	UAbilityTargetingIndicatorComponent* RootlessIndicator = NewObject<UAbilityTargetingIndicatorComponent>(RootlessOwner);
	RootlessIndicator->RegisterComponent();

	// Count of 4 covers the two direct InitializeIndicatorVisual() calls below plus the
	// two more triggered indirectly by Show()/Flash() further down (Flash() calls
	// Show(), and Show() self-initializes) - every one of them re-hits the same
	// retryable, never-latched warning since bHasInitializedIndicatorVisual is
	// deliberately never set on this failure path.
	AddExpectedError(TEXT("found no Owner root component"), EAutomationExpectedErrorFlags::Contains, 4, false);
	RootlessIndicator->InitializeIndicatorVisual();
	TestNull(TEXT("IndicatorMeshComponent should stay null when the owner has no RootComponent"),
		ToRawPtr(RootlessIndicator->IndicatorMeshComponent));

	RootlessIndicator->InitializeIndicatorVisual();
	TestNull(TEXT("A second InitializeIndicatorVisual() call on a rootless owner should still leave IndicatorMeshComponent null"),
		ToRawPtr(RootlessIndicator->IndicatorMeshComponent));

	// Show()/Hide()/Flash() must stay crash-safe when IndicatorMeshComponent is (and
	// will always remain) null for this rootless owner. They only null-check
	// IndicatorMeshComponent before touching it, not before updating their own
	// reflected state, so bIsVisible still reports "visible" with nothing to render -
	// documenting that current behavior explicitly rather than leaving it unexercised.
	FAbilityIndicatorShapeSpec RootlessTestSpec;
	RootlessTestSpec.Kind = EAbilityIndicatorShapeKind::CircleAtActor;
	RootlessTestSpec.RangeUnits = 50.0f;
	RootlessIndicator->Show(RootlessTestSpec, FLinearColor::White);
	TestTrue(TEXT("Show() on a null-mesh rootless indicator should not crash and should still report bIsVisible true"),
		RootlessIndicator->bIsVisible);

	RootlessIndicator->Hide();
	TestFalse(TEXT("Hide() on a null-mesh rootless indicator should not crash and should clear bIsVisible"),
		RootlessIndicator->bIsVisible);

	RootlessIndicator->Flash(RootlessTestSpec, FLinearColor::White, 0.15f);
	TestTrue(TEXT("Flash() on a null-mesh rootless indicator should not crash and should still report bIsVisible true"),
		RootlessIndicator->bIsVisible);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
