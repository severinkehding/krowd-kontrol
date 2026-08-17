// Confirms UAbilityUnlockComponent (issue #69) starts a run with only Stun unlocked
// and unlocks Sleep/Root/Fear/Snare in the correct order and timing as
// NotifyLevelReached() is called for levels 2-5, firing OnAbilityUnlocked exactly once
// per ability, with level-1/out-of-range calls as safe no-ops.
//
// Uses a bare NewObject(), no UWorld needed: NotifyLevelReached/IsAbilityUnlocked call
// neither GetWorld() nor GetOwner(), mirroring KrowdKontrolAbilityCooldownTest.
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the other
// KrowdKontrol.Unit.* tests.

#include "Misc/AutomationTest.h"
#include "AbilityUnlockComponent.h"
#include "AbilityUnlockTestListener.h"
#include "AbilityCooldownTrayWidget.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"
#include "Blueprint/UserWidget.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolAbilityUnlockSequenceTest,
	"KrowdKontrol.Unit.AbilityUnlockSequence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolAbilityUnlockSequenceTest::RunTest(const FString& Parameters)
{
	UAbilityUnlockComponent* Component = NewObject<UAbilityUnlockComponent>();
	if (!TestNotNull(TEXT("UAbilityUnlockComponent should construct"), Component))
	{
		return false;
	}

	UAbilityUnlockTestListener* Listener = NewObject<UAbilityUnlockTestListener>();
	Component->OnAbilityUnlocked.AddDynamic(Listener, &UAbilityUnlockTestListener::HandleAbilityUnlocked);

	// (a) Run-start state: only Stun unlocked, no events fired yet.
	TestTrue(TEXT("Stun should be unlocked at run start"), Component->IsAbilityUnlocked(EAbilitySlot::Stun));
	TestFalse(TEXT("Sleep should be locked at run start"), Component->IsAbilityUnlocked(EAbilitySlot::Sleep));
	TestFalse(TEXT("Root should be locked at run start"), Component->IsAbilityUnlocked(EAbilitySlot::Root));
	TestFalse(TEXT("Fear should be locked at run start"), Component->IsAbilityUnlocked(EAbilitySlot::Fear));
	TestFalse(TEXT("Snare should be locked at run start"), Component->IsAbilityUnlocked(EAbilitySlot::Snare));
	TestEqual(TEXT("No unlock events should have fired at run start"), Listener->UnlockedOrder.Num(), 0);

	// (b) Level 2 unlocks Sleep, and only Sleep - none early.
	Component->NotifyLevelReached(2);
	TestTrue(TEXT("Sleep should be unlocked after NotifyLevelReached(2)"), Component->IsAbilityUnlocked(EAbilitySlot::Sleep));
	TestFalse(TEXT("Root should still be locked after NotifyLevelReached(2)"), Component->IsAbilityUnlocked(EAbilitySlot::Root));
	TestFalse(TEXT("Fear should still be locked after NotifyLevelReached(2)"), Component->IsAbilityUnlocked(EAbilitySlot::Fear));
	TestFalse(TEXT("Snare should still be locked after NotifyLevelReached(2)"), Component->IsAbilityUnlocked(EAbilitySlot::Snare));
	TestEqual(TEXT("Only Sleep's unlock event should have fired so far"), Listener->UnlockedOrder.Num(), 1);
	if (Listener->UnlockedOrder.Num() > 0)
	{
		TestEqual(TEXT("The first unlock event should be Sleep"), static_cast<uint8>(Listener->UnlockedOrder[0]), static_cast<uint8>(EAbilitySlot::Sleep));
	}

	// (c) Level 3 unlocks Root, and only Root newly - Sleep stays unlocked, Fear/Snare
	// stay locked.
	Component->NotifyLevelReached(3);
	TestTrue(TEXT("Root should be unlocked after NotifyLevelReached(3)"), Component->IsAbilityUnlocked(EAbilitySlot::Root));
	TestTrue(TEXT("Sleep should remain unlocked after NotifyLevelReached(3)"), Component->IsAbilityUnlocked(EAbilitySlot::Sleep));
	TestFalse(TEXT("Fear should still be locked after NotifyLevelReached(3)"), Component->IsAbilityUnlocked(EAbilitySlot::Fear));
	TestFalse(TEXT("Snare should still be locked after NotifyLevelReached(3)"), Component->IsAbilityUnlocked(EAbilitySlot::Snare));

	// (d) Level 4 unlocks Fear.
	Component->NotifyLevelReached(4);
	TestTrue(TEXT("Fear should be unlocked after NotifyLevelReached(4)"), Component->IsAbilityUnlocked(EAbilitySlot::Fear));
	TestFalse(TEXT("Snare should still be locked after NotifyLevelReached(4)"), Component->IsAbilityUnlocked(EAbilitySlot::Snare));

	// (e) Level 5 unlocks Snare. After all four calls, the recorded order must be
	// exactly Sleep, Root, Fear, Snare.
	Component->NotifyLevelReached(5);
	TestTrue(TEXT("Snare should be unlocked after NotifyLevelReached(5)"), Component->IsAbilityUnlocked(EAbilitySlot::Snare));

	TArray<EAbilitySlot> ExpectedOrder = { EAbilitySlot::Sleep, EAbilitySlot::Root, EAbilitySlot::Fear, EAbilitySlot::Snare };
	if (TestEqual(TEXT("Exactly 4 unlock events should have fired, in order"), Listener->UnlockedOrder.Num(), ExpectedOrder.Num()))
	{
		for (int32 Index = 0; Index < ExpectedOrder.Num(); ++Index)
		{
			TestEqual(*FString::Printf(TEXT("Unlock event %d should match the expected order"), Index),
				static_cast<uint8>(Listener->UnlockedOrder[Index]), static_cast<uint8>(ExpectedOrder[Index]));
		}
	}

	// (f) OnAbilityUnlocked fires exactly once per ability - a duplicate
	// NotifyLevelReached call for an already-processed level is a no-op.
	Component->NotifyLevelReached(2);
	TestEqual(TEXT("A repeat NotifyLevelReached(2) should not fire a duplicate event"), Listener->UnlockedOrder.Num(), 4);
	TestTrue(TEXT("Sleep should remain unlocked after the repeat call"), Component->IsAbilityUnlocked(EAbilitySlot::Sleep));

	// (g) Level 1 and out-of-range levels are safe no-ops on a fresh component.
	UAbilityUnlockComponent* FreshComponent = NewObject<UAbilityUnlockComponent>();
	if (!TestNotNull(TEXT("A fresh UAbilityUnlockComponent should construct"), FreshComponent))
	{
		return false;
	}
	UAbilityUnlockTestListener* FreshListener = NewObject<UAbilityUnlockTestListener>();
	FreshComponent->OnAbilityUnlocked.AddDynamic(FreshListener, &UAbilityUnlockTestListener::HandleAbilityUnlocked);

	FreshComponent->NotifyLevelReached(1);
	FreshComponent->NotifyLevelReached(6);
	TestEqual(TEXT("Level 1 and out-of-range NotifyLevelReached calls should fire no events"), FreshListener->UnlockedOrder.Num(), 0);
	TestTrue(TEXT("Stun should still be the only unlocked ability on the fresh component"), FreshComponent->IsAbilityUnlocked(EAbilitySlot::Stun));
	TestFalse(TEXT("Sleep should still be locked on the fresh component"), FreshComponent->IsAbilityUnlocked(EAbilitySlot::Sleep));
	TestFalse(TEXT("Root should still be locked on the fresh component"), FreshComponent->IsAbilityUnlocked(EAbilitySlot::Root));
	TestFalse(TEXT("Fear should still be locked on the fresh component"), FreshComponent->IsAbilityUnlocked(EAbilitySlot::Fear));
	TestFalse(TEXT("Snare should still be locked on the fresh component"), FreshComponent->IsAbilityUnlocked(EAbilitySlot::Snare));

	// (h) Reaching a later level without the intermediate ones only unlocks that level's
	// ability - no catch-up. Catch-up unlocking is explicitly out of scope (issue #69).
	UAbilityUnlockComponent* SkipComponent = NewObject<UAbilityUnlockComponent>();
	if (!TestNotNull(TEXT("A skip-ahead UAbilityUnlockComponent should construct"), SkipComponent))
	{
		return false;
	}
	UAbilityUnlockTestListener* SkipListener = NewObject<UAbilityUnlockTestListener>();
	SkipComponent->OnAbilityUnlocked.AddDynamic(SkipListener, &UAbilityUnlockTestListener::HandleAbilityUnlocked);

	SkipComponent->NotifyLevelReached(5);
	TestTrue(TEXT("Snare should unlock from a direct NotifyLevelReached(5)"), SkipComponent->IsAbilityUnlocked(EAbilitySlot::Snare));
	TestFalse(TEXT("Sleep should NOT catch-up unlock from a level-5 signal alone"), SkipComponent->IsAbilityUnlocked(EAbilitySlot::Sleep));
	TestFalse(TEXT("Root should NOT catch-up unlock from a level-5 signal alone"), SkipComponent->IsAbilityUnlocked(EAbilitySlot::Root));
	TestFalse(TEXT("Fear should NOT catch-up unlock from a level-5 signal alone"), SkipComponent->IsAbilityUnlocked(EAbilitySlot::Fear));
	TestEqual(TEXT("Exactly one unlock event should fire for the skipped-ahead call"), SkipListener->UnlockedOrder.Num(), 1);

	// (i) production wiring: BindAbilityUnlockComponent() must initialize the tray's
	// locked-slot visuals from the component's unlock state and keep them live via
	// OnAbilityUnlocked - the consumer path issue #69's acceptance criteria describe
	// ("locked/uncastable... not shown as an active, usable ability-tray slot").
	// CreateWidget needs a real UWorld (same pattern/rationale as
	// KrowdKontrolAbilityCooldownTrayWidgetTest.cpp - works under -nullrhi).
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
	{
		return false;
	}
	UAbilityCooldownTrayWidget* Tray =
		CreateWidget<UAbilityCooldownTrayWidget>(World, UAbilityCooldownTrayWidget::StaticClass());
	if (!TestNotNull(TEXT("Tray widget should construct via CreateWidget"), Tray))
	{
		return false;
	}
	UAbilityUnlockComponent* BoundComponent = NewObject<UAbilityUnlockComponent>();
	if (!TestNotNull(TEXT("A bind-target UAbilityUnlockComponent should construct"), BoundComponent))
	{
		return false;
	}

	Tray->BindAbilityUnlockComponent(BoundComponent);
	TestFalse(TEXT("Stun's tray slot should be unlocked immediately after binding (unlocked at run start)"),
		Tray->IsSlotLocked(EAbilitySlot::Stun));
	TestTrue(TEXT("Sleep's tray slot should be locked immediately after binding"),
		Tray->IsSlotLocked(EAbilitySlot::Sleep));
	TestTrue(TEXT("Root's tray slot should be locked immediately after binding"),
		Tray->IsSlotLocked(EAbilitySlot::Root));
	TestTrue(TEXT("Fear's tray slot should be locked immediately after binding"),
		Tray->IsSlotLocked(EAbilitySlot::Fear));
	TestTrue(TEXT("Snare's tray slot should be locked immediately after binding"),
		Tray->IsSlotLocked(EAbilitySlot::Snare));

	BoundComponent->NotifyLevelReached(2);
	TestFalse(TEXT("Sleep's tray slot should unlock live when level 2 is reached"),
		Tray->IsSlotLocked(EAbilitySlot::Sleep));
	TestTrue(TEXT("Root's tray slot should stay locked after only level 2"),
		Tray->IsSlotLocked(EAbilitySlot::Root));

	// Re-binding must not stack duplicate subscriptions (AddUniqueDynamic) - a second
	// bind then an unlock should still flip exactly the expected slot, no side effects.
	Tray->BindAbilityUnlockComponent(BoundComponent);
	TestFalse(TEXT("Re-binding should re-read state: Sleep stays unlocked"),
		Tray->IsSlotLocked(EAbilitySlot::Sleep));
	BoundComponent->NotifyLevelReached(3);
	TestFalse(TEXT("Root's tray slot should unlock live when level 3 is reached (post re-bind)"),
		Tray->IsSlotLocked(EAbilitySlot::Root));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
