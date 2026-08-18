// Confirms UAbilityCastVFXComponent (issue #67, PRD 13 REQ-2) tints its cast-flash
// light to each of the 5 abilities' own locked AbilityData colour, that every
// colour it ever renders is one of MISSION.md Hard Invariant 3's 5 reserved
// gameplay-information colours (never a 6th), and that a real
// UAbilityCastComponent::OnAbilityCastApplied broadcast drives it end-to-end
// through the same AddDynamic binding AFlatCamera3DPrototypePawn's constructor
// uses in production.
//
// Mirrors KrowdKontrolAbilityDataTest.cpp's pure per-slot mapping loop shape, and
// KrowdKontrolEnemyTypeIndicatorComponentTest.cpp's NewObject+RegisterComponent+
// explicit-Initialize call shape plus its reserved-colour non-collision idiom.
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the other
// KrowdKontrol.Unit.* tests.

#include "Misc/AutomationTest.h"
#include "AbilityCastVFXComponent.h"
#include "AbilityCastComponent.h"
#include "AbilityUnlockComponent.h"
#include "AbilityCooldownComponent.h"
#include "AbilityData.h"
#include "ReservedGameplayColours.h"
#include "EnemyBaseTestActor.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Pawn.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolAbilityVFXColourTest,
	"KrowdKontrol.Unit.AbilityVFXColour",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolAbilityVFXColourTest::RunTest(const FString& Parameters)
{
	const TArray<EAbilitySlot> AllSlots = { EAbilitySlot::Stun, EAbilitySlot::Sleep,
		EAbilitySlot::Root, EAbilitySlot::Fear, EAbilitySlot::Snare };
	const TArray<FLinearColor> AllReserved = ReservedGameplayColours::GetAll();

	// (a)+(b) Direct-call mapping and idempotency: for each of the 5 abilities, the cast flash's
	// colour matches AbilityData::Get(Slot).Colour exactly, and is always one of the
	// 5 reserved colours (Hard Invariant 3 regression guard - catches a hardcoded
	// literal creeping into this component independently of AbilityData's own
	// correctness, mirroring KrowdKontrolEnemyTypeIndicatorComponentTest.cpp part (e)).
	{
		UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
		if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
		{
			return false;
		}
		AActor* Owner = World->SpawnActor<AActor>();
		if (!TestNotNull(TEXT("AActor owner should spawn into the test World"), Owner))
		{
			return false;
		}
		USceneComponent* Root = NewObject<USceneComponent>(Owner);
		Root->RegisterComponent();
		Owner->SetRootComponent(Root);

		UAbilityCastVFXComponent* VFXComponent = NewObject<UAbilityCastVFXComponent>(Owner);
		VFXComponent->RegisterComponent();
		VFXComponent->InitializeCastVFX();

		if (!TestNotNull(TEXT("CastFlashLightComponent should exist after InitializeCastVFX"),
			ToRawPtr(VFXComponent->CastFlashLightComponent)))
		{
			return false;
		}

		// Idempotency regression: a second InitializeCastVFX() call must not create a
		// duplicate CastFlashLightComponent. Mirrors
		// KrowdKontrolEnemyTypeIndicatorComponentTest.cpp case (f).
		UPointLightComponent* FirstLightComponent = VFXComponent->CastFlashLightComponent;
		VFXComponent->InitializeCastVFX();
		TestTrue(TEXT("A second InitializeCastVFX() call should not create a duplicate CastFlashLightComponent"),
			VFXComponent->CastFlashLightComponent == FirstLightComponent);

		for (const EAbilitySlot Slot : AllSlots)
		{
			VFXComponent->HandleAbilityCastApplied(Slot, nullptr);

			const FLinearColor FlashColour = VFXComponent->CastFlashLightComponent->GetLightColor();
			TestTrue(*FString::Printf(TEXT("Cast VFX colour for slot %d should match its locked AbilityData colour"), static_cast<int32>(Slot)),
				FlashColour.Equals(AbilityData::Get(Slot).Colour, 0.01f));
			TestTrue(*FString::Printf(TEXT("Cast VFX colour for slot %d should be one of the 5 reserved gameplay colours"), static_cast<int32>(Slot)),
				AllReserved.ContainsByPredicate([FlashColour](const FLinearColor& Reserved) { return Reserved.Equals(FlashColour, 0.01f); }));
		}
	}

	// (c) Pre-initialization guard: HandleAbilityCastApplied firing before
	// InitializeCastVFX() has ever succeeded (CastFlashLightComponent still null)
	// must no-op, not crash.
	{
		UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
		if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
		{
			return false;
		}
		AActor* Owner = World->SpawnActor<AActor>();
		if (!TestNotNull(TEXT("AActor owner should spawn into the test World"), Owner))
		{
			return false;
		}

		UAbilityCastVFXComponent* UninitializedVFXComponent = NewObject<UAbilityCastVFXComponent>(Owner);
		UninitializedVFXComponent->RegisterComponent();
		UninitializedVFXComponent->HandleAbilityCastApplied(EAbilitySlot::Stun, nullptr); // should no-op, not crash
		TestNull(TEXT("HandleAbilityCastApplied before a successful InitializeCastVFX should stay a no-op"),
			ToRawPtr(UninitializedVFXComponent->CastFlashLightComponent));
	}

	// (d) Regression: an owner with no RootComponent must warn, not crash, and must
	// leave CastFlashLightComponent null - a later InitializeCastVFX() call should
	// still retry rather than being permanently skipped (bHasInitializedCastVFX must
	// not be set on this failure path). Mirrors
	// KrowdKontrolEnemyTypeIndicatorComponentTest.cpp case (g).
	{
		UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
		if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
		{
			return false;
		}
		AActor* RootlessOwner = World->SpawnActor<AActor>();
		if (!TestNotNull(TEXT("Rootless owner actor should spawn into the test World"), RootlessOwner))
		{
			return false;
		}
		TestNull(TEXT("Sanity: plain AActor should have no RootComponent by default"), RootlessOwner->GetRootComponent());

		UAbilityCastVFXComponent* RootlessVFXComponent = NewObject<UAbilityCastVFXComponent>(RootlessOwner);
		RootlessVFXComponent->RegisterComponent();

		AddExpectedError(TEXT("found no Owner root component"), EAutomationExpectedErrorFlags::Contains, 2, false);
		RootlessVFXComponent->InitializeCastVFX();
		TestNull(TEXT("CastFlashLightComponent should stay null when the owner has no RootComponent"),
			ToRawPtr(RootlessVFXComponent->CastFlashLightComponent));

		RootlessVFXComponent->InitializeCastVFX();
		TestNull(TEXT("A second InitializeCastVFX() call on a rootless owner should still leave CastFlashLightComponent null"),
			ToRawPtr(RootlessVFXComponent->CastFlashLightComponent));
	}

	// (e) End-to-end integration: a real UAbilityCastComponent::TryCastAbility success
	// broadcasts OnAbilityCastApplied, which - via the same AddDynamic binding
	// AFlatCamera3DPrototypePawn's constructor uses in production - drives the VFX
	// component's colour without the test calling HandleAbilityCastApplied directly.
	{
		UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
		if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
		{
			return false;
		}
		APawn* Owner = World->SpawnActor<APawn>();
		if (!TestNotNull(TEXT("APawn should spawn into the test World"), Owner))
		{
			return false;
		}
		USceneComponent* Root = NewObject<USceneComponent>(Owner);
		Root->RegisterComponent();
		Owner->SetRootComponent(Root);

		UAbilityUnlockComponent* UnlockComponent = NewObject<UAbilityUnlockComponent>(Owner);
		UnlockComponent->RegisterComponent();
		// Sleep is not unlocked by default (only Stun is) - NotifyLevelReached(2) is
		// the documented entry point that unlocks it, matching
		// UAbilityUnlockComponent.cpp's LevelToAbility map (2 -> Sleep).
		UnlockComponent->NotifyLevelReached(2);
		UAbilityCooldownComponent* CooldownComponent = NewObject<UAbilityCooldownComponent>(Owner);
		CooldownComponent->RegisterComponent();
		UAbilityCastComponent* CastComponent = NewObject<UAbilityCastComponent>(Owner);
		CastComponent->RegisterComponent();
		UAbilityCastVFXComponent* VFXComponent = NewObject<UAbilityCastVFXComponent>(Owner);
		VFXComponent->RegisterComponent();
		VFXComponent->InitializeCastVFX();
		CastComponent->OnAbilityCastApplied.AddDynamic(VFXComponent, &UAbilityCastVFXComponent::HandleAbilityCastApplied);

		AEnemyBaseTestActor* Enemy = World->SpawnActor<AEnemyBaseTestActor>();
		if (!TestNotNull(TEXT("AEnemyBaseTestActor should spawn into the test World"), Enemy))
		{
			return false;
		}
		// CastRangeUnits defaults to 1500 and Owner stays at the world origin, so this
		// offset stays comfortably in range without needing to touch CastRangeUnits.
		const FVector ExpectedFlashLocation(500.0f, 250.0f, 0.0f);
		Enemy->SetActorLocation(ExpectedFlashLocation);
		Enemy->TickCheckDetection(FVector::ZeroVector); // Idle -> Alert

		const bool bCastResult = CastComponent->TryCastAbility(EAbilitySlot::Sleep);
		TestTrue(TEXT("TryCastAbility should succeed with an eligible target in range"), bCastResult);
		TestTrue(TEXT("A real cast broadcast should drive the VFX colour to Sleep's locked Blue"),
			VFXComponent->CastFlashLightComponent->GetLightColor().Equals(AbilityData::Get(EAbilitySlot::Sleep).Colour, 0.01f));
		TestTrue(TEXT("The cast flash should be lit (non-zero intensity) immediately after a successful cast"),
			VFXComponent->CastFlashLightComponent->Intensity > 0.0f);
		TestTrue(TEXT("The cast flash should be positioned at the target enemy's actual location"),
			VFXComponent->CastFlashLightComponent->GetComponentLocation().Equals(ExpectedFlashLocation, 1.0f));

		// Regression (PR #155 review): CastFlashLightComponent must stay world-fixed
		// at the cast target even if the caster (player pawn) moves during the flash
		// window, not drag along with the pawn - it must not be attached to Owner's
		// root component.
		Owner->SetActorLocation(FVector(-2000.0f, 750.0f, 0.0f));
		TestTrue(TEXT("The cast flash should stay fixed at the target location after the caster pawn moves"),
			VFXComponent->CastFlashLightComponent->GetComponentLocation().Equals(ExpectedFlashLocation, 1.0f));

		// ClearCastFlash() (timer-driven in production) should zero the flash light's
		// intensity back off. Called directly here via the friend-grant idiom already
		// established in this PR (EnemyBase.h) since there's no existing precedent in
		// this test suite for advancing a live FTimerManager.
		VFXComponent->ClearCastFlash();
		TestTrue(TEXT("ClearCastFlash should zero the flash light's intensity"),
			VFXComponent->CastFlashLightComponent->Intensity == 0.0f);
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
