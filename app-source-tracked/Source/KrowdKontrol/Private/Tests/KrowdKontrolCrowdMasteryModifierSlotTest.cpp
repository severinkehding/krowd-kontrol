// Confirms UCrowdMasteryTotalSubsystem's modifier grant/slot/unslot API (issue #376,
// docs/prd-mastery-skill-tree.md REQ-4): GrantModifier/TrySlotModifier/UnslotModifier
// mutate state only on success, TrySlotModifier's category rule is anti-duplication
// across a bubble's own slots (not fixed-per-slot-category - see that function's doc
// comment), and RefundAllAndClearUnlocks() clears slotted modifiers while leaving the
// owned-modifiers inventory untouched (PRD REQ-5).
//
// No UWorld/CreateNewMap() needed - same rationale
// KrowdKontrolCrowdMasteryTotalSubsystemSpendTest.cpp documents: this subsystem's
// public API never calls GetWorld() or GetGameInstance(), so it's constructed
// directly via NewObject<>(), with MasteryTreeTable/ModifierCatalogTable assigned
// in-code fixture UDataTables rather than the real content assets. DepositRunMastery()
// write-throughs to the same shared on-disk save slot, so this test cleans it up
// before and after, matching that sibling test's own precedent.

#include "Misc/AutomationTest.h"
#include "CrowdMasteryTotalSubsystem.h"
#include "MasteryTreeData.h"
#include "ModifierData.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "LevelClearTimeSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolCrowdMasteryModifierSlotTest,
	"KrowdKontrol.Unit.CrowdMasteryModifierSlot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

namespace KrowdKontrolCrowdMasteryModifierSlotTest
{
	// Builds a 4-entry Bubbles array costing 1/2/3/4 points, named "<Prefix>_Bubble0..3" -
	// mirrors KrowdKontrolCrowdMasteryTotalSubsystemSpendTest.cpp's BuildFourBubbles() shape.
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

	// Builds a single modifier row - lets the fixture below stay a flat list of
	// (Id, Category, Tier) triples instead of repeating FMasteryModifierRow literals.
	FMasteryModifierRow BuildModifierRow(EModifierCategory Category, EModifierTier Tier, const FString& DisplayName, const FString& EffectHookId)
	{
		FMasteryModifierRow Row;
		Row.Category = Category;
		Row.Tier = Tier;
		Row.DisplayName = FText::FromString(DisplayName);
		Row.EffectHookId = FName(*EffectHookId);
		return Row;
	}
}

