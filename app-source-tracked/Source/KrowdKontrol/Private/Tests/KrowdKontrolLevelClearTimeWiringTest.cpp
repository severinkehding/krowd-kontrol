// Confirms issue #170 (PRD "Run Lifecycle & Progression Signals" REQ-2): once
// ULevelClearTimeSubsystem::SubscribeToLevelLifecycle() binds it to a
// ULevelLifecycleSubsystem, that world's OnLevelBegin starts the map's clear-time
// timer and its OnLevelClear stops and records it, producing a persisted best time -
// end-to-end, through the real delegate wiring rather than calling
// StartLevelTimer/StopLevelTimerAndRecordClear directly (that half is already covered
// by KrowdKontrolLevelClearTimeSubsystemTest.cpp). Also confirms a repeat
// SubscribeToLevelLifecycle() call to the same ULevelLifecycleSubsystem does not
// double-bind (review focus area 1), and that AKrowdKontrolPlayerController::BeginPlay()
// itself - not just the subsystem-level plumbing above - results in a working
// subscription (consolidated review HIGH Issue 1).
//
// GetGameInstance() is null in this project's CreateNewMap()-based Automation test
// worlds (see KrowdKontrolLevelClearTimeSubsystemTest.cpp's own rationale), so this
// test never touches it - ULevelClearTimeSubsystem is constructed directly via
// NewObject<>() and SubscribeToLevelLifecycle() takes the ULevelLifecycleSubsystem as
// an explicit parameter, exactly the seam that lets this test drive the wiring with
// zero GameInstance involvement, mirroring KrowdKontrolLevelFailedTest.cpp's dedicated
// cross-system "wiring" test shape for issue #171. The controller-mediated case below
// instead injects CachedLevelClearTimeSubsystem via the FKrowdKontrolLevelClearTimeWiringTest
// friendship on AKrowdKontrolPlayerController, mirroring KrowdKontrolLevelFailedTest.cpp's
// identical injection pattern.
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the other
// KrowdKontrol.Unit.* tests.

