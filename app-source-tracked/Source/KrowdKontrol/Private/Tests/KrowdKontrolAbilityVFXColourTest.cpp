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

	// (a)+(b) Direct-call mapping: for each of the 5 abilities, the cast flash's
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

	// (c) End-to-end integration: a real UAbilityCastComponent::TryCastAbility success
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
		Enemy->TickCheckDetection(FVector::ZeroVector); // Idle -> Alert

		const bool bCastResult = CastComponent->TryCastAbility(EAbilitySlot::Sleep);
		TestTrue(TEXT("TryCastAbility should succeed with an eligible target in range"), bCastResult);
		TestTrue(TEXT("A real cast broadcast should drive the VFX colour to Sleep's locked Blue"),
			VFXComponent->CastFlashLightComponent->GetLightColor().Equals(AbilityData::Get(EAbilitySlot::Sleep).Colour, 0.01f));
		TestTrue(TEXT("The cast flash should be lit (non-zero intensity) immediately after a successful cast"),
			VFXComponent->CastFlashLightComponent->Intensity > 0.0f);
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
