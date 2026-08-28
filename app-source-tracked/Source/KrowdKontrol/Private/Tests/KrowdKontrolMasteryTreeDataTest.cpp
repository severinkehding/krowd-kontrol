// Confirms issue #370 (docs/prd-mastery-skill-tree.md REQ-1, node/bubble data-schema
// half): FMasteryTreeNode/FMasterySkillBubble round-trip through a UDataTable exactly
// like every other DataTable-row struct in this module, and the real authored
// /Game/Data/DT_MasteryTreeTable content asset actually satisfies the issue's
// acceptance criteria (>=2 rows, 4 bubbles each, a real parent/child prerequisite
// link) rather than just compiling.
//
// No UWorld/CreateNewMap() needed - this is pure DataTable/reflection data, no
// subsystem, no BeginPlay, same as KrowdKontrolAbilityDataTest.cpp.
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the other
// KrowdKontrol.Unit.* tests.

#include "Misc/AutomationTest.h"
#include "MasteryTreeData.h"
#include "Engine/DataTable.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolMasteryTreeDataTest,
	"KrowdKontrol.Unit.MasteryTreeData",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

namespace KrowdKontrolMasteryTreeDataTest
{
	// Builds a 4-entry Bubbles array so both cases below can construct nodes without
	// repeating the same 4 FMasterySkillBubble literals inline.
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

bool FKrowdKontrolMasteryTreeDataTest::RunTest(const FString& Parameters)
{
	using namespace KrowdKontrolMasteryTreeDataTest;

	// (a) In-code round-trip: a root node (ParentNodeId = NAME_None) and a child node
	// whose ParentNodeId is the root's row name, each with 4 bubbles, must round-trip
	// every field exactly through AddRow()/FindRow().
	{
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

		const FMasteryTreeNode* FoundRoot = Table->FindRow<FMasteryTreeNode>(FName(TEXT("Node_Root")), TEXT("MasteryTreeDataTest"));
		if (TestNotNull(TEXT("Node_Root should round-trip through the DataTable"), FoundRoot))
		{
			TestEqual(TEXT("Root ParentNodeId should round-trip as NAME_None"), FoundRoot->ParentNodeId, FName(NAME_None));
			TestEqual(TEXT("Root Phase should round-trip as Phase1"), static_cast<uint8>(FoundRoot->Phase), static_cast<uint8>(EMasteryTreePhase::Phase1));
			TestEqual(TEXT("Root should have exactly 4 bubbles"), FoundRoot->Bubbles.Num(), 4);

			for (int32 Index = 0; Index < FoundRoot->Bubbles.Num(); ++Index)
			{
				const FMasterySkillBubble& Bubble = FoundRoot->Bubbles[Index];
				TestEqual(*FString::Printf(TEXT("Root bubble %d BubbleId should round-trip"), Index),
					Bubble.BubbleId, FName(*FString::Printf(TEXT("Root_Bubble%d"), Index)));
				TestEqual(*FString::Printf(TEXT("Root bubble %d DisplayName should round-trip"), Index),
					Bubble.DisplayName.ToString(), FString::Printf(TEXT("Root Skill %d"), Index));
				TestEqual(*FString::Printf(TEXT("Root bubble %d PointCost should round-trip"), Index),
					Bubble.PointCost, Index + 1);
				TestEqual(*FString::Printf(TEXT("Root bubble %d EffectHookId should round-trip"), Index),
					Bubble.EffectHookId, FName(*FString::Printf(TEXT("Root_Effect%d"), Index)));
			}
		}

		const FMasteryTreeNode* FoundChild = Table->FindRow<FMasteryTreeNode>(FName(TEXT("Node_Child")), TEXT("MasteryTreeDataTest"));
		if (TestNotNull(TEXT("Node_Child should round-trip through the DataTable"), FoundChild))
		{
			TestEqual(TEXT("Child ParentNodeId should round-trip to the root's row name"), FoundChild->ParentNodeId, FName(TEXT("Node_Root")));
			TestEqual(TEXT("Child Phase should round-trip as Phase2"), static_cast<uint8>(FoundChild->Phase), static_cast<uint8>(EMasteryTreePhase::Phase2));
			TestEqual(TEXT("Child should have exactly 4 bubbles"), FoundChild->Bubbles.Num(), 4);
		}
	}

	// (b) Real-asset shape: the authored placeholder DT_MasteryTreeTable content asset
	// must actually satisfy the acceptance criteria - at least 2 rows, exactly 4
	// bubbles per row, and at least one real parent/child prerequisite link (not 2
	// disconnected roots). A missing asset fails loudly via TestNotNull, which is the
	// intended signal - not a reason to weaken this to a soft skip.
	{
		UDataTable* RealTable = LoadObject<UDataTable>(nullptr, TEXT("/Game/Data/DT_MasteryTreeTable.DT_MasteryTreeTable"));
		if (TestNotNull(TEXT("DT_MasteryTreeTable should load from Content"), RealTable))
		{
			const TMap<FName, uint8*>& RowMap = RealTable->GetRowMap();
			TestTrue(TEXT("DT_MasteryTreeTable should have at least 2 rows"), RowMap.Num() >= 2);

			TSet<FName> SeenBubbleIds;
			bool bFoundRealParentLink = false;
			for (const TPair<FName, uint8*>& RowPair : RowMap)
			{
				const FMasteryTreeNode* Node = reinterpret_cast<const FMasteryTreeNode*>(RowPair.Value);
				TestEqual(*FString::Printf(TEXT("Row %s should have exactly 4 bubbles"), *RowPair.Key.ToString()),
					Node->Bubbles.Num(), 4);

				for (const FMasterySkillBubble& Bubble : Node->Bubbles)
				{
					TestNotEqual(*FString::Printf(TEXT("Row %s bubble %s EffectHookId should not be empty"), *RowPair.Key.ToString(), *Bubble.BubbleId.ToString()),
						Bubble.EffectHookId, FName(NAME_None));
					TestFalse(*FString::Printf(TEXT("Row %s bubble %s DisplayName should not be empty"), *RowPair.Key.ToString(), *Bubble.BubbleId.ToString()),
						Bubble.DisplayName.IsEmpty());

					bool bAlreadySeen = false;
					SeenBubbleIds.Add(Bubble.BubbleId, &bAlreadySeen);
					TestFalse(*FString::Printf(TEXT("BubbleId %s should be unique across the tree"), *Bubble.BubbleId.ToString()), bAlreadySeen);
				}

				if (Node->ParentNodeId != NAME_None)
				{
					TestTrue(*FString::Printf(TEXT("Row %s ParentNodeId '%s' should resolve to another row"),
						*RowPair.Key.ToString(), *Node->ParentNodeId.ToString()),
						RowMap.Contains(Node->ParentNodeId));
					bFoundRealParentLink = true;
				}
			}

			TestTrue(TEXT("At least one row's ParentNodeId should resolve to another row's name"), bFoundRealParentLink);
		}
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
