// Confirms L_Level04 (issue #367, docs/prd-levels-4-5.md REQ-1) - the next Alpha level
// after L_Level03 (issue #45) - loads without errors and is measurably harder than
// L_Level03: strictly more rooms and strictly more enemies, computed dynamically
// against the live L_Level03 asset rather than a hardcoded magic number, so the
// comparison can't silently drift if L_Level03 is ever revised.
//
// Uses FAutomationEditorCommonUtils::LoadMap (not CreateNewMap) because this is
// regression coverage for the shipped level assets themselves, not the ARoomActor/
// ADoorConnectorActor classes in isolation - mirrors KrowdKontrolLevel03Test.cpp's
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
	FKrowdKontrolLevel04StructureTest,
	"KrowdKontrol.Unit.Level04Structure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolLevel04StructureTest::RunTest(const FString& Parameters)
{
	// Load L_Level03 first to gather its room/enemy counts as the live baseline the
	// difficulty ramp (REQ-3) must exceed - computed here, not hardcoded, so this test
	// can't silently drift if L_Level03 is ever revised.
	FAutomationEditorCommonUtils::LoadMap(TEXT("/Game/Maps/L_Level03"));

	UWorld* Level03World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!TestNotNull(TEXT("L_Level03 should load into a valid World"), Level03World))
	{
		return false;
	}

	int32 Level03RoomCount = 0;
	for (TActorIterator<ARoomActor> It(Level03World); It; ++It)
	{
		++Level03RoomCount;
	}

	int32 Level03EnemyCount = 0;
	for (TActorIterator<AEnemyBase> It(Level03World); It; ++It)
	{
		++Level03EnemyCount;
	}

	FAutomationEditorCommonUtils::LoadMap(TEXT("/Game/Maps/L_Level04"));

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!TestNotNull(TEXT("L_Level04 should load into a valid World"), World))
	{
		return false;
	}

	TArray<ARoomActor*> Rooms;
	for (TActorIterator<ARoomActor> It(World); It; ++It)
	{
		Rooms.Add(*It);
	}
	TestTrue(TEXT("L_Level04's room count should be strictly greater than L_Level03's (REQ-3 difficulty ramp)"),
		Rooms.Num() > Level03RoomCount);
	// Exact-count check is intentionally in addition to (not instead of) the dynamic
	// comparison above: L_Level04 is a hand-authored level with a fixed design target
	// per the PRD, not a procedurally generated one, so "6" here isn't a magic number
	// at risk of drifting relative to some other live value - it's this specific
	// level's own spec. The dynamic check guards the REQ-3 difficulty ramp against
	// L_Level03 changing; this check guards L_Level04 itself against an unintended
	// room being added/removed.
	TestEqual(TEXT("L_Level04 room count should match its design target of 6"), Rooms.Num(), 6);

	TArray<ADoorConnectorActor*> Doors;
	for (TActorIterator<ADoorConnectorActor> It(World); It; ++It)
	{
		Doors.Add(*It);
	}
	TestEqual(TEXT("L_Level04 should have 5 doors connecting its 6 rooms in a chain"), Doors.Num(), 5);

	TArray<ALevelLightingRigActor*> LightingRigs;
	for (TActorIterator<ALevelLightingRigActor> It(World); It; ++It)
	{
		LightingRigs.Add(*It);
	}
	TestEqual(TEXT("L_Level04 should have exactly one ALevelLightingRigActor placed (issue #186, PRD REQ-2)"),
		LightingRigs.Num(), 1);

	KrowdKontrolLevelTestUtils::CheckAllRoomsReachableViaDoors(*this, Rooms, Doors);
	KrowdKontrolLevelTestUtils::CheckDoorsHaveVisibleMarker(*this, Doors);

	TMap<ARoomActor*, TSet<EEnemyType>> EnemyTypesByRoom;
	TMap<ARoomActor*, int32> EnemyCountByRoom;
	int32 TotalLevel04EnemyCount = 0;
	for (TActorIterator<AEnemyBase> It(World); It; ++It)
	{
		AEnemyBase* Enemy = *It;
		++TotalLevel04EnemyCount;
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

	// Issue #31: Level 4's entrance room must be a forced-safe solo Bomber
	// encounter, since Fear (unlocked on arrival in Level 4) counters B0_0MR
	// (AbilityData.cpp).
	KrowdKontrolLevelTestUtils::CheckSoloEncounterForCounteredType(
		*this, Rooms, EnemyCountByRoom, EnemyTypesByRoom, EEnemyType::B0_0MR);

	KrowdKontrolLevelTestUtils::CheckRoomBankingZonesSelfHeal(*this, Rooms);

	TestTrue(TEXT("L_Level04's total enemy count should be strictly greater than L_Level03's (REQ-3 difficulty ramp)"),
		TotalLevel04EnemyCount > Level03EnemyCount);
	// See the room-count comment above: same rationale applies to this exact-count
	// check alongside the dynamic difficulty-ramp comparison.
	TestEqual(TEXT("L_Level04 total enemy count should match its design target of 12"), TotalLevel04EnemyCount, 12);

	// All four core enemy types (Hard Invariant #5) must be present somewhere in the
	// level (issue #367 AC) - Level03's own 5-room chain only guarantees this if the
	// cycling pattern happens to span all 4 types, so Level04's own 6-room chain
	// checks it explicitly rather than assuming the ramp keeps it true.
	TSet<EEnemyType> AllPlacedTypes;
	for (const TPair<ARoomActor*, TSet<EEnemyType>>& Pair : EnemyTypesByRoom)
	{
		AllPlacedTypes.Append(Pair.Value);
	}
	TestEqual(TEXT("L_Level04 should place all four core enemy types somewhere in the level (issue #367)"),
		AllPlacedTypes.Num(), 4);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
