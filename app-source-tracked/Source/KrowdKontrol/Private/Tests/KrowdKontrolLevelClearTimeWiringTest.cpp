// Confirms issue #170 (PRD "Run Lifecycle & Progression Signals" REQ-2): once
// ULevelClearTimeSubsystem::SubscribeToLevelLifecycle() binds it to a
// ULevelLifecycleSubsystem, that world's OnLevelBegin starts the map's clear-time
// timer and its OnLevelClear stops and records it, producing a persisted best time -
// end-to-end, through the real delegate wiring rather than calling
// StartLevelTimer/StopLevelTimerAndRecordClear directly (that half is already covered
// by KrowdKontrolLevelClearTimeSubsystemTest.cpp).
//
// GetGameInstance() is null in this project's CreateNewMap()-based Automation test
// worlds (see KrowdKontrolLevelClearTimeSubsystemTest.cpp's own rationale), so this
// test never touches it - ULevelClearTimeSubsystem is constructed directly via
// NewObject<>() and SubscribeToLevelLifecycle() takes the ULevelLifecycleSubsystem as
// an explicit parameter, exactly the seam that lets this test drive the wiring with
// zero GameInstance involvement, mirroring KrowdKontrolLevelFailedTest.cpp's dedicated
// cross-system "wiring" test shape for issue #171.
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

	UGameplayStatics::DeleteGameInSlot(ULevelClearTimeSubsystem::SaveSlotName, 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