#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "KrowdKontrolPlayerController.h"
#include "LevelLifecycleSubsystem.h"
#include "LevelClearTimeSubsystem.h"
#include "EnemyBaseTestActor.h"
#include "AbilitySlot.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Engine.h"
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

	// Repeat bind to the same non-null LifecycleSubsystem (review focus area 1): must
	// no-op via AddUniqueDynamic, not double-bind HandleLevelBegin/HandleLevelClear. If
	// a future AddDynamic/AddUniqueDynamic typo ever double-bound these, the second
	// HandleLevelClear() invocation within RefreshLevelClearState()'s single Broadcast()
	// call below would hit StopLevelTimerAndRecordClear's "no active timer" no-op
	// warning (the first invocation already removed the timer entry) - deliberately NOT
	// pre-registered via AddExpectedError, so that regression fails this test with an
	// unexpected log entry instead of silently passing. (Occurrences=0 in this engine
	// version means "any count", not "must not occur" - see KrowdKontrolFirstStunBeaconComponentTest.cpp's
	// D-012 AddExpectedError convention comment - so it can't be used here to assert
	// absence.)
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

	// Fire OnLevelClear - mirrors KrowdKontrolLevelLifecycleSubsystemTest.cpp's use of
	// RefreshLevelClearState() to drive it deterministically.
	LifecycleSubsystem->RefreshLevelClearState();

	// This single assertion pair can only be true if HandleLevelBegin actually called
	// StartLevelTimer - otherwise StopLevelTimerAndRecordClear inside HandleLevelClear
	// would hit the "no active timer" no-op path and never record anything - proving
	// both halves of the OnLevelBegin/OnLevelClear wiring at once.
	TestTrue(TEXT("OnLevelClear should have recorded a best time for this map via the OnLevelBegin/OnLevelClear wiring"),
		ClearTimeSubsystem->GetBestClearTimeSeconds(MapName, OutBest));
	TestTrue(TEXT("The recorded best time should be non-negative"), OutBest >= 0.0f);

	// Null-safety: must not crash.
	ClearTimeSubsystem->SubscribeToLevelLifecycle(nullptr);

	// End-to-end controller wiring (consolidated review HIGH Issue 1): the block above
	// proves ULevelClearTimeSubsystem's own SubscribeToLevelLifecycle()/HandleLevelBegin()/
	// HandleLevelClear() logic works when called directly, but not that
	// AKrowdKontrolPlayerController::BeginPlay() itself - the PR's actual production
	// caller - results in a working subscription. A broken/dropped subscribe call inside
	// BeginPlay() would compile cleanly and pass every other test in this file. Fresh
	// World so this map's clear-time key doesn't already have a recorded best from the
	// block above.
	UWorld* ControllerWorld = FAutomationEditorCommonUtils::CreateNewMap();
	if (!TestNotNull(TEXT("Second CreateNewMap should return a valid World"), ControllerWorld))
	{
		UGameplayStatics::DeleteGameInSlot(ULevelClearTimeSubsystem::SaveSlotName, 0);
		return false;
	}

	ULevelLifecycleSubsystem* ControllerLifecycleSubsystem = ControllerWorld->GetSubsystem<ULevelLifecycleSubsystem>();
	if (!TestNotNull(TEXT("ControllerWorld should auto-instantiate ULevelLifecycleSubsystem"), ControllerLifecycleSubsystem))
	{
		UGameplayStatics::DeleteGameInSlot(ULevelClearTimeSubsystem::SaveSlotName, 0);
		return false;
	}

	AKrowdKontrolPlayerController* Controller = ControllerWorld->SpawnActor<AKrowdKontrolPlayerController>();
	if (!TestNotNull(TEXT("AKrowdKontrolPlayerController should spawn"), Controller))
	{
		UGameplayStatics::DeleteGameInSlot(ULevelClearTimeSubsystem::SaveSlotName, 0);
		return false;
	}

	// SetAsLocalPlayerController() alone only flips bIsLocalPlayerController - it does
	// NOT attach a UPlayer, which CreateHUDWidgets() (called from BeginPlay()) hard-requires,
	// mirroring KrowdKontrolHUDWiringTest.cpp's identical precedent/rationale.
	Controller->Player = NewObject<ULocalPlayer>(GEngine);
	Controller->SetAsLocalPlayerController();

	// GetGameInstance() is null in this CreateNewMap() World - inject a directly-constructed
	// subsystem via the FKrowdKontrolLevelClearTimeWiringTest friendship, mirroring
	// KrowdKontrolLevelFailedTest.cpp's identical precedent. Injected before
	// DispatchBeginPlay() so BeginPlay()'s own ResolveLevelClearTimeSubsystem() finds it
	// already cached instead of hitting the no-subsystem warning path - a real
	// GameInstance-resolved subsystem would likewise already exist before BeginPlay runs.
	UGameInstance* ControllerGameInstanceOuter = NewObject<UGameInstance>();
	ULevelClearTimeSubsystem* ControllerClearTimeSubsystem = NewObject<ULevelClearTimeSubsystem>(ControllerGameInstanceOuter);
	Controller->CachedLevelClearTimeSubsystem = ControllerClearTimeSubsystem;

	Controller->DispatchBeginPlay(); // exercises the real BeginPlay() wiring block

	const FName ControllerMapName = FName(*ControllerWorld->GetMapName());

	ControllerLifecycleSubsystem->OnWorldBeginPlay(*ControllerWorld);

	AEnemyBaseTestActor* ControllerEnemy = ControllerWorld->SpawnActor<AEnemyBaseTestActor>();
	if (!TestNotNull(TEXT("AEnemyBaseTestActor should spawn into ControllerWorld"), ControllerEnemy))
	{
		UGameplayStatics::DeleteGameInSlot(ULevelClearTimeSubsystem::SaveSlotName, 0);
		return false;
	}
	ControllerEnemy->TickCheckDetection(ZeroDistanceLocation); // Idle -> Alert
	ControllerEnemy->TickCheckDetection(ZeroDistanceLocation); // Alert -> Attack
	ControllerEnemy->ReceiveControl(EAbilitySlot::Stun);        // -> Controlled
	ControllerEnemy->TransitionToBanked();                      // -> Banked

	ControllerLifecycleSubsystem->RefreshLevelClearState();

	float ControllerOutBest = 0.0f;
	TestTrue(TEXT("BeginPlay()'s own wiring should result in a recorded clear time"),
		ControllerClearTimeSubsystem->GetBestClearTimeSeconds(ControllerMapName, ControllerOutBest));

	UGameplayStatics::DeleteGameInSlot(ULevelClearTimeSubsystem::SaveSlotName, 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
