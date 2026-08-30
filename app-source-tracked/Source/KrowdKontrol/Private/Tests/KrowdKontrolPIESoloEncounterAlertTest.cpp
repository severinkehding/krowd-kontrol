// Adds KrowdKontrol.PIE.SoloEncounterAlert.<LevelName> (issue #31 pass-1 review
// follow-up) - KrowdKontrolLevelTestUtils::CheckSoloEncounterForCounteredType (the
// KrowdKontrol.Unit.Level0*Structure tests) only proves the entrance room holds
// exactly one enemy of the right type at level load; it can't observe a live
// EEnemyState::Alert transition because FAutomationEditorCommonUtils::LoadMap never
// dispatches BeginPlay (see those tests' own header comments). This test proves the
// runtime half of issue #31's AC instead: opens each level in a real PIE session
// (mirrors KrowdKontrolPIESerializedPlacedActorHealthTest.cpp's AutomationOpenMap
// shape) and polls, on a wall-clock timeout rather than a fixed frame count, until
// the entrance room's sole enemy transitions Idle->Alert.
//
// Wall-clock polling, not a short fixed-frame wait, because AEnemyBase::
// TickCheckDetection's Idle->Alert branch is gated on
// !OwningRoom->IsActivationPending() (EnemyBase.cpp), and ARoomActor's own
// first-entry countdown (RoomActivationCountdownSeconds, default 3.0s - RoomActor.h)
// keeps that pending for a few real seconds after the player first enters, even
// though the player (PlayerStart, confirmed via a headless Python read of each map)
// starts right on top of the entrance room's own origin, well inside
// DetectionRangeUnits (1500 units - EnemyBase.h; the entrance-room enemy itself sits
// ~200-215 units off that centre). Mirrors
// KrowdKontrolPIESniperRangeBreakChaseTest.cpp's wall-clock-timeout polling shape for
// exactly this reason (that file's own WaitForRoomActivated phase hits the same
// gate).
//
// FOUR SEPARATE TESTS, NOT ONE LOOP: same AutomationOpenMap sequencing constraint
// KrowdKontrolPIESerializedPlacedActorHealthTest.cpp's header documents in detail -
// one IMPLEMENT_SIMPLE_AUTOMATION_TEST per map, each with its own single
// AutomationOpenMap call.
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the other
// KrowdKontrol.* automation tests.

#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/LevelStructureTestUtils.h"
#include "RoomActor.h"
#include "EnemyBase.h"
#include "Engine/World.h"
#include "EngineUtils.h"

#if WITH_DEV_AUTOMATION_TESTS

class FKrowdKontrolAssertSoloEncounterAlertCommand : public IAutomationLatentCommand
{
public:
	FKrowdKontrolAssertSoloEncounterAlertCommand(FAutomationTestBase* InTest, FString InMapPath, EEnemyType InCounteredType)
		: Test(InTest)
		, MapPath(MoveTemp(InMapPath))
		, CounteredType(InCounteredType)
		, StartTime(FPlatformTime::Seconds())
	{
	}

	virtual bool Update() override;

private:
	FAutomationTestBase* Test;
	FString MapPath;
	EEnemyType CounteredType;
	double StartTime;
};

bool FKrowdKontrolAssertSoloEncounterAlertCommand::Update()
{
	UWorld* PIEWorld = AutomationCommon::GetAnyGameWorld();
	if (!PIEWorld)
	{
		return false;
	}

	TArray<ARoomActor*> Rooms;
	for (TActorIterator<ARoomActor> It(PIEWorld); It; ++It)
	{
		Rooms.Add(*It);
	}
	TArray<ARoomActor*> SortedRooms = KrowdKontrolLevelTestUtils::SortRoomsByX(Rooms);

	AEnemyBase* EntranceEnemy = nullptr;
	if (SortedRooms.Num() > 0)
	{
		ARoomActor* EntranceRoom = SortedRooms[0];
		for (TActorIterator<AEnemyBase> It(PIEWorld); It; ++It)
		{
			if (KrowdKontrolLevelTestUtils::FindNearestRoom(*It, Rooms) == EntranceRoom)
			{
				EntranceEnemy = *It;
				break;
			}
		}
	}

	if (EntranceEnemy && EntranceEnemy->GetEnemyState() == EEnemyState::Alert)
	{
		Test->TestEqual(FString::Printf(TEXT("[%s] Entrance room's sole enemy should be the newly-unlocked ability's countered type"), *MapPath),
			KrowdKontrolLevelTestUtils::GetPlacedEnemyType(EntranceEnemy).Get(EEnemyType::RU_NNR), CounteredType);
		return true;
	}

	// 10s wall-clock timeout comfortably covers RoomActivationCountdownSeconds
	// (3.0s default) plus render/tick overhead, without hanging the automation run
	// indefinitely if the entrance enemy never alerts (same shape
	// KrowdKontrolPIESniperRangeBreakChaseTest.cpp's driver command uses).
	if (FPlatformTime::Seconds() - StartTime > 10.0)
	{
		Test->AddError(FString::Printf(TEXT("[%s] Entrance room's sole enemy never went Alert within 10 seconds (issue #31 forced-safe solo encounter)"), *MapPath));
		return true;
	}

	return false;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolPIESoloEncounterAlertLevel02Test,
	"KrowdKontrol.PIE.SoloEncounterAlert.L_Level02",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolPIESoloEncounterAlertLevel02Test::RunTest(const FString& Parameters)
{
	static const FString MapPath = TEXT("/Game/Maps/L_Level02");
	AutomationOpenMap(MapPath);
	ADD_LATENT_AUTOMATION_COMMAND(FKrowdKontrolAssertSoloEncounterAlertCommand(this, MapPath, EEnemyType::SN_1PR));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolPIESoloEncounterAlertLevel03Test,
	"KrowdKontrol.PIE.SoloEncounterAlert.L_Level03",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolPIESoloEncounterAlertLevel03Test::RunTest(const FString& Parameters)
{
	static const FString MapPath = TEXT("/Game/Maps/L_Level03");
	AutomationOpenMap(MapPath);
	ADD_LATENT_AUTOMATION_COMMAND(FKrowdKontrolAssertSoloEncounterAlertCommand(this, MapPath, EEnemyType::TR_UPR));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolPIESoloEncounterAlertLevel04Test,
	"KrowdKontrol.PIE.SoloEncounterAlert.L_Level04",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolPIESoloEncounterAlertLevel04Test::RunTest(const FString& Parameters)
{
	static const FString MapPath = TEXT("/Game/Maps/L_Level04");
	AutomationOpenMap(MapPath);
	ADD_LATENT_AUTOMATION_COMMAND(FKrowdKontrolAssertSoloEncounterAlertCommand(this, MapPath, EEnemyType::B0_0MR));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolPIESoloEncounterAlertLevel05Test,
	"KrowdKontrol.PIE.SoloEncounterAlert.L_Level05",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolPIESoloEncounterAlertLevel05Test::RunTest(const FString& Parameters)
{
	static const FString MapPath = TEXT("/Game/Maps/L_Level05");
	AutomationOpenMap(MapPath);
	ADD_LATENT_AUTOMATION_COMMAND(FKrowdKontrolAssertSoloEncounterAlertCommand(this, MapPath, EEnemyType::RU_NNR));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
