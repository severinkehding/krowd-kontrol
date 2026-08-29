// Confirms UCrowdMasteryTotalSubsystem's spend/prerequisite/refund API
// (docs/prd-mastery-skill-tree.md REQ-1, issue #371): TrySpendOnBubble mutates state
// only on success, IsPrerequisiteMet reflects MasteryTreeData.h's "parent node
// reached" rule, and RefundAllAndClearUnlocks performs a full, exact-restoration
// respec without touching the earned AccumulatedTotal.
//
// No UWorld/CreateNewMap() needed - same rationale
// KrowdKontrolCrowdMasteryTotalSubsystemTest.cpp documents: this subsystem's public
// API never calls GetWorld() or GetGameInstance(), so it's constructed directly via
// NewObject<>(), with MasteryTreeTable assigned an in-code fixture UDataTable rather
// than the real content asset. DepositRunMastery() write-throughs to the same shared
// on-disk save slot, so this test cleans it up before and after, matching that
// sibling test's own precedent.

#include "Misc/AutomationTest.h"
#include "CrowdMasteryTotalSubsystem.h"
#include "MasteryTreeData.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "LevelClearTimeSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolCrowdMasteryTotalSubsystemSpendTest,
	"KrowdKontrol.Unit.CrowdMasteryTotalSubsystemSpend",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

namespace KrowdKontrolCrowdMasteryTotalSubsystemSpendTest
{
	// Builds a 4-entry Bubbles array costing 1/2/3/4 points, named "<Prefix>_Bubble0..3" -
	// mirrors KrowdKontrolMasteryTreeDataTest.cpp's BuildFourBubbles() shape.
	TArray<FMasterySkillBubble> BuildFourBubbles(const FString& Prefix)
	{
		TArray<FMasterySkillBubble> Bubbles;
		for (int32 Index = 0; Index < 4; ++Index)
		{
			FMasterySkillBubble Bubble;
			Bubble.BubbleId = FName(*FString::Printf(TEXT("%s_Bubble%d"), *Prefix, Index));
			Bubble.DisplayName = FText::FromString(FString::Printf(TEXT("%s Skill %d"), *Prefix, Index));
			Bubble.PointCost = Index + 1;
			Bubble.EffectHookId = FName(*FString::Printf(TEXT("%s_Effect%d"), *Prefix, Index));
			Bubbles.Add(Bubble);
		}
		return Bubbles;
	}
}

