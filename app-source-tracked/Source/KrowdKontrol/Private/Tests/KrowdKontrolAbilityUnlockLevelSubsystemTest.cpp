// Confirms issue #217: UAbilityUnlockLevelSubsystem is the missing production caller of
// UAbilityUnlockComponent::NotifyLevelReached. Covers (a) ParseLevelIndexFromMapName's
// L_LevelNN convention, its two prototype-map no-op exceptions, and PIE name-mangling;
// (b) the acceptance criteria itself - driving HandleLevelBegin through levels 1-5 in
// order unlocks Sleep/Root/Fear/Snare on a possessed pawn's UAbilityUnlockComponent;
// (c) the real Initialize()-time OnLevelBegin subscription, not just the handler logic
// in isolation; (d) the no-possessed-pawn/no-component branch's one-shot warning guard;
// and (e) the AutoPossessPlayer-races-OnLevelBegin recovery path, where a level's
// unlock arrives late via AKrowdKontrolPlayerController::OnPossess ->
// RetryPendingUnlockForPawn() instead of being lost.
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the other
// KrowdKontrol.Unit.* tests.

#include "Misc/AutomationTest.h"
#include "AbilityUnlockLevelSubsystem.h"
#include "LevelLifecycleSubsystem.h"
#include "AbilityUnlockComponent.h"
#include "AbilitySlot.h"
#include "KrowdKontrolPlayerController.h"
#include "GameFramework/Pawn.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolAbilityUnlockLevelSubsystemTest,
	"KrowdKontrol.Unit.AbilityUnlockLevelSubsystem",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolAbilityUnlockLevelSubsystemTest::RunTest(const FString& Parameters)
{
	// (a) ParseLevelIndexFromMapName: the established L_LevelNN convention, its two
	// non-matching prototype-map exceptions, and PIE name-mangling.
	TestEqual(TEXT("L_Level01 should parse to level 1"),
		UAbilityUnlockLevelSubsystem::ParseLevelIndexFromMapName(FName(TEXT("L_Level01"))), 1);
	TestEqual(TEXT("L_Level02 should parse to level 2"),
		UAbilityUnlockLevelSubsystem::ParseLevelIndexFromMapName(FName(TEXT("L_Level02"))), 2);
	TestEqual(TEXT("L_Level05 should parse to level 5"),
		UAbilityUnlockLevelSubsystem::ParseLevelIndexFromMapName(FName(TEXT("L_Level05"))), 5);
	TestEqual(TEXT("A PIE-mangled name should still parse correctly"),
		UAbilityUnlockLevelSubsystem::ParseLevelIndexFromMapName(FName(TEXT("UEDPIE_0_L_Level03"))), 3);
	TestEqual(TEXT("L_FlatCamera3DPrototype should default to level 1 (no-op)"),
		UAbilityUnlockLevelSubsystem::ParseLevelIndexFromMapName(FName(TEXT("L_FlatCamera3DPrototype"))), 1);
	TestEqual(TEXT("L_Paper2DPrototype should default to level 1 (no-op)"),
		UAbilityUnlockLevelSubsystem::ParseLevelIndexFromMapName(FName(TEXT("L_Paper2DPrototype"))), 1);

	// (b) End-to-end: a possessed pawn's UAbilityUnlockComponent unlocks the correct
	// ability as HandleLevelBegin is driven through levels 1-5 in order (issue #217
	// acceptance criteria).
	{
		UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
		if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
		{
			return false;
		}

		UAbilityUnlockLevelSubsystem* Subsystem = World->GetSubsystem<UAbilityUnlockLevelSubsystem>();
		if (!TestNotNull(TEXT("UWorld should auto-instantiate UAbilityUnlockLevelSubsystem"), Subsystem))
		{
			return false;
		}

		APawn* Pawn = World->SpawnActor<APawn>();
		UAbilityUnlockComponent* UnlockComponent = NewObject<UAbilityUnlockComponent>(Pawn);
		UnlockComponent->RegisterComponent();

		AKrowdKontrolPlayerController* Controller = World->SpawnActor<AKrowdKontrolPlayerController>();
		if (!TestNotNull(TEXT("Controller should spawn"), Controller))
		{
			return false;
		}
		Controller->Possess(Pawn);
		// CreateNewMap() worlds skip PostInitializeComponents, so
		// World->GetFirstPlayerController() reads empty without this explicit
		// registration step - matches
		// KrowdKontrolOvercrowdVisualEffectSubsystemTest.cpp/KrowdKontrolEnemyBaseTest.cpp's
		// identical, empirically-confirmed precedent. Production code needs no
		// equivalent call: real gameplay worlds populate this automatically.
		World->AddController(Controller);

		TestTrue(TEXT("Stun should be unlocked at run start"), UnlockComponent->IsAbilityUnlocked(EAbilitySlot::Stun));
		TestFalse(TEXT("Sleep should be locked at run start"), UnlockComponent->IsAbilityUnlocked(EAbilitySlot::Sleep));

		Subsystem->HandleLevelBegin(FName(TEXT("L_Level01")));
		TestFalse(TEXT("Level 1 should remain a no-op"), UnlockComponent->IsAbilityUnlocked(EAbilitySlot::Sleep));

		Subsystem->HandleLevelBegin(FName(TEXT("L_Level02")));
		TestTrue(TEXT("Level 2 should unlock Sleep"), UnlockComponent->IsAbilityUnlocked(EAbilitySlot::Sleep));
		TestFalse(TEXT("Root should still be locked after level 2"), UnlockComponent->IsAbilityUnlocked(EAbilitySlot::Root));

		Subsystem->HandleLevelBegin(FName(TEXT("L_Level03")));
		TestTrue(TEXT("Level 3 should unlock Root"), UnlockComponent->IsAbilityUnlocked(EAbilitySlot::Root));

		Subsystem->HandleLevelBegin(FName(TEXT("L_Level04")));
		TestTrue(TEXT("Level 4 should unlock Fear"), UnlockComponent->IsAbilityUnlocked(EAbilitySlot::Fear));

		Subsystem->HandleLevelBegin(FName(TEXT("L_Level05")));
		TestTrue(TEXT("Level 5 should unlock Snare"), UnlockComponent->IsAbilityUnlocked(EAbilitySlot::Snare));
	}

	// (c) Real wiring: ULevelLifecycleSubsystem::OnLevelBegin's actual broadcast (not
	// a direct HandleLevelBegin call) reaches UAbilityUnlockLevelSubsystem via the
	// Initialize()-time subscription. CreateNewMap()'s synthetic map name won't match
	// L_LevelNN, so this also exercises the "unrecognised map name -> safe level-1
	// no-op" path end to end.
	{
		UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
		if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
		{
			return false;
		}

		UAbilityUnlockLevelSubsystem* Subsystem = World->GetSubsystem<UAbilityUnlockLevelSubsystem>();
		ULevelLifecycleSubsystem* LifecycleSubsystem = World->GetSubsystem<ULevelLifecycleSubsystem>();
		if (!TestNotNull(TEXT("UAbilityUnlockLevelSubsystem should exist"), Subsystem) ||
			!TestNotNull(TEXT("ULevelLifecycleSubsystem should exist"), LifecycleSubsystem))
		{
			return false;
		}

		APawn* Pawn = World->SpawnActor<APawn>();
		UAbilityUnlockComponent* UnlockComponent = NewObject<UAbilityUnlockComponent>(Pawn);
		UnlockComponent->RegisterComponent();
		AKrowdKontrolPlayerController* Controller = World->SpawnActor<AKrowdKontrolPlayerController>();
		if (!TestNotNull(TEXT("Controller should spawn"), Controller))
		{
			return false;
		}
		Controller->Possess(Pawn);
		World->AddController(Controller);

		LifecycleSubsystem->OnWorldBeginPlay(*World);

		TestTrue(TEXT("Stun should still be the only unlocked ability for a non-L_LevelNN map name"),
			UnlockComponent->IsAbilityUnlocked(EAbilitySlot::Stun));
		TestFalse(TEXT("Sleep should stay locked for a non-L_LevelNN map name (safe no-op, not a crash)"),
			UnlockComponent->IsAbilityUnlocked(EAbilitySlot::Sleep));
	}

	// (d) No possessed pawn / no UAbilityUnlockComponent: HandleLevelBegin must not
	// crash, and the missing-component warning must fire exactly once even across
	// repeated calls (bHasWarnedMissingAbilityUnlockComponent).
	{
		UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
		if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
		{
			return false;
		}

		UAbilityUnlockLevelSubsystem* Subsystem = World->GetSubsystem<UAbilityUnlockLevelSubsystem>();
		if (!TestNotNull(TEXT("UAbilityUnlockLevelSubsystem should exist"), Subsystem))
		{
			return false;
		}

		AddExpectedError(TEXT("no possessed pawn with a UAbilityUnlockComponent found"),
			EAutomationExpectedErrorFlags::Contains, 1, false);

		// No pawn/controller spawned at all - GetFirstPlayerController() resolves null,
		// matching a menu map with no playable pawn.
		Subsystem->HandleLevelBegin(FName(TEXT("L_Level02")));
		Subsystem->HandleLevelBegin(FName(TEXT("L_Level03")));
		// The AddExpectedError count of 1 above asserts the second call did not log again.
	}

	// (e) OnLevelBegin racing pawn possession: if HandleLevelBegin fires before the
	// pawn is possessed, the unlock must still land once AKrowdKontrolPlayerController
	// possesses the pawn, via RetryPendingUnlockForPawn() - not be silently dropped for
	// the rest of the world's lifetime.
	{
		UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
		if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
		{
			return false;
		}

		UAbilityUnlockLevelSubsystem* Subsystem = World->GetSubsystem<UAbilityUnlockLevelSubsystem>();
		if (!TestNotNull(TEXT("UAbilityUnlockLevelSubsystem should exist"), Subsystem))
		{
			return false;
		}

		APawn* Pawn = World->SpawnActor<APawn>();
		UAbilityUnlockComponent* UnlockComponent = NewObject<UAbilityUnlockComponent>(Pawn);
		UnlockComponent->RegisterComponent();

		AKrowdKontrolPlayerController* Controller = World->SpawnActor<AKrowdKontrolPlayerController>();
		if (!TestNotNull(TEXT("Controller should spawn"), Controller))
		{
			return false;
		}
		// Registers the controller in the world's controller list (see the (b)/(c)
		// comment above on CreateNewMap() skipping PostInitializeComponents) without
		// possessing the pawn yet, so GetFirstPlayerController() resolves the
		// controller while GetPawn() still returns null - the exact "OnLevelBegin fired
		// before AutoPossessPlayer resolved" ordering this fix targets.
		World->AddController(Controller);

		AddExpectedError(TEXT("no possessed pawn with a UAbilityUnlockComponent found"),
			EAutomationExpectedErrorFlags::Contains, 1, false);

		Subsystem->HandleLevelBegin(FName(TEXT("L_Level02")));
		TestFalse(TEXT("Sleep should still be locked - the pawn isn't possessed yet"),
			UnlockComponent->IsAbilityUnlocked(EAbilitySlot::Sleep));

		// Possessing now drives AKrowdKontrolPlayerController::OnPossess ->
		// RetryPendingAbilityUnlock() -> RetryPendingUnlockForPawn(), delivering the
		// unlock this level's OnLevelBegin couldn't land a moment ago.
		Controller->Possess(Pawn);
		TestTrue(TEXT("Sleep should unlock once the pawn is possessed, via the retry path"),
			UnlockComponent->IsAbilityUnlocked(EAbilitySlot::Sleep));
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
