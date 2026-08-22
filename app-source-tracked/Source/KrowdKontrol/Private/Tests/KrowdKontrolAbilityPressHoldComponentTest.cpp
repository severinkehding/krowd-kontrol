// Confirms UAbilityPressHoldComponent (issue #265, docs/prd-cursor-aiming.md REQ-3)
// implements the locked press/hold indicator semantics: a press shows the shared
// UAbilityTargetingIndicatorComponent and casts unconditionally; holding past
// HoldThresholdSeconds - including while the ability is on cooldown - previews the
// indicator with no additional cast; releasing a held key never triggers a delayed
// cast; releasing before the hold threshold cancels the pending hold-preview timer
// cleanly.
//
// Mirrors KrowdKontrolAbilityCastComponentTest.cpp's standalone NewObject+
// RegisterComponent component scaffolding + per-case World isolation (AbilityCastComponent
// ::FindNearestValidTarget scans the whole World, and TickCheckDetection is only
// reachable via this test class's own friend grant on AEnemyBase - not from a free
// helper function, hence each case below is self-contained rather than sharing a
// helper), and KrowdKontrolAbilityTargetingIndicatorComponentTest.cpp's friend-driven
// timer-callback testing without a real tick loop.
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the other
// KrowdKontrol.Unit.* tests.