bool FKrowdKontrolCrowdMasteryModifierSlotTest::RunTest(const FString& Parameters)
{
	using namespace KrowdKontrolCrowdMasteryModifierSlotTest;

	UGameplayStatics::DeleteGameInSlot(ULevelClearTimeSubsystem::SaveSlotName, 0);

	UGameInstance* GameInstanceOuter = NewObject<UGameInstance>();
	UCrowdMasteryTotalSubsystem* Subsystem = NewObject<UCrowdMasteryTotalSubsystem>(GameInstanceOuter);
	if (!TestNotNull(TEXT("UCrowdMasteryTotalSubsystem should construct"), Subsystem))
	{
		UGameplayStatics::DeleteGameInSlot(ULevelClearTimeSubsystem::SaveSlotName, 0);
		return false;
	}

	// Unset-table fail-safe: every guarded entry point should fail closed, not
	// crash, while ModifierCatalogTable is still unassigned - mirrors
	// KrowdKontrolCrowdMasteryTotalSubsystemSpendTest.cpp's MasteryTreeTable check.
	// TrySlotModifier isn't independently assertable here since it also requires an
	// unlocked bubble, which itself requires MasteryTreeTable; GrantModifier/
	// GetOwnedModifiers alone are sufficient to exercise HasModifierCatalogTable()'s
	// false branch, matching the granularity the sibling test uses for MasteryTreeTable.
	TestFalse(TEXT("GrantModifier should fail closed when ModifierCatalogTable is unset"),
		Subsystem->GrantModifier(FName(TEXT("Mod_SurvivalA"))));
	TestEqual(TEXT("GetOwnedModifiers should be empty when ModifierCatalogTable is unset"),
		Subsystem->GetOwnedModifiers().Num(), 0);

	// Fixture tree: a single root node (no prerequisite) with 4 bubbles costing
	// 1/2/3/4 points - Bubble0/2/3 get unlocked for the scenarios below, Bubble1 is
	// deliberately left never-unlocked (scenario 2).
	UDataTable* TreeTable = NewObject<UDataTable>();
	TreeTable->RowStruct = FMasteryTreeNode::StaticStruct();
	FMasteryTreeNode RootRow;
	RootRow.ParentNodeId = NAME_None;
	RootRow.Phase = EMasteryTreePhase::Phase1;
	RootRow.Bubbles = BuildFourBubbles(TEXT("Root"));
	TreeTable->AddRow(FName(TEXT("Node_Root")), RootRow);
	Subsystem->MasteryTreeTable = TreeTable;

	const FName BubbleA(TEXT("Root_Bubble0"));
	const FName BubbleNeverUnlocked(TEXT("Root_Bubble1"));
	const FName BubbleFull(TEXT("Root_Bubble2"));
	const FName BubbleCategory(TEXT("Root_Bubble3"));

	// Fixture modifier catalog: 2 SurvivalType (for the category-duplicate case), 1
	// each of AttackType/AbilityType/ItemType. Mod_NeverGranted is deliberately
	// never passed to GrantModifier (scenario 7's not-owned rejection).
	UDataTable* ModifierTable = NewObject<UDataTable>();
	ModifierTable->RowStruct = FMasteryModifierRow::StaticStruct();
	const FName ModSurvivalA(TEXT("Mod_SurvivalA"));
	const FName ModSurvivalB(TEXT("Mod_SurvivalB"));
	const FName ModAttack(TEXT("Mod_Attack"));
	const FName ModAbility(TEXT("Mod_Ability"));
	const FName ModNeverGranted(TEXT("Mod_NeverGranted"));
	ModifierTable->AddRow(ModSurvivalA, BuildModifierRow(EModifierCategory::SurvivalType, EModifierTier::TierI, TEXT("Survival A"), TEXT("SurvivalA")));
	ModifierTable->AddRow(ModSurvivalB, BuildModifierRow(EModifierCategory::SurvivalType, EModifierTier::TierI, TEXT("Survival B"), TEXT("SurvivalB")));
	ModifierTable->AddRow(ModAttack, BuildModifierRow(EModifierCategory::AttackType, EModifierTier::TierI, TEXT("Attack"), TEXT("Attack")));
	ModifierTable->AddRow(ModAbility, BuildModifierRow(EModifierCategory::AbilityType, EModifierTier::TierI, TEXT("Ability"), TEXT("Ability")));
	ModifierTable->AddRow(ModNeverGranted, BuildModifierRow(EModifierCategory::ItemType, EModifierTier::TierI, TEXT("Never Granted"), TEXT("NeverGranted")));
	Subsystem->ModifierCatalogTable = ModifierTable;

	// Deposit enough to afford all 4 root bubbles (1+2+3+4=10).
	Subsystem->DepositRunMastery(10);
	TestTrue(TEXT("Unlocking BubbleA should succeed"), Subsystem->TrySpendOnBubble(BubbleA));
	TestTrue(TEXT("Unlocking BubbleFull should succeed"), Subsystem->TrySpendOnBubble(BubbleFull));
	TestTrue(TEXT("Unlocking BubbleCategory should succeed"), Subsystem->TrySpendOnBubble(BubbleCategory));
	// BubbleNeverUnlocked is deliberately never spent on.

	TestTrue(TEXT("GrantModifier(SurvivalA) should succeed"), Subsystem->GrantModifier(ModSurvivalA));
	TestTrue(TEXT("GrantModifier(SurvivalB) should succeed"), Subsystem->GrantModifier(ModSurvivalB));
	TestTrue(TEXT("GrantModifier(Attack) should succeed"), Subsystem->GrantModifier(ModAttack));
	TestTrue(TEXT("GrantModifier(Ability) should succeed"), Subsystem->GrantModifier(ModAbility));

	// 1. Successful slot.
	TestTrue(TEXT("Slotting an owned modifier into an unlocked bubble should succeed"),
		Subsystem->TrySlotModifier(BubbleA, ModSurvivalA));
	TestTrue(TEXT("GetSlottedModifiers should contain the newly-slotted modifier"),
		Subsystem->GetSlottedModifiers(BubbleA).Contains(ModSurvivalA));

	// 2. Slot-when-not-unlocked rejection.
	TestFalse(TEXT("Slotting into a never-unlocked bubble should fail"),
		Subsystem->TrySlotModifier(BubbleNeverUnlocked, ModAttack));
	TestEqual(TEXT("A rejected not-unlocked slot should not mutate that bubble's slots"),
		Subsystem->GetSlottedModifiers(BubbleNeverUnlocked).Num(), 0);

	// 3. Slot-when-full rejection. Also pins down current cross-bubble-reuse behavior:
	// ModSurvivalA is already slotted into BubbleA from scenario 1, and TrySlotModifier
	// has no guard against slotting the same owned modifier into more than one bubble
	// at once (OwnedModifierIds is own-once, not consumed on slot) - this is documented
	// here as current behavior, not a confirmed design decision. Flag for reviewer if
	// this should instead be rejected.
	TestTrue(TEXT("Slotting the first modifier into BubbleFull should succeed (also documents cross-bubble reuse: ModSurvivalA is already slotted into BubbleA)"),
		Subsystem->TrySlotModifier(BubbleFull, ModSurvivalA));
	TestTrue(TEXT("Slotting a second, different-category modifier into BubbleFull should succeed"),
		Subsystem->TrySlotModifier(BubbleFull, ModAttack));
	TestFalse(TEXT("Slotting a third modifier once both slots are full should fail"),
		Subsystem->TrySlotModifier(BubbleFull, ModAbility));
	TestEqual(TEXT("BubbleFull should still have exactly 2 slotted modifiers after the rejected third"),
		Subsystem->GetSlottedModifiers(BubbleFull).Num(), 2);

	// 4. Category-duplicate rejection, and same-slot different-category success -
	// proves the rejection is category-based, not slot-index-based.
	TestTrue(TEXT("Slotting the first SurvivalType modifier into BubbleCategory should succeed"),
		Subsystem->TrySlotModifier(BubbleCategory, ModSurvivalA));
	TestFalse(TEXT("Slotting a second, different SurvivalType modifier into the same bubble should fail (category duplicate)"),
		Subsystem->TrySlotModifier(BubbleCategory, ModSurvivalB));
	TestEqual(TEXT("BubbleCategory should still have exactly 1 slotted modifier after the rejected duplicate-category attempt"),
		Subsystem->GetSlottedModifiers(BubbleCategory).Num(), 1);
	TestTrue(TEXT("Slotting a different-category modifier into that same still-open slot should succeed"),
		Subsystem->TrySlotModifier(BubbleCategory, ModAttack));
	TestEqual(TEXT("BubbleCategory should have exactly 2 slotted modifiers after the different-category success"),
		Subsystem->GetSlottedModifiers(BubbleCategory).Num(), 2);

	// 5. Unslot.
	TestTrue(TEXT("Unslotting a present modifier should succeed"), Subsystem->UnslotModifier(BubbleA, ModSurvivalA));
	TestEqual(TEXT("BubbleA should have no slotted modifiers after unslotting its only entry"),
		Subsystem->GetSlottedModifiers(BubbleA).Num(), 0);
	TestFalse(TEXT("Unslotting an already-absent modifier should no-op and return false"),
		Subsystem->UnslotModifier(BubbleA, ModSurvivalA));

	// 6. Defensive rejections (run before the respec below, while BubbleA is still
	// unlocked and has an open slot).
	TestFalse(TEXT("GrantModifier on an unknown ModifierId should fail"),
		Subsystem->GrantModifier(FName(TEXT("Mod_DoesNotExist"))));
	TestFalse(TEXT("GrantModifier on an already-owned ModifierId should fail (no duplicate entries)"),
		Subsystem->GrantModifier(ModSurvivalA));
	TestEqual(TEXT("GetOwnedModifiers should still have exactly 4 entries after the redundant/unknown grant attempts"),
		Subsystem->GetOwnedModifiers().Num(), 4);
	TestFalse(TEXT("TrySlotModifier with an unknown ModifierId should fail"),
		Subsystem->TrySlotModifier(BubbleA, FName(TEXT("Mod_DoesNotExist"))));
	TestFalse(TEXT("TrySlotModifier with a not-owned ModifierId should fail"),
		Subsystem->TrySlotModifier(BubbleA, ModNeverGranted));

	// 7. Respec clears slots, not inventory.
	Subsystem->RefundAllAndClearUnlocks();
	TestEqual(TEXT("RefundAllAndClearUnlocks should clear BubbleCategory's slotted modifiers"),
		Subsystem->GetSlottedModifiers(BubbleCategory).Num(), 0);
	TestEqual(TEXT("RefundAllAndClearUnlocks should clear BubbleFull's slotted modifiers"),
		Subsystem->GetSlottedModifiers(BubbleFull).Num(), 0);
	TestTrue(TEXT("RefundAllAndClearUnlocks should leave the owned-modifiers inventory untouched"),
		Subsystem->GetOwnedModifiers().Contains(ModSurvivalA));
	TestEqual(TEXT("RefundAllAndClearUnlocks should not change the owned-modifiers count"),
		Subsystem->GetOwnedModifiers().Num(), 4);

	UGameplayStatics::DeleteGameInSlot(ULevelClearTimeSubsystem::SaveSlotName, 0);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