bool FKrowdKontrolCrowdMasteryTotalSubsystemSpendTest::RunTest(const FString& Parameters)
{
	using namespace KrowdKontrolCrowdMasteryTotalSubsystemSpendTest;

	UGameplayStatics::DeleteGameInSlot(ULevelClearTimeSubsystem::SaveSlotName, 0);

	UGameInstance* GameInstanceOuter = NewObject<UGameInstance>();
	UCrowdMasteryTotalSubsystem* Subsystem = NewObject<UCrowdMasteryTotalSubsystem>(GameInstanceOuter);
	if (!TestNotNull(TEXT("UCrowdMasteryTotalSubsystem should construct"), Subsystem))
	{
		UGameplayStatics::DeleteGameInSlot(ULevelClearTimeSubsystem::SaveSlotName, 0);
		return false;
	}

	// Fixture tree: a root node (no prerequisite) and a child node whose prerequisite
	// is the root, each with 4 bubbles costing 1/2/3/4 points.
	UDataTable* Table = NewObject<UDataTable>();
	Table->RowStruct = FMasteryTreeNode::StaticStruct();

	FMasteryTreeNode RootRow;
	RootRow.ParentNodeId = NAME_None;
	RootRow.Phase = EMasteryTreePhase::Phase1;
	RootRow.Bubbles = BuildFourBubbles(TEXT("Root"));
	Table->AddRow(FName(TEXT("Node_Root")), RootRow);

	FMasteryTreeNode ChildRow;
	ChildRow.ParentNodeId = FName(TEXT("Node_Root"));
	ChildRow.Phase = EMasteryTreePhase::Phase2;
	ChildRow.Bubbles = BuildFourBubbles(TEXT("Child"));
	Table->AddRow(FName(TEXT("Node_Child")), ChildRow);

	Subsystem->MasteryTreeTable = Table;

	// Seed 5 earned points - enough to afford some but not all of the root's 4
	// bubbles (1+2+3+4=10), giving a real insufficient-points boundary.
	Subsystem->DepositRunMastery(5);

	const FName ChildBubble0(TEXT("Child_Bubble0"));
	const FName RootBubble0(TEXT("Root_Bubble0"));
	const FName RootBubble1(TEXT("Root_Bubble1"));
	const FName RootBubble3(TEXT("Root_Bubble3"));

	// Prerequisite rejection: child's prerequisite (root reached) is not yet met.
	TestFalse(TEXT("Child bubble's prerequisite should not be met before any root bubble is unlocked"),
		Subsystem->IsPrerequisiteMet(ChildBubble0));
	TestFalse(TEXT("Spending on a bubble whose prerequisite is not met should fail"),
		Subsystem->TrySpendOnBubble(ChildBubble0));
	TestEqual(TEXT("A rejected spend should not mutate SpentPoints"), Subsystem->GetSpentPoints(), 0);
	TestEqual(TEXT("A rejected spend should not mutate UnlockedBubbles"), Subsystem->GetUnlockedBubbles().Num(), 0);

	// Successful spend: root bubble has no prerequisite.
	TestTrue(TEXT("Spending on a root bubble (no prerequisite) should succeed"),
		Subsystem->TrySpendOnBubble(RootBubble0));
	TestEqual(TEXT("A successful spend should add the bubble's cost to SpentPoints"), Subsystem->GetSpentPoints(), 1);
	TestTrue(TEXT("GetUnlockedBubbles should contain the newly-unlocked bubble"),
		Subsystem->GetUnlockedBubbles().Contains(RootBubble0));

	// Prerequisite now met.
	TestTrue(TEXT("Child bubble's prerequisite should be met once the root has an unlocked bubble"),
		Subsystem->IsPrerequisiteMet(ChildBubble0));

	// Boundary spend then insufficient-points rejection.
	TestTrue(TEXT("Spending the remaining balance exactly should succeed"),
		Subsystem->TrySpendOnBubble(RootBubble3));
	TestEqual(TEXT("SpentPoints should now exactly equal the earned total"), Subsystem->GetSpentPoints(), 5);
	TestFalse(TEXT("Spending beyond the available balance should fail"),
		Subsystem->TrySpendOnBubble(RootBubble1));
	TestEqual(TEXT("A rejected insufficient-points spend should not mutate SpentPoints"), Subsystem->GetSpentPoints(), 5);
	TestEqual(TEXT("A rejected insufficient-points spend should not mutate UnlockedBubbles"), Subsystem->GetUnlockedBubbles().Num(), 2);

	// Already-unlocked rejection.
	TestFalse(TEXT("Spending on an already-unlocked bubble again should fail"),
		Subsystem->TrySpendOnBubble(RootBubble0));
	TestEqual(TEXT("A rejected already-unlocked spend should not mutate SpentPoints"), Subsystem->GetSpentPoints(), 5);

	// Unknown-bubble rejection.
	TestFalse(TEXT("Spending on an unknown BubbleId should fail, not crash"),
		Subsystem->TrySpendOnBubble(FName(TEXT("DoesNotExist"))));

	// Full respec.
	Subsystem->RefundAllAndClearUnlocks();
	TestEqual(TEXT("RefundAllAndClearUnlocks should zero SpentPoints"), Subsystem->GetSpentPoints(), 0);
	TestEqual(TEXT("RefundAllAndClearUnlocks should clear UnlockedBubbles"), Subsystem->GetUnlockedBubbles().Num(), 0);
	TestEqual(TEXT("RefundAllAndClearUnlocks should leave the earned total untouched"), Subsystem->GetAccumulatedTotal(), 5);

	// Refund actually restores capacity - re-spend the full total after the refund.
	TestTrue(TEXT("Re-spending after a refund should succeed (capacity was fully restored)"),
		Subsystem->TrySpendOnBubble(RootBubble3));
	TestTrue(TEXT("Re-spending the remaining balance after a refund should succeed"),
		Subsystem->TrySpendOnBubble(RootBubble0));
	TestEqual(TEXT("SpentPoints should again exactly equal the earned total after re-spending post-refund"),
		Subsystem->GetSpentPoints(), Subsystem->GetAccumulatedTotal());

	UGameplayStatics::DeleteGameInSlot(ULevelClearTimeSubsystem::SaveSlotName, 0);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
