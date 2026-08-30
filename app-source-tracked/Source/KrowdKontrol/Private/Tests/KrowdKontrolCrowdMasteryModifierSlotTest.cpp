// Confirms UCrowdMasteryTotalSubsystem's modifier grant/slot/unslot API (issue #376,
// docs/prd-mastery-skill-tree.md REQ-4): GrantModifier/TrySlotModifier/UnslotModifier
// mutate state only on success, TrySlotModifier's category rule matches a candidate
// modifier's Category against an open slot's pre-assigned accepted category
// (FMasterySkillBubble::SlotAcceptedCategories - not an anti-duplication check across
// already-slotted modifiers, see that function's doc comment), and
// RefundAllAndClearUnlocks() clears slotted modifiers while leaving the
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
	// Each bubble's 2 slots default to accepting SurvivalType/AttackType respectively -
	// callers that need a different accepted-category shape (the scenario-4 cases below)
	// overwrite SlotAcceptedCategories after the fact.
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
			Bubble.SlotAcceptedCategories = { EModifierCategory::SurvivalType, EModifierCategory::AttackType };
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

	// Fixture tree: a single root node (no prerequisite) with the 4 BuildFourBubbles()
	// bubbles (Bubble0/2/3 get unlocked below, Bubble1 is deliberately left
	// never-unlocked - scenario 2), plus one hand-built 5th bubble, BubbleMismatch,
	// whose slots accept different categories (Survival/Attack) with neither matching
	// AbilityType - proves TrySlotModifier rejects a category mismatch even while a
	// slot is still open, not just when both slots are full (scenario 4b).
	// BubbleCategory (Bubble3)'s default SlotAcceptedCategories is overridden below to
	// [AttackType, AttackType] - both slots accepting the *same* category - so
	// scenario 4 can prove the rule is per-slot acceptance, not anti-duplication.
	UDataTable* TreeTable = NewObject<UDataTable>();
	TreeTable->RowStruct = FMasteryTreeNode::StaticStruct();
	FMasteryTreeNode RootRow;
	RootRow.ParentNodeId = NAME_None;
	RootRow.Phase = EMasteryTreePhase::Phase1;
	RootRow.Bubbles = BuildFourBubbles(TEXT("Root"));
	RootRow.Bubbles[3].SlotAcceptedCategories = { EModifierCategory::AttackType, EModifierCategory::AttackType };
	FMasterySkillBubble MismatchBubble;
	MismatchBubble.BubbleId = FName(TEXT("Root_BubbleMismatch"));
	MismatchBubble.DisplayName = FText::FromString(TEXT("Root Skill Mismatch"));
	MismatchBubble.PointCost = 5;
	MismatchBubble.EffectHookId = FName(TEXT("Root_EffectMismatch"));
	MismatchBubble.SlotAcceptedCategories = { EModifierCategory::SurvivalType, EModifierCategory::AttackType };
	RootRow.Bubbles.Add(MismatchBubble);
	// BubbleSelfDup: both slots accept AttackType, dedicated (never touched by scenario 4)
	// so scenario 4c can prove TrySlotModifier has no guard against placing the SAME owned
	// modifier into two of its own bubble's slots.
	FMasterySkillBubble SelfDupBubble;
	SelfDupBubble.BubbleId = FName(TEXT("Root_BubbleSelfDup"));
	SelfDupBubble.DisplayName = FText::FromString(TEXT("Root Skill SelfDup"));
	SelfDupBubble.PointCost = 1;
	SelfDupBubble.EffectHookId = FName(TEXT("Root_EffectSelfDup"));
	SelfDupBubble.SlotAcceptedCategories = { EModifierCategory::AttackType, EModifierCategory::AttackType };
	RootRow.Bubbles.Add(SelfDupBubble);
	// BubbleUnauthored: SlotAcceptedCategories deliberately left at its empty default,
	// the real-world shape of a bubble unlocked before its DataTable row is fully
	// authored - scenario 4d proves TrySlotModifier fails closed against it.
	FMasterySkillBubble UnauthoredBubble;
	UnauthoredBubble.BubbleId = FName(TEXT("Root_BubbleUnauthored"));
	UnauthoredBubble.DisplayName = FText::FromString(TEXT("Root Skill Unauthored"));
	UnauthoredBubble.PointCost = 1;
	UnauthoredBubble.EffectHookId = FName(TEXT("Root_EffectUnauthored"));
	RootRow.Bubbles.Add(UnauthoredBubble);
	TreeTable->AddRow(FName(TEXT("Node_Root")), RootRow);
	Subsystem->MasteryTreeTable = TreeTable;

	const FName BubbleA(TEXT("Root_Bubble0"));
	const FName BubbleNeverUnlocked(TEXT("Root_Bubble1"));
	const FName BubbleFull(TEXT("Root_Bubble2"));
	const FName BubbleCategory(TEXT("Root_Bubble3"));
	const FName BubbleMismatch(TEXT("Root_BubbleMismatch"));
	const FName BubbleSelfDup(TEXT("Root_BubbleSelfDup"));
	const FName BubbleUnauthored(TEXT("Root_BubbleUnauthored"));

	// Fixture modifier catalog: 2 SurvivalType (SurvivalB doubles as the
	// BubbleMismatch slot-0-accepts-Survival case), 2 AttackType (for BubbleCategory's
	// both-slots-accept-AttackType case), 1 AbilityType (matches no slot on
	// BubbleCategory or BubbleMismatch), 1 ItemType. Mod_NeverGranted is deliberately
	// never passed to GrantModifier (scenario 6's not-owned rejection).
	UDataTable* ModifierTable = NewObject<UDataTable>();
	ModifierTable->RowStruct = FMasteryModifierRow::StaticStruct();
	const FName ModSurvivalA(TEXT("Mod_SurvivalA"));
	const FName ModSurvivalB(TEXT("Mod_SurvivalB"));
	const FName ModAttack(TEXT("Mod_Attack"));
	const FName ModAttackB(TEXT("Mod_AttackB"));
	const FName ModAbility(TEXT("Mod_Ability"));
	const FName ModNeverGranted(TEXT("Mod_NeverGranted"));
	ModifierTable->AddRow(ModSurvivalA, BuildModifierRow(EModifierCategory::SurvivalType, EModifierTier::TierI, TEXT("Survival A"), TEXT("SurvivalA")));
	ModifierTable->AddRow(ModSurvivalB, BuildModifierRow(EModifierCategory::SurvivalType, EModifierTier::TierI, TEXT("Survival B"), TEXT("SurvivalB")));
	ModifierTable->AddRow(ModAttack, BuildModifierRow(EModifierCategory::AttackType, EModifierTier::TierI, TEXT("Attack"), TEXT("Attack")));
	ModifierTable->AddRow(ModAttackB, BuildModifierRow(EModifierCategory::AttackType, EModifierTier::TierI, TEXT("Attack B"), TEXT("AttackB")));
	ModifierTable->AddRow(ModAbility, BuildModifierRow(EModifierCategory::AbilityType, EModifierTier::TierI, TEXT("Ability"), TEXT("Ability")));
	ModifierTable->AddRow(ModNeverGranted, BuildModifierRow(EModifierCategory::ItemType, EModifierTier::TierI, TEXT("Never Granted"), TEXT("NeverGranted")));
	Subsystem->ModifierCatalogTable = ModifierTable;

	// Deposit enough to afford all 4 root bubbles plus BubbleMismatch, BubbleSelfDup,
	// and BubbleUnauthored (1+2+3+4+5+1+1=17).
	Subsystem->DepositRunMastery(17);
	TestTrue(TEXT("Unlocking BubbleA should succeed"), Subsystem->TrySpendOnBubble(BubbleA));
	TestTrue(TEXT("Unlocking BubbleFull should succeed"), Subsystem->TrySpendOnBubble(BubbleFull));
	TestTrue(TEXT("Unlocking BubbleCategory should succeed"), Subsystem->TrySpendOnBubble(BubbleCategory));
	TestTrue(TEXT("Unlocking BubbleMismatch should succeed"), Subsystem->TrySpendOnBubble(BubbleMismatch));
	TestTrue(TEXT("Unlocking BubbleSelfDup should succeed"), Subsystem->TrySpendOnBubble(BubbleSelfDup));
	TestTrue(TEXT("Unlocking BubbleUnauthored should succeed"), Subsystem->TrySpendOnBubble(BubbleUnauthored));
	// BubbleNeverUnlocked is deliberately never spent on.

	TestTrue(TEXT("GrantModifier(SurvivalA) should succeed"), Subsystem->GrantModifier(ModSurvivalA));
	TestTrue(TEXT("GrantModifier(SurvivalB) should succeed"), Subsystem->GrantModifier(ModSurvivalB));
	TestTrue(TEXT("GrantModifier(Attack) should succeed"), Subsystem->GrantModifier(ModAttack));
	TestTrue(TEXT("GrantModifier(AttackB) should succeed"), Subsystem->GrantModifier(ModAttackB));
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

	// 4. Slot-accepts-category matching, not anti-duplication: BubbleCategory's slots
	// both accept AttackType, so two DIFFERENT AttackType modifiers both fit. An
	// anti-duplication rule (reject a candidate sharing an already-slotted
	// modifier's Category) would have rejected the second one here - this proves the
	// rule is per-slot acceptance instead.
	TestTrue(TEXT("Slotting the first AttackType modifier into BubbleCategory (both slots accept AttackType) should succeed"),
		Subsystem->TrySlotModifier(BubbleCategory, ModAttack));
	TestTrue(TEXT("Slotting a second, different AttackType modifier into BubbleCategory's other AttackType-accepting slot should succeed (proves this isn't anti-duplication)"),
		Subsystem->TrySlotModifier(BubbleCategory, ModAttackB));
	TestEqual(TEXT("BubbleCategory should have exactly 2 slotted modifiers once both AttackType-accepting slots fill"),
		Subsystem->GetSlottedModifiers(BubbleCategory).Num(), 2);

	// 4b. Slot-accepts-category rejection with an open slot still available - proves
	// the rule is a fixed per-slot accepted category, not "first open slot wins".
	// BubbleMismatch's slot 0 accepts SurvivalType and slot 1 accepts AttackType, so
	// an AbilityType modifier fits neither slot even though both are open.
	TestFalse(TEXT("Slotting an AbilityType modifier into BubbleMismatch should fail: neither slot accepts AbilityType"),
		Subsystem->TrySlotModifier(BubbleMismatch, ModAbility));
	TestEqual(TEXT("BubbleMismatch should have no slotted modifiers after the rejected mismatch attempt"),
		Subsystem->GetSlottedModifiers(BubbleMismatch).Num(), 0);
	TestTrue(TEXT("Slotting a SurvivalType modifier into BubbleMismatch's SurvivalType-accepting slot should succeed"),
		Subsystem->TrySlotModifier(BubbleMismatch, ModSurvivalB));
	TestFalse(TEXT("Slotting an AbilityType modifier into BubbleMismatch's one remaining (AttackType-accepting) slot should still fail"),
		Subsystem->TrySlotModifier(BubbleMismatch, ModAbility));
	TestTrue(TEXT("Slotting an AttackType modifier into BubbleMismatch's remaining AttackType-accepting slot should succeed"),
		Subsystem->TrySlotModifier(BubbleMismatch, ModAttack));
	TestEqual(TEXT("BubbleMismatch should have exactly 2 slotted modifiers once both differently-accepting slots fill"),
		Subsystem->GetSlottedModifiers(BubbleMismatch).Num(), 2);

	// 4c. Same-bubble self-duplication: BubbleSelfDup's slots both accept AttackType,
	// and TrySlotModifier has no guard against placing the SAME owned modifier into two
	// of its own bubble's slots (mirrors the cross-bubble reuse documented in scenario 3).
	TestTrue(TEXT("Slotting ModAttack into BubbleSelfDup's first AttackType-accepting slot should succeed"),
		Subsystem->TrySlotModifier(BubbleSelfDup, ModAttack));
	TestTrue(TEXT("Slotting the SAME ModAttack again into BubbleSelfDup's second AttackType-accepting slot should also succeed (documents same-bubble self-duplication, mirroring scenario 3's cross-bubble note)"),
		Subsystem->TrySlotModifier(BubbleSelfDup, ModAttack));
	TestEqual(TEXT("BubbleSelfDup should report 2 slotted-modifier entries even though both hold the same ModifierId"),
		Subsystem->GetSlottedModifiers(BubbleSelfDup).Num(), 2);

	// 4d. Un-authored slot fails closed: BubbleUnauthored's SlotAcceptedCategories is
	// left at its empty default, the real-world shape of a bubble unlocked before its
	// DataTable row is fully authored - every slot attempt should fail closed, same
	// posture as an unset MasteryTreeTable/ModifierCatalogTable.
	TestFalse(TEXT("Slotting into a bubble with an un-authored (empty) SlotAcceptedCategories should fail closed"),
		Subsystem->TrySlotModifier(BubbleUnauthored, ModAttack));
	TestEqual(TEXT("BubbleUnauthored should have no slotted modifiers after the fail-closed rejection"),
		Subsystem->GetSlottedModifiers(BubbleUnauthored).Num(), 0);

	// 5. Unslot.
	TestTrue(TEXT("Unslotting a present modifier should succeed"), Subsystem->UnslotModifier(BubbleA, ModSurvivalA));
	TestEqual(TEXT("BubbleA should have no slotted modifiers after unslotting its only entry"),
		Subsystem->GetSlottedModifiers(BubbleA).Num(), 0);
	TestFalse(TEXT("Unslotting an already-absent modifier should no-op and return false"),
		Subsystem->UnslotModifier(BubbleA, ModSurvivalA));
	TestFalse(TEXT("Unslotting from a bubble that was never slotted into at all should no-op and return false"),
		Subsystem->UnslotModifier(BubbleNeverUnlocked, ModAttack));
	// BubbleA's slots are now [NAME_None, NAME_None] after the unslot above - the exact
	// state where IndexOfByKey(NAME_None) would spuriously find the open-slot sentinel
	// without the ModifierId == NAME_None guard.
	TestFalse(TEXT("UnslotModifier(BubbleId, NAME_None) should fail rather than spuriously matching an empty slot sentinel"),
		Subsystem->UnslotModifier(BubbleA, NAME_None));

	// 6. Defensive rejections (run before the respec below, while BubbleA is still
	// unlocked and has an open slot).
	TestFalse(TEXT("GrantModifier on an unknown ModifierId should fail"),
		Subsystem->GrantModifier(FName(TEXT("Mod_DoesNotExist"))));
	TestFalse(TEXT("GrantModifier on an already-owned ModifierId should fail (no duplicate entries)"),
		Subsystem->GrantModifier(ModSurvivalA));
	TestEqual(TEXT("GetOwnedModifiers should still have exactly 5 entries after the redundant/unknown grant attempts"),
		Subsystem->GetOwnedModifiers().Num(), 5);
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
		Subsystem->GetOwnedModifiers().Num(), 5);

	UGameplayStatics::DeleteGameInSlot(ULevelClearTimeSubsystem::SaveSlotName, 0);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
