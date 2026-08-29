// Confirms L_Level05 (issue #368, docs/prd-levels-4-5.md REQ-2) - the fifth and final
// Alpha level after L_Level04 (issue #367) - loads without errors and is measurably
// harder than L_Level04: strictly more rooms and strictly more enemies, computed
// dynamically against the live L_Level04 asset rather than a hardcoded magic number, so
// the comparison can't silently drift if L_Level04 is ever revised.
//
// Uses FAutomationEditorCommonUtils::LoadMap (not CreateNewMap) because this is
// regression coverage for the shipped level assets themselves, not the ARoomActor/
// ADoorConnectorActor classes in isolation - mirrors KrowdKontrolLevel04Test.cpp's
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
	FKrowdKontrolLevel05StructureTest,
	"KrowdKontrol.Unit.Level05Structure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolLevel05StructureTest::RunTest(const FString& Parameters)
{
	// Load L_Level04 first to gather its room/enemy counts as the live baseline the
	// difficulty ramp (REQ-3) must exceed - computed here, not hardcoded, so this test
	// can't silently drift if L_Level04 is ever revised.
	FAutomationEditorCommonUtils::LoadMap(TEXT("/Game/Maps/L_Level04"));

	UWorld* Level04World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!TestNotNull(TEXT("L_Level04 should load into a valid World"), Level04World))
	{
		return false;
	}

	int32 Level04RoomCount = 0;
	for (TActorIterator<ARoomActor> It(Level04World); It; ++It)
	{
		++Level04RoomCount;
	}

	int32 Level04EnemyCount = 0;
	for (TActorIterator<AEnemyBase> It(Level04World); It; ++It)
	{
		++Level04EnemyCount;
	}

	FAutomationEditorCommonUtils::LoadMap(TEXT("/Game/Maps/L_Level05"));

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!TestNotNull(TEXT("L_Level05 should load into a valid World"), World))
	{
		return false;
	}

	TArray<ARoomActor*> Rooms;
	for (TActorIterator<ARoomActor> It(World); It; ++It)
	{
		Rooms.Add(*It);
	}
	TestTrue(TEXT("L_Level05's room count should be strictly greater than L_Level04's (REQ-3 difficulty ramp)"),
		Rooms.Num() > Level04RoomCount);
	// Exact-count check is intentionally in addition to (not instead of) the dynamic
	// comparison above: L_Level05 is a hand-authored level with a fixed design target
	// per the PRD, not a procedurally generated one, so "7" here isn't a magic number
	// at risk of drifting relative to some other live value - it's this specific
	// level's own spec. The dynamic check guards the REQ-3 difficulty ramp against
	// L_Level04 changing; this check guards L_Level05 itself against an unintended
	// room being added/removed.
	TestEqual(TEXT("L_Level05 room count should match its design target of 7"), Rooms.Num(), 7);

	TArray<ADoorConnectorActor*> Doors;
	for (TActorIterator<ADoorConnectorActor> It(World); It; ++It)
	{
		Doors.Add(*It);
	}
	TestEqual(TEXT("L_Level05 should have 6 doors connecting its 7 rooms in a chain"), Doors.Num(), 6);

	TArray<ALevelLightingRigActor*> LightingRigs;
	for (TActorIterator<ALevelLightingRigActor> It(World); It; ++It)
	{
		LightingRigs.Add(*It);
	}
	TestEqual(TEXT("L_Level05 should have exactly one ALevelLightingRigActor placed (issue #186, PRD REQ-2)"),
		LightingRigs.Num(), 1);

	KrowdKontrolLevelTestUtils::CheckAllRoomsReachableViaDoors(*this, Rooms, Doors);
	KrowdKontrolLevelTestUtils::CheckDoorsHaveVisibleMarker(*this, Doors);

	TMap<ARoomActor*, TSet<EEnemyType>> EnemyTypesByRoom;
	TMap<ARoomActor*, int32> EnemyCountByRoom;
	int32 TotalLevel05EnemyCount = 0;
	for (TActorIterator<AEnemyBase> It(World); It; ++It)
	{
		AEnemyBase* Enemy = *It;
		++TotalLevel05EnemyCount;
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

	TestTrue(TEXT("L_Level05's total enemy count should be strictly greater than L_Level04's (REQ-3 difficulty ramp)"),
		TotalLevel05EnemyCount > Level04EnemyCount);
	// See the room-count comment above: same rationale applies to this exact-count
	// check alongside the dynamic difficulty-ramp comparison.
	TestEqual(TEXT("L_Level05 total enemy count should match its design target of 14"), TotalLevel05EnemyCount, 14);

	// All four core enemy types (Hard Invariant #5) must be present somewhere in the
	// level (issue #368 AC) - Level04's own 6-room chain only guarantees this if the
	// cycling pattern happens to span all 4 types, so Level05's own 7-room chain
	// checks it explicitly rather than assuming the ramp keeps it true.
	TSet<EEnemyType> AllPlacedTypes;
	for (const TPair<ARoomActor*, TSet<EEnemyType>>& Pair : EnemyTypesByRoom)
	{
		AllPlacedTypes.Append(Pair.Value);
	}
	TestEqual(TEXT("L_Level05 should place all four core enemy types somewhere in the level (issue #368)"),
		AllPlacedTypes.Num(), 4);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