#include "Misc/AutomationTest.h"
#include "AbilityPressHoldComponent.h"
#include "AbilityCastComponent.h"
#include "AbilityCooldownComponent.h"
#include "AbilityUnlockComponent.h"
#include "AbilityTargetingIndicatorComponent.h"
#include "AbilityCastAppliedTestListener.h"
#include "AbilityData.h"
#include "EnemyBaseTestActor.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolAbilityPressHoldComponentTest,
	"KrowdKontrol.Unit.AbilityPressHoldComponent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolAbilityPressHoldComponentTest::RunTest(const FString& Parameters)
{
	// (a) Press triggers show-and-cast; HandlePressFlashComplete (simulating the flash
	// timer elapsing with no hold) hides the indicator.
	{
		UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
		if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
		{
			return false;
		}
		APawn* Owner = World->SpawnActor<APawn>();
		UAbilityUnlockComponent* UnlockComponent = NewObject<UAbilityUnlockComponent>(Owner);
		UnlockComponent->RegisterComponent();
		UAbilityCooldownComponent* CooldownComponent = NewObject<UAbilityCooldownComponent>(Owner);
		CooldownComponent->RegisterComponent();
		UAbilityCastComponent* CastComponent = NewObject<UAbilityCastComponent>(Owner);
		CastComponent->RegisterComponent();
		UAbilityTargetingIndicatorComponent* Indicator = NewObject<UAbilityTargetingIndicatorComponent>(Owner);
		Indicator->RegisterComponent();
		UAbilityPressHoldComponent* PressHold = NewObject<UAbilityPressHoldComponent>(Owner);
		PressHold->RegisterComponent();
		PressHold->CastComponent = CastComponent;
		PressHold->IndicatorComponent = Indicator;

		UAbilityCastAppliedTestListener* Listener = NewObject<UAbilityCastAppliedTestListener>();
		CastComponent->OnAbilityCastApplied.AddDynamic(Listener, &UAbilityCastAppliedTestListener::HandleAbilityCastApplied);

		AEnemyBaseTestActor* Enemy = World->SpawnActor<AEnemyBaseTestActor>();
		if (!TestNotNull(TEXT("AEnemyBaseTestActor should spawn into the test World"), Enemy))
		{
			return false;
		}
		Enemy->TickCheckDetection(FVector::ZeroVector); // Idle -> Alert, in range

		PressHold->HandleAbilityKeyPressed(EAbilitySlot::Stun);

		TestTrue(TEXT("(a) Indicator should be visible immediately after a press"), Indicator->bIsVisible);
		TestTrue(TEXT("(a) Indicator colour should match Stun's locked AbilityData colour"),
			Indicator->CurrentColour.Equals(AbilityData::Get(EAbilitySlot::Stun).Colour, 0.01f));
		TestEqual(TEXT("(a) OnAbilityCastApplied should have fired exactly once"), Listener->CallCount, 1);
		TestEqual(TEXT("(a) The target should be Controlled after the press-cast"),
			static_cast<uint8>(Enemy->GetEnemyState()), static_cast<uint8>(EEnemyState::Controlled));

		PressHold->HandlePressFlashComplete(EAbilitySlot::Stun);
		TestFalse(TEXT("(a) Indicator should be hidden once the flash timer elapses with no hold"), Indicator->bIsVisible);
	}

	// (b) Hold-without-release previews without casting again.
	{
		UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
		if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
		{
			return false;
		}
		APawn* Owner = World->SpawnActor<APawn>();
		UAbilityUnlockComponent* UnlockComponent = NewObject<UAbilityUnlockComponent>(Owner);
		UnlockComponent->RegisterComponent();
		UnlockComponent->NotifyLevelReached(2); // unlocks Sleep
		UAbilityCooldownComponent* CooldownComponent = NewObject<UAbilityCooldownComponent>(Owner);
		CooldownComponent->RegisterComponent();
		UAbilityCastComponent* CastComponent = NewObject<UAbilityCastComponent>(Owner);
		CastComponent->RegisterComponent();
		UAbilityTargetingIndicatorComponent* Indicator = NewObject<UAbilityTargetingIndicatorComponent>(Owner);
		Indicator->RegisterComponent();
		UAbilityPressHoldComponent* PressHold = NewObject<UAbilityPressHoldComponent>(Owner);
		PressHold->RegisterComponent();
		PressHold->CastComponent = CastComponent;
		PressHold->IndicatorComponent = Indicator;

		UAbilityCastAppliedTestListener* Listener = NewObject<UAbilityCastAppliedTestListener>();
		CastComponent->OnAbilityCastApplied.AddDynamic(Listener, &UAbilityCastAppliedTestListener::HandleAbilityCastApplied);

		AEnemyBaseTestActor* Enemy = World->SpawnActor<AEnemyBaseTestActor>();
		if (!TestNotNull(TEXT("AEnemyBaseTestActor should spawn into the test World"), Enemy))
		{
			return false;
		}
		Enemy->TickCheckDetection(FVector::ZeroVector); // Idle -> Alert, in range

		PressHold->HandleAbilityKeyPressed(EAbilitySlot::Sleep);
		TestEqual(TEXT("(b) The initial press should cast exactly once"), Listener->CallCount, 1);
		TestEqual(TEXT("(b) The target should be Controlled after the press-cast"),
			static_cast<uint8>(Enemy->GetEnemyState()), static_cast<uint8>(EEnemyState::Controlled));

		PressHold->BeginHoldPreview(EAbilitySlot::Sleep);
		TestTrue(TEXT("(b) bAbilityHoldPreviewActive should be true once the hold threshold fires while still held"),
			PressHold->bAbilityHoldPreviewActive[static_cast<int32>(EAbilitySlot::Sleep)]);

		PressHold->HandlePressFlashComplete(EAbilitySlot::Sleep);
		TestTrue(TEXT("(b) Indicator should stay visible - flash-complete is a no-op once hold-preview is active"),
			Indicator->bIsVisible);
		TestEqual(TEXT("(b) Entering hold-preview must not fire an additional cast"), Listener->CallCount, 1);
	}

	// (c) Hold-during-cooldown previews without casting.
	{
		UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
		if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
		{
			return false;
		}
		APawn* Owner = World->SpawnActor<APawn>();
		UAbilityUnlockComponent* UnlockComponent = NewObject<UAbilityUnlockComponent>(Owner);
		UnlockComponent->RegisterComponent();
		UnlockComponent->NotifyLevelReached(3); // unlocks Root
		UAbilityCooldownComponent* CooldownComponent = NewObject<UAbilityCooldownComponent>(Owner);
		CooldownComponent->RegisterComponent();
		UAbilityCastComponent* CastComponent = NewObject<UAbilityCastComponent>(Owner);
		CastComponent->RegisterComponent();
		UAbilityTargetingIndicatorComponent* Indicator = NewObject<UAbilityTargetingIndicatorComponent>(Owner);
		Indicator->RegisterComponent();
		UAbilityPressHoldComponent* PressHold = NewObject<UAbilityPressHoldComponent>(Owner);
		PressHold->RegisterComponent();
		PressHold->CastComponent = CastComponent;
		PressHold->IndicatorComponent = Indicator;

		UAbilityCastAppliedTestListener* Listener = NewObject<UAbilityCastAppliedTestListener>();
		CastComponent->OnAbilityCastApplied.AddDynamic(Listener, &UAbilityCastAppliedTestListener::HandleAbilityCastApplied);

		AEnemyBaseTestActor* Enemy = World->SpawnActor<AEnemyBaseTestActor>();
		if (!TestNotNull(TEXT("AEnemyBaseTestActor should spawn into the test World"), Enemy))
		{
			return false;
		}
		Enemy->TickCheckDetection(FVector::ZeroVector); // Idle -> Alert, in range

		CooldownComponent->TryStartCooldown(EAbilitySlot::Root); // pre-arm cooldown before the press

		PressHold->HandleAbilityKeyPressed(EAbilitySlot::Root);
		TestEqual(TEXT("(c) A cast blocked by cooldown must not fire OnAbilityCastApplied"), Listener->CallCount, 0);
		TestEqual(TEXT("(c) The target's state must be unchanged when the cast is blocked by cooldown"),
			static_cast<uint8>(Enemy->GetEnemyState()), static_cast<uint8>(EEnemyState::Alert));
		TestTrue(TEXT("(c) Indicator should still flash/preview unconditionally on press, independent of cast success"),
			Indicator->bIsVisible);
		TestTrue(TEXT("(c) Indicator colour should match Root's locked AbilityData colour"),
			Indicator->CurrentColour.Equals(AbilityData::Get(EAbilitySlot::Root).Colour, 0.01f));

		PressHold->BeginHoldPreview(EAbilitySlot::Root);
		TestTrue(TEXT("(c) bAbilityHoldPreviewActive should be true even though the cast itself was blocked"),
			PressHold->bAbilityHoldPreviewActive[static_cast<int32>(EAbilitySlot::Root)]);
		TestTrue(TEXT("(c) Indicator should still be visible after entering hold-preview"), Indicator->bIsVisible);

		PressHold->HandlePressFlashComplete(EAbilitySlot::Root);
		TestTrue(TEXT("(c) Indicator should still be visible after the flash-complete no-op"), Indicator->bIsVisible);
		TestEqual(TEXT("(c) No cast should ever fire throughout the hold-during-cooldown sequence"), Listener->CallCount, 0);
	}

	// (d) Releasing a held (hold-preview) key does not trigger a delayed cast.
	{
		UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
		if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
		{
			return false;
		}
		APawn* Owner = World->SpawnActor<APawn>();
		UAbilityUnlockComponent* UnlockComponent = NewObject<UAbilityUnlockComponent>(Owner);
		UnlockComponent->RegisterComponent();
		UnlockComponent->NotifyLevelReached(4); // unlocks Fear
		UAbilityCooldownComponent* CooldownComponent = NewObject<UAbilityCooldownComponent>(Owner);
		CooldownComponent->RegisterComponent();
		UAbilityCastComponent* CastComponent = NewObject<UAbilityCastComponent>(Owner);
		CastComponent->RegisterComponent();
		UAbilityTargetingIndicatorComponent* Indicator = NewObject<UAbilityTargetingIndicatorComponent>(Owner);
		Indicator->RegisterComponent();
		UAbilityPressHoldComponent* PressHold = NewObject<UAbilityPressHoldComponent>(Owner);
		PressHold->RegisterComponent();
		PressHold->CastComponent = CastComponent;
		PressHold->IndicatorComponent = Indicator;

		UAbilityCastAppliedTestListener* Listener = NewObject<UAbilityCastAppliedTestListener>();
		CastComponent->OnAbilityCastApplied.AddDynamic(Listener, &UAbilityCastAppliedTestListener::HandleAbilityCastApplied);

		AEnemyBaseTestActor* Enemy = World->SpawnActor<AEnemyBaseTestActor>();
		if (!TestNotNull(TEXT("AEnemyBaseTestActor should spawn into the test World"), Enemy))
		{
			return false;
		}
		Enemy->TickCheckDetection(FVector::ZeroVector); // Idle -> Alert, in range

		PressHold->HandleAbilityKeyPressed(EAbilitySlot::Fear);
		TestEqual(TEXT("(d) The initial press should cast exactly once"), Listener->CallCount, 1);

		PressHold->BeginHoldPreview(EAbilitySlot::Fear);
		PressHold->HandleAbilityKeyReleased(EAbilitySlot::Fear);

		TestFalse(TEXT("(d) Indicator should be hidden immediately on release from hold-preview"), Indicator->bIsVisible);
		TestEqual(TEXT("(d) Releasing a held key must never trigger a second/delayed cast"), Listener->CallCount, 1);
	}

	// (e) Regression: releasing before the hold threshold cancels the pending
	// hold-preview timer.
	{
		UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
		if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
		{
			return false;
		}
		APawn* Owner = World->SpawnActor<APawn>();
		UAbilityUnlockComponent* UnlockComponent = NewObject<UAbilityUnlockComponent>(Owner);
		UnlockComponent->RegisterComponent();
		UnlockComponent->NotifyLevelReached(5); // unlocks Snare
		UAbilityCooldownComponent* CooldownComponent = NewObject<UAbilityCooldownComponent>(Owner);
		CooldownComponent->RegisterComponent();
		UAbilityCastComponent* CastComponent = NewObject<UAbilityCastComponent>(Owner);
		CastComponent->RegisterComponent();
		UAbilityTargetingIndicatorComponent* Indicator = NewObject<UAbilityTargetingIndicatorComponent>(Owner);
		Indicator->RegisterComponent();
		UAbilityPressHoldComponent* PressHold = NewObject<UAbilityPressHoldComponent>(Owner);
		PressHold->RegisterComponent();
		PressHold->CastComponent = CastComponent;
		PressHold->IndicatorComponent = Indicator;

		AEnemyBaseTestActor* Enemy = World->SpawnActor<AEnemyBaseTestActor>();
		if (!TestNotNull(TEXT("AEnemyBaseTestActor should spawn into the test World"), Enemy))
		{
			return false;
		}
		Enemy->TickCheckDetection(FVector::ZeroVector); // Idle -> Alert, in range

		PressHold->HandleAbilityKeyPressed(EAbilitySlot::Snare);
		PressHold->HandleAbilityKeyReleased(EAbilitySlot::Snare); // fast tap, well before any timer fires

		if (UWorld* TestWorld = PressHold->GetWorld())
		{
			TestFalse(TEXT("(e) HoldThresholdTimerHandle should no longer be active after a fast-tap release"),
				TestWorld->GetTimerManager().IsTimerActive(PressHold->HoldThresholdTimerHandles[static_cast<int32>(EAbilitySlot::Snare)]));
		}
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
