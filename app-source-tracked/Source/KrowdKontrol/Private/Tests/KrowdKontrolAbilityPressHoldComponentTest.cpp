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

		{
			const int32 Index = static_cast<int32>(EAbilitySlot::Stun);
			TestEqual(TEXT("(a) Hold-threshold timer should be armed at HoldThresholdSeconds"),
				World->GetTimerManager().GetTimerRate(PressHold->HoldThresholdTimerHandles[Index]), PressHold->HoldThresholdSeconds);
			TestEqual(TEXT("(a) Press-flash timer should be armed at PressFlashDurationSeconds"),
				World->GetTimerManager().GetTimerRate(PressHold->PressFlashTimerHandles[Index]), PressHold->PressFlashDurationSeconds);
			TestTrue(TEXT("(a) Design invariant: hold threshold must fire before press-flash completes to avoid flicker"),
				PressHold->HoldThresholdSeconds < PressHold->PressFlashDurationSeconds);
		}

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

	// (f) Regression: a second press on the same slot after release, while the first
	// press's flash timer is still conceptually pending, must not leak or double-fire.
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
		PressHold->HandleAbilityKeyReleased(EAbilitySlot::Stun); // before any timer fires
		PressHold->HandleAbilityKeyPressed(EAbilitySlot::Stun); // second press, same slot

		// Only the first press actually casts - the first press's TryStartCooldown blocks
		// the second, same as case (c)'s hold-during-cooldown scenario. The indicator/timer
		// machinery below must still behave correctly on the second press regardless of
		// whether its cast was blocked, since Show()/SetTimer fire unconditionally on press.
		TestEqual(TEXT("(f) Only the first press should cast - the second is blocked by the cooldown the first press started"),
			Listener->CallCount, 1);
		TestTrue(TEXT("(f) Indicator should still be visible from the second press"), Indicator->bIsVisible);

		PressHold->HandlePressFlashComplete(EAbilitySlot::Stun);
		TestFalse(TEXT("(f) Indicator should hide once the (re-armed) flash timer completes"), Indicator->bIsVisible);
	}

	// (g) Invalid-index guard branches in all four handlers no-op cleanly rather than
	// crashing or mutating state - defensive against EAbilitySlot::Count/NumAbilitySlots
	// drifting out of sync.
	{
		UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
		if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
		{
			return false;
		}
		APawn* Owner = World->SpawnActor<APawn>();
		UAbilityPressHoldComponent* PressHold = NewObject<UAbilityPressHoldComponent>(Owner);
		PressHold->RegisterComponent();

		const EAbilitySlot OutOfRangeSlot = static_cast<EAbilitySlot>(UAbilityPressHoldComponent::NumAbilitySlots);

		PressHold->HandleAbilityKeyPressed(OutOfRangeSlot);
		PressHold->HandleAbilityKeyReleased(OutOfRangeSlot);
		PressHold->HandlePressFlashComplete(OutOfRangeSlot);
		PressHold->BeginHoldPreview(OutOfRangeSlot);

		TestTrue(TEXT("(g) Out-of-range slot calls should no-op without crashing or growing tracked state"),
			PressHold->bAbilityHoldPreviewActive.Num() == UAbilityPressHoldComponent::NumAbilitySlots);
	}

	// (h) Cursor-aimed press (issue #257): a supplied target location shows a
	// CircleAtCursor indicator at that location (not CircleAtActor at the owner) and
	// routes the cast through TryCastThrownAbilityAtLocation - an enemy well inside
	// ThrownCircleLandingRadiusUnits of the supplied location is Controlled; a second,
	// otherwise-identical enemy placed outside that radius is not, proving the
	// component actually routed to the new thrown-ability cast path rather than merely
	// compiling against it.
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

		AEnemyBaseTestActor* InCircleEnemy = World->SpawnActor<AEnemyBaseTestActor>();
		AEnemyBaseTestActor* OutOfCircleEnemy = World->SpawnActor<AEnemyBaseTestActor>();
		if (!TestNotNull(TEXT("(h) In-circle AEnemyBaseTestActor should spawn"), InCircleEnemy)
			|| !TestNotNull(TEXT("(h) Out-of-circle AEnemyBaseTestActor should spawn"), OutOfCircleEnemy))
		{
			return false;
		}
		InCircleEnemy->TickCheckDetection(FVector::ZeroVector); // Idle -> Alert
		OutOfCircleEnemy->TickCheckDetection(FVector::ZeroVector); // Idle -> Alert
		const FVector SomeLocation(500.0f, 0.0f, 0.0f);
		InCircleEnemy->SetActorLocation(SomeLocation + FVector(100.0f, 0.0f, 0.0f)); // inside the 400-unit radius
		OutOfCircleEnemy->SetActorLocation(SomeLocation + FVector(700.0f, 0.0f, 0.0f)); // outside the 400-unit radius

		PressHold->HandleAbilityKeyPressed(EAbilitySlot::Sleep, true, SomeLocation);

		TestEqual(TEXT("(h) Indicator shape kind should be CircleAtCursor when a target location is supplied"),
			static_cast<uint8>(Indicator->CurrentShapeSpec.Kind), static_cast<uint8>(EAbilityIndicatorShapeKind::CircleAtCursor));
		TestEqual(TEXT("(h) Indicator origin should be the supplied target location, not the owner's location"),
			Indicator->CurrentShapeSpec.Origin, SomeLocation);
		TestEqual(TEXT("(h) The in-circle enemy should be Controlled"),
			static_cast<uint8>(InCircleEnemy->GetEnemyState()), static_cast<uint8>(EEnemyState::Controlled));
		TestEqual(TEXT("(h) The out-of-circle enemy should be left untouched"),
			static_cast<uint8>(OutOfCircleEnemy->GetEnemyState()), static_cast<uint8>(EEnemyState::Alert));
	}

	// (i) Cursor-aimed press via Stun (issue #256): mirrors case (h) exactly, proving
	// AFlatCamera3DPrototypePawn::CastStunAbility()'s new cursor-forwarding call
	// actually results in CircleAtCursor + AoE routing at this component level, not
	// just that the pawn compiles against the API. Stun is unlocked by default
	// (unlike Sleep), so no NotifyLevelReached() call is needed.
	{
		UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
		if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
		{
			return false;
		}
		APawn* Owner = World->SpawnActor<APawn>();
		UAbilityUnlockComponent* UnlockComponent = NewObject<UAbilityUnlockComponent>(Owner);
		UnlockComponent->RegisterComponent(); // Stun is unlocked by default - no NotifyLevelReached() needed
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

		AEnemyBaseTestActor* InCircleEnemy = World->SpawnActor<AEnemyBaseTestActor>();
		AEnemyBaseTestActor* OutOfCircleEnemy = World->SpawnActor<AEnemyBaseTestActor>();
		if (!TestNotNull(TEXT("(i) In-circle AEnemyBaseTestActor should spawn"), InCircleEnemy)
			|| !TestNotNull(TEXT("(i) Out-of-circle AEnemyBaseTestActor should spawn"), OutOfCircleEnemy))
		{
			return false;
		}
		InCircleEnemy->TickCheckDetection(FVector::ZeroVector); // Idle -> Alert
		OutOfCircleEnemy->TickCheckDetection(FVector::ZeroVector); // Idle -> Alert
		const FVector SomeLocation(500.0f, 0.0f, 0.0f);
		InCircleEnemy->SetActorLocation(SomeLocation + FVector(100.0f, 0.0f, 0.0f)); // inside the 400-unit radius
		OutOfCircleEnemy->SetActorLocation(SomeLocation + FVector(700.0f, 0.0f, 0.0f)); // outside the 400-unit radius

		PressHold->HandleAbilityKeyPressed(EAbilitySlot::Stun, true, SomeLocation);

		TestEqual(TEXT("(i) Indicator shape kind should be CircleAtCursor when a target location is supplied"),
			static_cast<uint8>(Indicator->CurrentShapeSpec.Kind), static_cast<uint8>(EAbilityIndicatorShapeKind::CircleAtCursor));
		TestEqual(TEXT("(i) Indicator origin should be the supplied target location, not the owner's location"),
			Indicator->CurrentShapeSpec.Origin, SomeLocation);
		TestEqual(TEXT("(i) The in-circle enemy should be Controlled"),
			static_cast<uint8>(InCircleEnemy->GetEnemyState()), static_cast<uint8>(EEnemyState::Controlled));
		TestEqual(TEXT("(i) The out-of-circle enemy should be left untouched"),
			static_cast<uint8>(OutOfCircleEnemy->GetEnemyState()), static_cast<uint8>(EEnemyState::Alert));
	}

	// (j) Cursor-aimed press via Root (issue #255): proves the Line-target branch -
	// Indicator gets EAbilityIndicatorShapeKind::Line (not CircleAtCursor), Origin is the
	// Owner's location (not the cursor point - a line starts at the robot), and the cast
	// actually routes through TryCastLineAbilityTowardLocation (an enemy directly on the
	// line is Controlled; an equidistant enemy off the line is not).
	{
		UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
		if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World)) { return false; }
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

		AEnemyBaseTestActor* OnLineEnemy = World->SpawnActor<AEnemyBaseTestActor>();
		AEnemyBaseTestActor* OffLineEnemy = World->SpawnActor<AEnemyBaseTestActor>();
		if (!TestNotNull(TEXT("(j) On-line AEnemyBaseTestActor should spawn"), OnLineEnemy)
			|| !TestNotNull(TEXT("(j) Off-line AEnemyBaseTestActor should spawn"), OffLineEnemy))
		{
			return false;
		}
		OnLineEnemy->TickCheckDetection(FVector::ZeroVector); // Idle -> Alert
		OffLineEnemy->TickCheckDetection(FVector::ZeroVector); // Idle -> Alert
		// Owner defaults to World origin; cursor straight down +X puts the line along +X.
		const FVector CursorLocation(500.0f, 0.0f, 0.0f);
		OnLineEnemy->SetActorLocation(FVector(400.0f, 0.0f, 0.0f)); // on the line, well within LineHitWidthUnits
		OffLineEnemy->SetActorLocation(FVector(400.0f, CastComponent->LineHitWidthUnits * 3.0f, 0.0f)); // same X, far off the line

		PressHold->HandleAbilityKeyPressed(EAbilitySlot::Root, true, CursorLocation);

		TestEqual(TEXT("(j) Indicator shape kind should be Line when a target location is supplied for a Line-target ability"),
			static_cast<uint8>(Indicator->CurrentShapeSpec.Kind), static_cast<uint8>(EAbilityIndicatorShapeKind::Line));
		TestEqual(TEXT("(j) Indicator origin should be the Owner's location, not the cursor point"),
			Indicator->CurrentShapeSpec.Origin, Owner->GetActorLocation());
		TestEqual(TEXT("(j) Indicator facing rotation should point toward the cursor"),
			Indicator->CurrentShapeSpec.FacingRotation, (CursorLocation - Owner->GetActorLocation()).Rotation());
		TestEqual(TEXT("(j) Indicator range should be the full Long tier, not clamped to the cursor's own distance"),
			Indicator->CurrentShapeSpec.RangeUnits, CastComponent->LongThrowRangeUnits);
		TestEqual(TEXT("(j) The on-line enemy should be Controlled"),
			static_cast<uint8>(OnLineEnemy->GetEnemyState()), static_cast<uint8>(EEnemyState::Controlled));
		TestEqual(TEXT("(j) The off-line enemy should be left untouched"),
			static_cast<uint8>(OffLineEnemy->GetEnemyState()), static_cast<uint8>(EEnemyState::Alert));
	}

	// (k) Cursor-aimed press via Snare (issue #254): proves the Cone-target branch -
	// Indicator gets EAbilityIndicatorShapeKind::Cone (not CircleAtCursor), Origin is the
	// Owner's location (a cone's apex is at the robot), FacingRotation points toward the
	// cursor, RangeUnits/ConeFullAngleDegrees match the cast component's own values, and
	// the cast actually routes through TryCastConeAbilityTowardLocation (an in-cone enemy
	// is Controlled; an enemy directly behind the owner, outside the cone, is not).
	{
		UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
		if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World)) { return false; }
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

		AEnemyBaseTestActor* InConeEnemy = World->SpawnActor<AEnemyBaseTestActor>();
		AEnemyBaseTestActor* BehindOwnerEnemy = World->SpawnActor<AEnemyBaseTestActor>();
		if (!TestNotNull(TEXT("(k) In-cone AEnemyBaseTestActor should spawn"), InConeEnemy)
			|| !TestNotNull(TEXT("(k) Behind-owner AEnemyBaseTestActor should spawn"), BehindOwnerEnemy))
		{
			return false;
		}
		InConeEnemy->TickCheckDetection(FVector::ZeroVector); // Idle -> Alert
		BehindOwnerEnemy->TickCheckDetection(FVector::ZeroVector); // Idle -> Alert
		// Owner defaults to World origin; cursor straight down +X aims the cone along +X.
		const FVector CursorLocation(500.0f, 0.0f, 0.0f);
		InConeEnemy->SetActorLocation(FVector(400.0f, 0.0f, 0.0f)); // on the cone centreline, well in range
		BehindOwnerEnemy->SetActorLocation(FVector(-400.0f, 0.0f, 0.0f)); // directly behind the owner, outside any sub-360 cone

		PressHold->HandleAbilityKeyPressed(EAbilitySlot::Snare, true, CursorLocation);

		TestEqual(TEXT("(k) Indicator shape kind should be Cone when a target location is supplied for a Cone-target ability"),
			static_cast<uint8>(Indicator->CurrentShapeSpec.Kind), static_cast<uint8>(EAbilityIndicatorShapeKind::Cone));
		TestEqual(TEXT("(k) Indicator origin should be the Owner's location, not the cursor point"),
			Indicator->CurrentShapeSpec.Origin, Owner->GetActorLocation());
		TestEqual(TEXT("(k) Indicator facing rotation should point toward the cursor"),
			Indicator->CurrentShapeSpec.FacingRotation, (CursorLocation - Owner->GetActorLocation()).Rotation());
		TestEqual(TEXT("(k) Indicator range should be the Medium tier"),
			Indicator->CurrentShapeSpec.RangeUnits, CastComponent->MediumThrowRangeUnits);
		TestEqual(TEXT("(k) Indicator cone full angle should match the cast component's ConeFullAngleDegrees"),
			Indicator->CurrentShapeSpec.ConeFullAngleDegrees, CastComponent->ConeFullAngleDegrees);
		TestEqual(TEXT("(k) The in-cone enemy should be Controlled"),
			static_cast<uint8>(InConeEnemy->GetEnemyState()), static_cast<uint8>(EEnemyState::Controlled));
		TestEqual(TEXT("(k) The behind-owner enemy should be left untouched"),
			static_cast<uint8>(BehindOwnerEnemy->GetEnemyState()), static_cast<uint8>(EEnemyState::Alert));
	}

	// (l) No-cursor press via Fear (issue #253): proves the SelfCircle-target branch -
	// Indicator gets EAbilityIndicatorShapeKind::CircleAtActor (not the generic
	// CastRangeUnits fallback), Origin is the Owner's location, RangeUnits matches
	// CastComponent->SelfCircleRadiusUnits (not CastRangeUnits), and the cast actually
	// routes through TryCastSelfCircleAbility - TWO enemies both inside the circle both
	// end up Controlled from the single press, proving multi-target dispatch (unlike
	// the old single-target TryCastAbility case (d) exercised before this branch existed).
	{
		UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
		if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World)) { return false; }
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

		AEnemyBaseTestActor* FirstEnemy = World->SpawnActor<AEnemyBaseTestActor>();
		AEnemyBaseTestActor* SecondEnemy = World->SpawnActor<AEnemyBaseTestActor>();
		if (!TestNotNull(TEXT("(l) First AEnemyBaseTestActor should spawn"), FirstEnemy)
			|| !TestNotNull(TEXT("(l) Second AEnemyBaseTestActor should spawn"), SecondEnemy))
		{
			return false;
		}
		FirstEnemy->TickCheckDetection(FVector::ZeroVector); // Idle -> Alert
		SecondEnemy->TickCheckDetection(FVector::ZeroVector); // Idle -> Alert
		// Owner defaults to World origin; both enemies well inside SelfCircleRadiusUnits.
		FirstEnemy->SetActorLocation(FVector(100.0f, 0.0f, 0.0f));
		SecondEnemy->SetActorLocation(FVector(-100.0f, 0.0f, 0.0f));

		PressHold->HandleAbilityKeyPressed(EAbilitySlot::Fear); // no cursor location, as CastFearAbility() always calls it

		TestEqual(TEXT("(l) Indicator shape kind should be CircleAtActor for a SelfCircle-target ability"),
			static_cast<uint8>(Indicator->CurrentShapeSpec.Kind), static_cast<uint8>(EAbilityIndicatorShapeKind::CircleAtActor));
		TestEqual(TEXT("(l) Indicator origin should be the Owner's location"),
			Indicator->CurrentShapeSpec.Origin, Owner->GetActorLocation());
		TestEqual(TEXT("(l) Indicator range should be SelfCircleRadiusUnits, not CastRangeUnits"),
			Indicator->CurrentShapeSpec.RangeUnits, CastComponent->SelfCircleRadiusUnits);
		TestEqual(TEXT("(l) The first enemy should be Controlled"),
			static_cast<uint8>(FirstEnemy->GetEnemyState()), static_cast<uint8>(EEnemyState::Controlled));
		TestEqual(TEXT("(l) The second enemy should be Controlled"),
			static_cast<uint8>(SecondEnemy->GetEnemyState()), static_cast<uint8>(EEnemyState::Controlled));
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
