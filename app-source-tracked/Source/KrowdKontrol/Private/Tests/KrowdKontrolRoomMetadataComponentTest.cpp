// Confirms URoomMetadataComponent (issue #49, PRD 05 REQ-4) attaches to a real
// ARoomActor and every field (EnemyTypeBudget, TargetZoneCounts, DifficultyTier,
// RequiredAbility) round-trips: set, then read back correctly. Metadata storage only
// - no shuffling/sequencing logic is exercised here, per the issue's explicit scope.
//
// Needs a real UWorld to spawn an ARoomActor into, same rationale as
// KrowdKontrolRoomActorTest.cpp - FAutomationEditorCommonUtils::CreateNewMap() gives
// a real editor UWorld without needing PIE.
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the other
// KrowdKontrol.Unit.* tests.

#include "Misc/AutomationTest.h"
#include "RoomActor.h"
#include "RoomMetadataComponent.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FRoomEnemyTypeCount* FindEntry(TArray<FRoomEnemyTypeCount>& Entries, EEnemyType Type)
	{
		return Entries.FindByPredicate([Type](const FRoomEnemyTypeCount& Entry) { return Entry.EnemyType == Type; });
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolRoomMetadataComponentTest,
	"KrowdKontrol.Unit.RoomMetadataComponent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolRoomMetadataComponentTest::RunTest(const FString& Parameters)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
	{
		return false;
	}

	ARoomActor* Room = World->SpawnActor<ARoomActor>();
	if (!TestNotNull(TEXT("ARoomActor should spawn into the test World"), Room))
	{
		return false;
	}

	URoomMetadataComponent* Component = NewObject<URoomMetadataComponent>(Room);
	Component->RegisterComponent();
	if (!TestNotNull(TEXT("URoomMetadataComponent should construct"), Component))
	{
		return false;
	}
	TestEqual(TEXT("Component's owner should be the room it was attached to"), Component->GetOwner(), static_cast<AActor*>(Room));

	// (a) Default state: all 4 locked enemy types present at 0, Easy tier, no ability gate.
	TestEqual(TEXT("EnemyTypeBudget should start with one entry per locked enemy type"), Component->EnemyTypeBudget.Num(), 4);
	TestEqual(TEXT("TargetZoneCounts should start with one entry per locked enemy type"), Component->TargetZoneCounts.Num(), 4);
	TestEqual(TEXT("DifficultyTier should default to Easy"), static_cast<uint8>(Component->DifficultyTier), static_cast<uint8>(ERoomDifficultyTier::Easy));
	TestEqual(TEXT("RequiredAbility should default to None"), static_cast<uint8>(Component->RequiredAbility), static_cast<uint8>(ERoomAbilityGate::None));

	for (EEnemyType Type : { EEnemyType::RU_NNR, EEnemyType::TR_UPR, EEnemyType::B0_0MR, EEnemyType::SN_1PR })
	{
		FRoomEnemyTypeCount* BudgetDefault = FindEntry(Component->EnemyTypeBudget, Type);
		TestNotNull(TEXT("EnemyTypeBudget should contain an entry for every locked enemy type"), BudgetDefault);
		if (BudgetDefault)
		{
			TestEqual(TEXT("Default EnemyTypeBudget entry should start at count 0"), BudgetDefault->Count, 0);
		}

		FRoomEnemyTypeCount* ZoneDefault = FindEntry(Component->TargetZoneCounts, Type);
		TestNotNull(TEXT("TargetZoneCounts should contain an entry for every locked enemy type"), ZoneDefault);
		if (ZoneDefault)
		{
			TestEqual(TEXT("Default TargetZoneCounts entry should start at count 0"), ZoneDefault->Count, 0);
		}
	}

	// (b) Set every field, then read each back.
	FRoomEnemyTypeCount* BudgetEntry = FindEntry(Component->EnemyTypeBudget, EEnemyType::TR_UPR);
	if (!TestNotNull(TEXT("EnemyTypeBudget should contain an entry for TR_UPR"), BudgetEntry))
	{
		return false;
	}
	BudgetEntry->Count = 5;

	FRoomEnemyTypeCount* ZoneEntry = FindEntry(Component->TargetZoneCounts, EEnemyType::B0_0MR);
	if (!TestNotNull(TEXT("TargetZoneCounts should contain an entry for B0_0MR"), ZoneEntry))
	{
		return false;
	}
	ZoneEntry->Count = 2;

	Component->DifficultyTier = ERoomDifficultyTier::Hard;
	Component->RequiredAbility = ERoomAbilityGate::Snare;

	TestEqual(TEXT("EnemyTypeBudget's TR_UPR count should read back as set"), FindEntry(Component->EnemyTypeBudget, EEnemyType::TR_UPR)->Count, 5);
	TestEqual(TEXT("TargetZoneCounts's B0_0MR count should read back as set"), FindEntry(Component->TargetZoneCounts, EEnemyType::B0_0MR)->Count, 2);
	TestEqual(TEXT("DifficultyTier should read back as Hard"), static_cast<uint8>(Component->DifficultyTier), static_cast<uint8>(ERoomDifficultyTier::Hard));
	TestEqual(TEXT("RequiredAbility should read back as Snare"), static_cast<uint8>(Component->RequiredAbility), static_cast<uint8>(ERoomAbilityGate::Snare));

	// (c) None is itself a valid, round-trippable value - not just the default.
	Component->RequiredAbility = ERoomAbilityGate::None;
	TestEqual(TEXT("RequiredAbility should round-trip back to None after being set away from it"), static_cast<uint8>(Component->RequiredAbility), static_cast<uint8>(ERoomAbilityGate::None));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
