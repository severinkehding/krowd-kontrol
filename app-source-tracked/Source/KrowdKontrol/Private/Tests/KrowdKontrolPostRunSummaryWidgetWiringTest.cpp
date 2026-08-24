// Confirms issue #175 (PRD 06 REQ-6): once UPostRunSummaryWidget::BindToLevelLifecycle()
// binds it to a ULevelLifecycleSubsystem's OnLevelClear, a real OnLevelBegin -> bank-all-
// enemies-with-Crowd-Mastery-sampling -> OnLevelClear sequence produces real,
// non-placeholder clear-time/best-time/Crowd-Mastery values on the widget - end-to-end,
// through the real delegate wiring rather than calling SetSummaryValues() directly (that
// half is already covered by KrowdKontrolPostRunSummaryWidgetTest.cpp).
//
// Mirrors KrowdKontrolLevelClearTimeWiringTest.cpp's structure: GetGameInstance() is null
// in this project's CreateNewMap()-based Automation test worlds, so a bare
// NewObject<>()-constructed ULevelClearTimeSubsystem is injected into the widget's
// CachedLevelClearTimeSubsystem via friendship instead of resolving one through
// GetGameInstance().
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the other
// KrowdKontrol.Unit.* tests.

#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "PostRunSummaryWidget.h"
#include "LevelLifecycleSubsystem.h"
#include "LevelClearTimeSubsystem.h"
#include "CrowdMasterySubsystem.h"
#include "EnemyBaseTestActor.h"
#include "AbilitySlot.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolPostRunSummaryWidgetWiringTest,
	"KrowdKontrol.Unit.PostRunSummaryWidgetWiring",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolPostRunSummaryWidgetWiringTest::RunTest(const FString& Parameters)
{
	const FVector ZeroDistanceLocation(0.0f, 0.0f, 0.0f);

	// Clean slate: delete any leftover save data from a prior run of this same test,
	// mirroring KrowdKontrolLevelClearTimeWiringTest.cpp's own rationale.
	UGameplayStatics::DeleteGameInSlot(ULevelClearTimeSubsystem::SaveSlotName, 0);

	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
	{
		UGameplayStatics::DeleteGameInSlot(ULevelClearTimeSubsystem::SaveSlotName, 0);
		return false;
	}

	ULevelLifecycleSubsystem* LifecycleSubsystem = World->GetSubsystem<ULevelLifecycleSubsystem>();
	if (!TestNotNull(TEXT("UWorld should auto-instantiate ULevelLifecycleSubsystem"), LifecycleSubsystem))
	{
		UGameplayStatics::DeleteGameInSlot(ULevelClearTimeSubsystem::SaveSlotName, 0);
		return false;
	}

	UCrowdMasterySubsystem* CrowdMasterySubsystem = World->GetSubsystem<UCrowdMasterySubsystem>();
	if (!TestNotNull(TEXT("UWorld should auto-instantiate UCrowdMasterySubsystem"), CrowdMasterySubsystem))
	{
		UGameplayStatics::DeleteGameInSlot(ULevelClearTimeSubsystem::SaveSlotName, 0);
		return false;
	}

	// GetGameInstance() is null in this CreateNewMap() World - the UGameInstance outer
	// here is only there because NewObject<>() needs *an* outer, never touched via
	// GetGameInstance()/GetWorld(), mirroring KrowdKontrolLevelClearTimeWiringTest.cpp.
	UGameInstance* GameInstanceOuter = NewObject<UGameInstance>();
	ULevelClearTimeSubsystem* ClearTimeSubsystem = NewObject<ULevelClearTimeSubsystem>(GameInstanceOuter);
	if (!TestNotNull(TEXT("ULevelClearTimeSubsystem should construct"), ClearTimeSubsystem))
	{
		UGameplayStatics::DeleteGameInSlot(ULevelClearTimeSubsystem::SaveSlotName, 0);
		return false;
	}
	ClearTimeSubsystem->SubscribeToLevelLifecycle(LifecycleSubsystem);

	UPostRunSummaryWidget* Widget = CreateWidget<UPostRunSummaryWidget>(World, UPostRunSummaryWidget::StaticClass());
	if (!TestNotNull(TEXT("UPostRunSummaryWidget should construct"), Widget))
	{
		UGameplayStatics::DeleteGameInSlot(ULevelClearTimeSubsystem::SaveSlotName, 0);
		return false;
	}
	// CreateWidget() -> Initialize() -> NativeOnInitialized() already ran
	// BindToLevelLifecycle() for real, binding the widget to LifecycleSubsystem's
	// OnLevelClear. Inject the test-constructed ClearTimeSubsystem via friendship so
	// the widget's own HandleLevelClear() reads real, test-driven values instead of
	// trying (and failing) to resolve one through the null GetGameInstance().
	Widget->CachedLevelClearTimeSubsystem = ClearTimeSubsystem;

	// Fire OnLevelBegin - starts ClearTimeSubsystem's timer for real via its own
	// subscription, mirrors KrowdKontrolLevelClearTimeWiringTest.cpp's identical use of
	// OnWorldBeginPlay() to drive it deterministically without a real per-frame tick.
	LifecycleSubsystem->OnWorldBeginPlay(*World);

	// Spawn 2 enemies, drive each to Controlled and sample Crowd Mastery explicitly
	// (spawning/controlling alone does not sample it - HandleAbilityCastApplied must be
	// called, mirroring KrowdKontrolCrowdMasterySubsystemTest.cpp), then bank both so
	// the level-clear condition is met.
	TArray<AEnemyBaseTestActor*> Enemies;
	for (int32 i = 0; i < 2; ++i)
	{
		AEnemyBaseTestActor* Enemy = World->SpawnActor<AEnemyBaseTestActor>();
		if (!TestNotNull(TEXT("AEnemyBaseTestActor should spawn into the test World"), Enemy))
		{
			UGameplayStatics::DeleteGameInSlot(ULevelClearTimeSubsystem::SaveSlotName, 0);
			return false;
		}
		Enemy->TickCheckDetection(ZeroDistanceLocation); // Idle -> Alert
		Enemy->TickCheckDetection(ZeroDistanceLocation); // Alert -> Attack
		Enemy->ReceiveControl(EAbilitySlot::Stun);        // -> Controlled
		CrowdMasterySubsystem->HandleAbilityCastApplied(EAbilitySlot::Stun, Enemy);
		Enemies.Add(Enemy);
	}

	TestEqual(TEXT("Running Crowd Mastery max should reach 2 after both enemies are Controlled"),
		CrowdMasterySubsystem->GetRunningMaxControlledCount(), 2);

	for (AEnemyBaseTestActor* Enemy : Enemies)
	{
		Enemy->TransitionToBanked(); // -> Banked
	}

	// Fire OnLevelClear - in bind order, ClearTimeSubsystem->HandleLevelClear() (bound
	// first, from SubscribeToLevelLifecycle() above) records this run's elapsed time as
	// the new best (a fresh save slot means this run is trivially the new best), then
	// the widget's own HandleLevelClear() reads the now-up-to-date values.
	LifecycleSubsystem->RefreshLevelClearState();

	// Assert real, non-placeholder values reached the widget.
	TestFalse(TEXT("Clear time should not show the 4:32 placeholder after a real level clear"),
		Widget->GetClearTimeDisplayText().ToString().Contains(TEXT("4:32")));

	// Cross-check: the widget's displayed clear time should match this run's actual
	// elapsed time, read independently from the subsystem - not just "not the placeholder".
	const float ExpectedRunClearTimeSeconds = ClearTimeSubsystem->GetLastClearTimeSeconds();
	const int32 ClampedRunSeconds = FMath::Max(0, FMath::RoundToInt(ExpectedRunClearTimeSeconds));
	const FString ExpectedClearTimeDisplay = FString::Printf(
		TEXT("Clear Time: %d:%02d"), ClampedRunSeconds / 60, ClampedRunSeconds % 60);
	TestEqual(TEXT("Widget's displayed clear time should match the subsystem's independently-read run time"),
		Widget->GetClearTimeDisplayText().ToString(), ExpectedClearTimeDisplay);

	TestFalse(TEXT("Crowd Mastery should not show the 14 placeholder after a real level clear"),
		Widget->GetCrowdMasteryDisplayText().ToString().Contains(TEXT("14")));
	TestEqual(TEXT("Crowd Mastery should show the real value this test drove"),
		Widget->GetCrowdMasteryDisplayText().ToString(), FString(TEXT("Crowd Mastery: 2")));

	TestTrue(TEXT("Best clear time text should be non-empty after a real level clear"),
		!Widget->GetBestClearTimeDisplayText().IsEmpty());
	TestFalse(TEXT("Best clear time should not show the 4:32 placeholder after a real level clear"),
		Widget->GetBestClearTimeDisplayText().ToString().Contains(TEXT("Best: 4:32")));

	// Cross-check: the widget's displayed best time should be consistent with what the
	// subsystem itself has recorded, read independently by the test - not just "not the
	// placeholder".
	const FName MapName = FName(*World->GetMapName());
	float OutBestSeconds = 0.0f;
	const bool bHasBest = ClearTimeSubsystem->GetBestClearTimeSeconds(MapName, OutBestSeconds);
	TestTrue(TEXT("A best clear time should be persisted for this map after OnLevelClear"), bHasBest);
	TestTrue(TEXT("The persisted best clear time should be non-negative"), OutBestSeconds >= 0.0f);

	const int32 ClampedBestSeconds = FMath::Max(0, FMath::RoundToInt(OutBestSeconds));
	const FString ExpectedBestDisplay = FString::Printf(TEXT("Best: %d:%02d"), ClampedBestSeconds / 60, ClampedBestSeconds % 60);
	TestEqual(TEXT("Widget's displayed best time should match the subsystem's independently-read persisted best"),
		Widget->GetBestClearTimeDisplayText().ToString(), ExpectedBestDisplay);

	// Missing-subsystem degrade: HandleLevelClear() should not crash and should fall back
	// to 0 clear-time/best-time (Crowd Mastery is independent of ClearTimeSubsystem and
	// still populates) when CachedLevelClearTimeSubsystem was never resolved - a separate
	// UPostRunSummaryWidget instance's own state, not shared with Widget above.
	UPostRunSummaryWidget* DegradedWidget = CreateWidget<UPostRunSummaryWidget>(World, UPostRunSummaryWidget::StaticClass());
	if (TestNotNull(TEXT("UPostRunSummaryWidget should construct for the degrade case"), DegradedWidget))
	{
		// CachedLevelClearTimeSubsystem left null - simulates GetGameInstance() being null.
		DegradedWidget->HandleLevelClear();
		TestEqual(TEXT("Clear time should fall back to 0:00 with no resolvable subsystem"),
			DegradedWidget->GetClearTimeDisplayText().ToString(), FString(TEXT("Clear Time: 0:00")));
		TestEqual(TEXT("Best clear time should fall back to 0:00 with no resolvable subsystem"),
			DegradedWidget->GetBestClearTimeDisplayText().ToString(), FString(TEXT("Best: 0:00")));
	}

	UGameplayStatics::DeleteGameInSlot(ULevelClearTimeSubsystem::SaveSlotName, 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
