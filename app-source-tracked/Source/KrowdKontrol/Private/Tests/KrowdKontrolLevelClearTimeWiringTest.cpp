// Confirms issue #170 (PRD "Run Lifecycle & Progression Signals" REQ-2): once
// ULevelClearTimeSubsystem::SubscribeToLevelLifecycle() binds it to a
// ULevelLifecycleSubsystem, that world's OnLevelBegin starts the map's clear-time
// timer and its OnLevelClear stops and records it, producing a persisted best time -
// end-to-end, through the real delegate wiring rather than calling
// StartLevelTimer/StopLevelTimerAndRecordClear directly (that half is already covered
// by KrowdKontrolLevelClearTimeSubsystemTest.cpp). Also confirms a repeat
// SubscribeToLevelLifecycle() call to the same ULevelLifecycleSubsystem does not
// double-bind, and that SubscribeToLevelLifecycle(nullptr) is a defensive no-op.
//
// Deliberately drives SubscribeToLevelLifecycle() directly rather than through
// ULevelLifecycleSubsystem::EnsureLevelClearTimeSubscription()'s own
// GetGameInstance()-based auto-resolution: GetGameInstance() is null in this
// project's CreateNewMap()-based Automation test worlds (see
// KrowdKontrolLevelClearTimeSubsystemTest.cpp's own rationale), so that
// auto-resolution path is untestable here by design - see the plan's Files-to-Change
// note and NOT-Building scope limit for issue #170. It is covered instead by AC #5's
// required manual PIE check (see this PR's body).
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the other
// KrowdKontrol.Unit.* tests.

