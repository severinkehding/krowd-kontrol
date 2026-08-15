// Confirms URoomEnemyBudgetController (issue #82) keeps a room's active enemy count
// pinned to MaxConcurrentDensity as enemies are banked, exhausts TotalRoomBudget
// correctly, and fires OnRoomCleared exactly once - the mechanism PRD 01's REQ-6
// (count, not damage, is the difficulty lever) depends on.
//
// Needs a real UWorld to spawn into (SpawnEnemy() calls GetWorld()->SpawnActor), so
// unlike KrowdKontrolPlaceholderCubeActorTest.cpp this can't use a bare
// NewObject()-constructed actor with no world. FAutomationEditorCommonUtils::
// CreateNewMap() gives a real editor UWorld to spawn into without needing PIE or a
// full actor BeginPlay lifecycle - see KrowdKontrol.Build.cs's conditional UnrealEd
// dependency, added for this. InitializeRoom() is called directly rather than via
// BeginPlay() for the same reason: this test doesn't drive the World through
// World->BeginPlay(), so component BeginPlay would never fire on its own.
//
// (d) from the issue's acceptance criteria - "no damage/health values are touched" -
// is satisfied structurally, not by a runtime assertion: this codebase has no
// health/damage type yet for the controller to touch, and RoomEnemyBudgetController.h
// has no such dependency in its member list.
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the other
// KrowdKontrol.Unit.* tests.

#include "Misc/AutomationTest.h"
#include "RoomEnemyBudgetController.h"
#include "RoomClearedTestListener.h"
#include "PlaceholderCubeActor.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolRoomEnemyBudgetControllerTest,
	"KrowdKontrol.Unit.RoomEnemyBudgetController",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolRoomEnemyBudgetControllerTest::RunTest(const FString& Parameters)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
	{
		return false;
	}

	AActor* OwnerActor = World->SpawnActor<AActor>();
	if (!TestNotNull(TEXT("Owner actor should spawn into the test World"), OwnerActor))
	{
		return false;
	}

	URoomEnemyBudgetController* Controller =
		NewObject<URoomEnemyBudgetController>(OwnerActor);
	if (!TestNotNull(TEXT("URoomEnemyBudgetController should construct"), Controller))
	{
		return false;
	}
	Controller->RegisterComponent();

	Controller->TotalRoomBudget = 5;
	Controller->MaxConcurrentDensity = 2;
	Controller->EnemyClassToSpawn = APlaceholderCubeActor::StaticClass();

	URoomClearedTestListener* Listener = NewObject<URoomClearedTestListener>();
	Controller->OnRoomCleared.AddDynamic(Listener, &URoomClearedTestListener::HandleRoomCleared);

	Controller->InitializeRoom();

	// (a) initial fill never exceeds MaxConcurrentDensity, even though the 5-enemy
	// budget is larger than the 2-enemy density cap.
	TestEqual(TEXT("Active count should be capped at MaxConcurrentDensity after initial fill"),
		Controller->GetActiveEnemyCount(), 2);
	TestEqual(TEXT("Remaining budget should reflect the 2 enemies spawned at fill"),
		Controller->GetRemainingBudget(), 3);

	// (b) NotifyEnemyBanked() triggers a replacement spawn while budget remains.
	Controller->NotifyEnemyBanked();
	TestEqual(TEXT("Active count should stay at the density cap after a replacement spawn"),
		Controller->GetActiveEnemyCount(), 2);
	TestEqual(TEXT("Remaining budget should drop by one for the replacement spawn"),
		Controller->GetRemainingBudget(), 2);
	TestEqual(TEXT("OnRoomCleared should not have fired yet"), Listener->CallCount, 0);

	// Spend the rest of the budget on replacements - two more banked calls exhaust it.
	Controller->NotifyEnemyBanked();
	Controller->NotifyEnemyBanked();
	TestEqual(TEXT("Active count should still be at the density cap"),
		Controller->GetActiveEnemyCount(), 2);
	TestEqual(TEXT("Budget should be exhausted"), Controller->GetRemainingBudget(), 0);
	TestEqual(TEXT("OnRoomCleared should not fire while enemies are still active"),
		Listener->CallCount, 0);

	// (a) again: banking with no budget left must not spawn a replacement, so active
	// count only ever goes down from here - confirming density is never exceeded.
	Controller->NotifyEnemyBanked();
	TestEqual(TEXT("Active count should drop with no replacement once budget is exhausted"),
		Controller->GetActiveEnemyCount(), 1);

	// (c) OnRoomCleared fires exactly once, only once budget and active count both
	// reach 0.
	Controller->NotifyEnemyBanked();
	TestEqual(TEXT("Active count should reach 0"), Controller->GetActiveEnemyCount(), 0);
	TestEqual(TEXT("OnRoomCleared should have fired exactly once"), Listener->CallCount, 1);

	// Further banking calls after the room is already clear must not re-fire the event.
	Controller->NotifyEnemyBanked();
	TestEqual(TEXT("OnRoomCleared should still have fired exactly once"), Listener->CallCount, 1);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
