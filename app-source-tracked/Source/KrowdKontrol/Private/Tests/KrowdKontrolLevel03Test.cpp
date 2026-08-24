// Confirms L_Level03 (issue #45, PRD 05 REQ-1/REQ-2/REQ-3) - the next Alpha level
// after L_Level02 (issue #43) - loads without errors and is measurably harder than
// L_Level02: strictly more rooms and strictly more enemies, computed dynamically
// against the live L_Level02 asset rather than a hardcoded magic number, so the
// comparison can't silently drift if L_Level02 is ever revised.
//
// Uses FAutomationEditorCommonUtils::LoadMap (not CreateNewMap) because this is
// regression coverage for the shipped level assets themselves, not the ARoomActor/
// ADoorConnectorActor classes in isolation - mirrors KrowdKontrolLevel02Test.cpp's
// own approach.
//
// Room/door counts are asserted via TActorIterator rather than by name, since
// TActorIterator iteration order is not guaranteed to match spawn order - a re-save
// of either level that shuffles actor GUIDs must not break this test.
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the other
// KrowdKontrol.Unit.* tests.

#include "Misc/AutomationTest.h"
#include "RoomActor.h"
#include "DoorConnectorActor.h"
#include "EnemyBase.h"
#include "LevelLightingRigActor.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/LevelStructureTestUtils.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Editor.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolLevel03StructureTest,
	"KrowdKontrol.Unit.Level03Structure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolLevel03StructureTest::RunTest(const FString& Parameters)
{
	// Load L_Level02 first to gather its room/enemy counts as the live baseline the
	// difficulty ramp (REQ-3) must exceed - computed here, not hardcoded, so this test
	// can't silently drift if L_Level02 is ever revised.
	FAutomationEditorCommonUtils::LoadMap(TEXT("/Game/Maps/L_Level02"));

	UWorld* Level02World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!TestNotNull(TEXT("L_Level02 should load into a valid World"), Level02World))
	{
		return false;
	}

	int32 Level02RoomCount = 0;
	for (TActorIterator<ARoomActor> It(Level02World); It; ++It)
	{
		++Level02RoomCount;
	}

	int32 Level02EnemyCount = 0;
	for (TActorIterator<AEnemyBase> It(Level02World); It; ++It)
	{
		++Level02EnemyCount;
	}

	FAutomationEditorCommonUtils::LoadMap(TEXT("/Game/Maps/L_Level03"));

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!TestNotNull(TEXT("L_Level03 should load into a valid World"), World))
	{
		return false;
	}

	TArray<ARoomActor*> Rooms;
	for (TActorIterator<ARoomActor> It(World); It; ++It)
	{
		Rooms.Add(*It);
	}
	TestTrue(TEXT("L_Level03's room count should be strictly greater than L_Level02's (REQ-3 difficulty ramp)"),
		Rooms.Num() > Level02RoomCount);
	// Exact-count check is intentionally in addition to (not instead of) the dynamic
	// comparison above: L_Level03 is a hand-authored level with a fixed design target
	// per PRD 05, not a procedurally generated one, so "5" here isn't a magic number
	// at risk of drifting relative to some other live value - it's this specific
	// level's own spec. The dynamic check guards the REQ-3 difficulty ramp against
	// L_Level02 changing; this check guards L_Level03 itself against an unintended
	// room being added/removed.
	TestEqual(TEXT("L_Level03 room count should match its design target of 5"), Rooms.Num(), 5);

	TArray<ADoorConnectorActor*> Doors;
	for (TActorIterator<ADoorConnectorActor> It(World); It; ++It)
	{
		Doors.Add(*It);
	}
	TestEqual(TEXT("L_Level03 should have 4 doors connecting its 5 rooms in a chain"), Doors.Num(), 4);

	TArray<ALevelLightingRigActor*> LightingRigs;
	for (TActorIterator<ALevelLightingRigActor> It(World); It; ++It)
	{
		LightingRigs.Add(*It);
	}
	TestEqual(TEXT("L_Level03 should have exactly one ALevelLightingRigActor placed (issue #186, PRD REQ-2)"),
		LightingRigs.Num(), 1);

	KrowdKontrolLevelTestUtils::CheckAllRoomsReachableViaDoors(*this, Rooms, Doors);
	KrowdKontrolLevelTestUtils::CheckDoorsHaveVisibleMarker(*this, Doors);

	TMap<ARoomActor*, TSet<EEnemyType>> EnemyTypesByRoom;
	TMap<ARoomActor*, int32> EnemyCountByRoom;
	int32 TotalLevel03EnemyCount = 0;
	for (TActorIterator<AEnemyBase> It(World); It; ++It)
	{
		AEnemyBase* Enemy = *It;
		++TotalLevel03EnemyCount;
		ARoomActor* NearestRoom = KrowdKontrolLevelTestUtils::FindNearestRoom(Enemy, Rooms);
		if (!NearestRoom)
		{
			continue;
		}
		EnemyCountByRoom.FindOrAdd(NearestRoom, 0)++;
		if (TOptional<EEnemyType> EnemyType = KrowdKontrolLevelTestUtils::GetPlacedEnemyType(Enemy))
		{
			EnemyTypesByRoom.FindOrAdd(NearestRoom).Add(EnemyType.GetValue());
		}
	}

	KrowdKontrolLevelTestUtils::CheckRoomTargetZonesAndDensity(*this, Rooms, EnemyTypesByRoom, EnemyCountByRoom);
	KrowdKontrolLevelTestUtils::CheckRoomBankingZonesSelfHeal(*this, Rooms);

	TestTrue(TEXT("L_Level03's total enemy count should be strictly greater than L_Level02's (REQ-3 difficulty ramp)"),
		TotalLevel03EnemyCount > Level02EnemyCount);
	// See the room-count comment above: same rationale applies to this exact-count
	// check alongside the dynamic difficulty-ramp comparison.
	TestEqual(TEXT("L_Level03 total enemy count should match its design target of 10"), TotalLevel03EnemyCount, 10);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