#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "LevelLifecycleSubsystem.h"
#include "LevelClearTimeSubsystem.h"
#include "EnemyBaseTestActor.h"
#include "AbilitySlot.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolLevelClearTimeWiringTest,
	"KrowdKontrol.Unit.LevelClearTimeWiring",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolLevelClearTimeWiringTest::RunTest(const FString& Parameters)
{
	const FVector ZeroDistanceLocation(0.0f, 0.0f, 0.0f);

	// Clean slate: delete any leftover save data from a prior run of this same test,
	// mirroring KrowdKontrolLevelClearTimeSubsystemTest.cpp's own rationale.
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

	// GetGameInstance() is null in this CreateNewMap() World - the UGameInstance outer
	// here is only there because NewObject<>() needs *an* outer, never touched via
	// GetGameInstance()/GetWorld(), mirroring KrowdKontrolLevelClearTimeSubsystemTest.cpp.
	UGameInstance* GameInstanceOuter = NewObject<UGameInstance>();
	ULevelClearTimeSubsystem* ClearTimeSubsystem = NewObject<ULevelClearTimeSubsystem>(GameInstanceOuter);
	if (!TestNotNull(TEXT("ULevelClearTimeSubsystem should construct"), ClearTimeSubsystem))
	{
		UGameplayStatics::DeleteGameInSlot(ULevelClearTimeSubsystem::SaveSlotName, 0);
		return false;
	}

	ClearTimeSubsystem->SubscribeToLevelLifecycle(LifecycleSubsystem);

	// Repeat bind to the same non-null LifecycleSubsystem: must no-op via
	// AddUniqueDynamic, not double-bind HandleLevelBegin/HandleLevelClear. If a future
	// AddDynamic/AddUniqueDynamic typo ever double-bound these, the second
	// HandleLevelClear() invocation within RefreshLevelClearState()'s single Broadcast()
	// call below would hit StopLevelTimerAndRecordClear's "no active timer" no-op
	// warning (the first invocation already removed the timer entry). This project's
	// harness counts a test that merely logs an unexpected warning as a pass
	// (harness/run_ue_automation.sh's succeededWithWarnings bucket), so an unregistered
	// log alone would NOT fail this test - the "no active timer" entry scan around
	// RefreshLevelClearState() below is what actually enforces this regression check.
	ClearTimeSubsystem->SubscribeToLevelLifecycle(LifecycleSubsystem);

	const FName MapName = FName(*World->GetMapName());

	float OutBest = 0.0f;
	TestFalse(TEXT("No best time should exist before the level begins"),
		ClearTimeSubsystem->GetBestClearTimeSeconds(MapName, OutBest));

	// Fire OnLevelBegin - mirrors KrowdKontrolLevelLifecycleSubsystemTest.cpp's use of
	// OnWorldBeginPlay() to drive it deterministically without a real per-frame tick.
	LifecycleSubsystem->OnWorldBeginPlay(*World);

	AEnemyBaseTestActor* Enemy = World->SpawnActor<AEnemyBaseTestActor>();
	if (!TestNotNull(TEXT("AEnemyBaseTestActor should spawn into the test World"), Enemy))
	{
		UGameplayStatics::DeleteGameInSlot(ULevelClearTimeSubsystem::SaveSlotName, 0);
		return false;
	}
	Enemy->TickCheckDetection(ZeroDistanceLocation); // Idle -> Alert
	Enemy->TickCheckDetection(ZeroDistanceLocation); // Alert -> Attack
	Enemy->ReceiveControl(EAbilitySlot::Stun);        // -> Controlled
	Enemy->TransitionToBanked();                      // -> Banked

	// A double-bind (the AddUniqueDynamic regression this test guards against) would
	// make the single Broadcast() below invoke HandleLevelClear() twice on the same
	// subscriber, and the second invocation's StopLevelTimerAndRecordClear call would
	// hit the "no active timer" no-op warning. Assert that specific failure signature
	// directly via the entries logged during RefreshLevelClearState(), rather than a
	// total warning-count snapshot - a total-count comparison would also break on any
	// unrelated UE_LOG(Warning) added anywhere in this call path (e.g. issue #234's own
	// two new intentional diagnostic lines, which is exactly what forced this test's
	// baseline to change once already).
	//
	// Deliberately NOT AddExpectedError(TEXT("no active timer"), Contains, 0, false):
	// per AutomationTest.h's own docstring, Occurrences == 0 means "must be seen one or
	// more times or the test fails," not "must never occur" - it would make this test
	// fail on every healthy single-bind run instead of catching the double-bind
	// regression it's meant to guard against.
	FAutomationTestExecutionInfo PreClearExecutionInfo;
	GetExecutionInfo(PreClearExecutionInfo);
	const int32 EntryCountBeforeClear = PreClearExecutionInfo.GetEntries().Num();

	// Fire OnLevelClear - mirrors KrowdKontrolLevelLifecycleSubsystemTest.cpp's use of
	// RefreshLevelClearState() to drive it deterministically.
	LifecycleSubsystem->RefreshLevelClearState();

	FAutomationTestExecutionInfo PostClearExecutionInfo;
	GetExecutionInfo(PostClearExecutionInfo);
	const TArray<FAutomationExecutionEntry>& PostClearEntries = PostClearExecutionInfo.GetEntries();
	bool bSawNoActiveTimerWarning = false;
	for (int32 EntryIndex = EntryCountBeforeClear; EntryIndex < PostClearEntries.Num(); ++EntryIndex)
	{
		if (PostClearEntries[EntryIndex].Event.Message.Contains(TEXT("no active timer")))
		{
			bSawNoActiveTimerWarning = true;
			break;
		}
	}
	TestFalse(TEXT("RefreshLevelClearState() must not trigger StopLevelTimerAndRecordClear's 'no active timer' warning on a healthy single-bind clear - a double-bind would produce it via the second HandleLevelClear() invocation"),
		bSawNoActiveTimerWarning);

	// This assertion pair can only be true if HandleLevelBegin actually called
	// StartLevelTimer - otherwise StopLevelTimerAndRecordClear inside HandleLevelClear
	// would hit the "no active timer" no-op path and never record anything - proving
	// both halves of the OnLevelBegin/OnLevelClear wiring at once.
	TestTrue(TEXT("OnLevelClear should have recorded a best time for this map via the OnLevelBegin/OnLevelClear wiring"),
		ClearTimeSubsystem->GetBestClearTimeSeconds(MapName, OutBest));
	TestTrue(TEXT("The recorded best time should be non-negative"), OutBest >= 0.0f);

	// GetLastClearTimeSeconds() (issue #175) should reflect the same clear this
	// HandleLevelClear() invocation just persisted as the best - a fresh save slot
	// means this run's elapsed time and the recorded best are the same value.
	TestEqual(TEXT("GetLastClearTimeSeconds() should equal the elapsed time this OnLevelClear invocation recorded"),
		ClearTimeSubsystem->GetLastClearTimeSeconds(), OutBest);

	// Null-safety: must not crash, and must log the documented warning.
	AddExpectedError(TEXT("LifecycleSubsystem is null"), EAutomationExpectedErrorFlags::Contains, 1);
	ClearTimeSubsystem->SubscribeToLevelLifecycle(nullptr);

	UGameplayStatics::DeleteGameInSlot(ULevelClearTimeSubsystem::SaveSlotName, 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
