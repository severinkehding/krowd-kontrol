// Confirms ReservedGameplayColours (issue #70) is internally consistent - GetAll()
// has exactly 5 entries matching the 5 named accessors, all mutually distinct - and
// then audits each tracked widget's known chrome colours against it, asserting none
// collide with a reserved value. The tray audit checks all 5 slots, not just index 0
// like KrowdKontrolAbilityCooldownTrayWidgetTest.cpp's own precedent check, since this
// test's entire purpose is the audit's completeness. Also covers SlotCooldownTexts[i]
// (not just SlotIconBorders[i]) since the widget's own BuildWidgetTree() comment
// claims text colour is reserved-colour-safe chrome too.
//
// Widgets currently audited here: UAbilityCooldownTrayWidget, UPostRunSummaryWidget
// (issue #74), and UOnScreenPromptWidget (issue #34). UEnergyMeterWidget is issue
// #64/PR #92's deliverable and still isn't audited here - a pre-existing gap, not
// introduced by issue #74, tracked as a separate follow-up. Add any future HUD
// widget's chrome to this list.
//
// Needs friend-class access to each widget's private chrome members (e.g.
// SlotIconBorders/SlotCooldownTexts, RootBorder) - see each widget's own
// FKrowdKontrolReservedGameplayColoursTest friend declaration - rather than widening
// their public API.
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the other
// KrowdKontrol.Unit.* tests.

#include "Misc/AutomationTest.h"
#include "ReservedGameplayColours.h"
#include "AbilityCooldownTrayWidget.h"
#include "PostRunSummaryWidget.h"
#include "OnScreenPromptWidget.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolReservedGameplayColoursTest,
	"KrowdKontrol.Unit.ReservedGameplayColours",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolReservedGameplayColoursTest::RunTest(const FString& Parameters)
{
	// (1) Constant-list sanity - exactly 5 entries, each named accessor appears in
	// GetAll(), and all 5 are mutually distinct (a duplicate would silently shrink
	// real coverage).
	const TArray<FLinearColor> AllReserved = ReservedGameplayColours::GetAll();
	TestEqual(TEXT("GetAll() should have exactly 5 entries"), AllReserved.Num(), 5);

	TestTrue(TEXT("GetAll() should contain GetPurple()"), AllReserved.Contains(ReservedGameplayColours::GetPurple()));
	TestTrue(TEXT("GetAll() should contain GetTeal()"), AllReserved.Contains(ReservedGameplayColours::GetTeal()));
	TestTrue(TEXT("GetAll() should contain GetOrange()"), AllReserved.Contains(ReservedGameplayColours::GetOrange()));
	TestTrue(TEXT("GetAll() should contain GetBlue()"), AllReserved.Contains(ReservedGameplayColours::GetBlue()));
	TestTrue(TEXT("GetAll() should contain GetWhite()"), AllReserved.Contains(ReservedGameplayColours::GetWhite()));

	for (int32 i = 0; i < AllReserved.Num(); ++i)
	{
		for (int32 j = i + 1; j < AllReserved.Num(); ++j)
		{
			TestNotEqual(*FString::Printf(TEXT("Reserved colours %d and %d should be mutually distinct"), i, j),
				AllReserved[i], AllReserved[j]);
		}
	}

	// (1a) Each GetXTag() accessor's FName literal, pinned against its expected plain-
	// English string - every other place these accessors are exercised (AbilityData,
	// EnemyBase, RoomActor tests) only compares production output against a call to the
	// same accessor, which is circular for verifying the literal itself. This is the
	// one independent ground-truth check.
	TestEqual(TEXT("GetPurpleTag() should be 'Purple'"), ReservedGameplayColours::GetPurpleTag(), FName(TEXT("Purple")));
	TestEqual(TEXT("GetTealTag() should be 'Teal'"), ReservedGameplayColours::GetTealTag(), FName(TEXT("Teal")));
	TestEqual(TEXT("GetOrangeTag() should be 'Orange'"), ReservedGameplayColours::GetOrangeTag(), FName(TEXT("Orange")));
	TestEqual(TEXT("GetBlueTag() should be 'Blue'"), ReservedGameplayColours::GetBlueTag(), FName(TEXT("Blue")));
	TestEqual(TEXT("GetWhiteTag() should be 'White'"), ReservedGameplayColours::GetWhiteTag(), FName(TEXT("White")));

	// (1b) White (pure 1,1,1,1) is distinguishable from both widgets' actual text
	// colour (0.85,0.85,0.85,1.0) - proves the audit below isn't vacuously passing by
	// coincidence.
	TestNotEqual(TEXT("Reserved White should be distinct from the widgets' light-gray text colour"),
		ReservedGameplayColours::GetWhite(), FLinearColor(0.85f, 0.85f, 0.85f, 1.0f));

	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
	{
		return false;
	}

	// (2) Ability tray audit - all 5 slots, not just index 0. Covers both the slot
	// icon borders and the slot cooldown text, since BuildWidgetTree()'s own comment
	// claims text colour is reserved-colour-safe chrome too, not just the borders.
	// Deliberately does not audit SlotIconLabels[i] (the per-slot ability-abbreviation
	// UTextBlock) here: unlike SlotIconBorders/SlotCooldownTexts, which are chrome and
	// must avoid all 5 reserved colours per PRD 13 REQ-4, SlotIconLabels is this
	// widget's informational colour channel (issue #76 / PRD 13 REQ-7) - it is SUPPOSED
	// to render one of the 5 reserved colours, matching AbilityData::Get(slot).Colour.
	// Asserting "must not collide" against it here would assert the opposite of its
	// intended, tested (KrowdKontrolAbilityCooldownTrayWidgetTest.cpp) behaviour.
	UAbilityCooldownTrayWidget* TrayWidget =
		CreateWidget<UAbilityCooldownTrayWidget>(World, UAbilityCooldownTrayWidget::StaticClass());
	if (TestNotNull(TEXT("UAbilityCooldownTrayWidget should construct"), TrayWidget))
	{
		for (int32 Index = 0; Index < UAbilityCooldownTrayWidget::NumAbilitySlots; ++Index)
		{
			if (TestNotNull(*FString::Printf(TEXT("TrayWidget->SlotIconBorders[%d] should be non-null"), Index),
				ToRawPtr(TrayWidget->SlotIconBorders[Index])))
			{
				TestFalse(*FString::Printf(TEXT("TrayWidget slot %d border colour should not collide with a reserved gameplay colour"), Index),
					AllReserved.Contains(TrayWidget->SlotIconBorders[Index]->GetBrushColor()));
			}

			if (TestNotNull(*FString::Printf(TEXT("TrayWidget->SlotCooldownTexts[%d] should be non-null"), Index),
				ToRawPtr(TrayWidget->SlotCooldownTexts[Index])))
			{
				TestFalse(*FString::Printf(TEXT("TrayWidget slot %d cooldown text colour should not collide with a reserved gameplay colour"), Index),
					AllReserved.Contains(TrayWidget->SlotCooldownTexts[Index]->GetColorAndOpacity().GetSpecifiedColor()));
			}

			// (2b) Locked-state border colour (issue #68 / PRD 13 REQ-3) - audited the
			// same way as the normal chrome colour above, then restored to unlocked so
			// iteration order doesn't leave stray locked slots affecting later checks.
			TrayWidget->SetSlotLocked(static_cast<EAbilitySlot>(Index), true);
			if (TestNotNull(*FString::Printf(TEXT("TrayWidget->SlotIconBorders[%d] should be non-null while locked"), Index),
				ToRawPtr(TrayWidget->SlotIconBorders[Index])))
			{
				TestFalse(*FString::Printf(TEXT("TrayWidget slot %d locked border colour should not collide with a reserved gameplay colour"), Index),
					AllReserved.Contains(TrayWidget->SlotIconBorders[Index]->GetBrushColor()));
			}
			TrayWidget->SetSlotLocked(static_cast<EAbilitySlot>(Index), false);
		}
	}

	// (3) Post-run summary screen audit (issue #74) - background border and both text
	// fields, mirroring the tray widget's audit above.
	UPostRunSummaryWidget* SummaryWidget =
		CreateWidget<UPostRunSummaryWidget>(World, UPostRunSummaryWidget::StaticClass());
	if (TestNotNull(TEXT("UPostRunSummaryWidget should construct"), SummaryWidget))
	{
		TestFalse(TEXT("Summary root border colour should not collide with a reserved gameplay colour"),
			AllReserved.Contains(SummaryWidget->RootBorder->GetBrushColor()));
		TestFalse(TEXT("Clear time text colour should not collide with a reserved gameplay colour"),
			AllReserved.Contains(SummaryWidget->ClearTimeText->GetColorAndOpacity().GetSpecifiedColor()));
		TestFalse(TEXT("Crowd Mastery text colour should not collide with a reserved gameplay colour"),
			AllReserved.Contains(SummaryWidget->CrowdMasteryText->GetColorAndOpacity().GetSpecifiedColor()));
	}

	// (4) On-screen prompt widget audit (issue #34) - border and text colours,
	// mirroring the tray/summary widgets' audits above.
	UOnScreenPromptWidget* PromptWidget =
		CreateWidget<UOnScreenPromptWidget>(World, UOnScreenPromptWidget::StaticClass());
	if (TestNotNull(TEXT("UOnScreenPromptWidget should construct"), PromptWidget))
	{
		TestFalse(TEXT("Prompt border colour should not collide with a reserved gameplay colour"),
			AllReserved.Contains(PromptWidget->PromptBorder->GetBrushColor()));
		TestFalse(TEXT("Prompt text colour should not collide with a reserved gameplay colour"),
			AllReserved.Contains(PromptWidget->PromptText->GetColorAndOpacity().GetSpecifiedColor()));
	}

	// (5) Proves the audit's TestFalse(...Contains(...)) shape actually goes red on a
	// genuine collision, not just on the (never-colliding) real widget colours above -
	// guards against an inverted/typo'd assertion silently passing forever.
	UBorder* CollidingBorder = NewObject<UBorder>(GetTransientPackage());
	CollidingBorder->SetBrushColor(ReservedGameplayColours::GetPurple());
	TestTrue(TEXT("A border deliberately set to a reserved colour should be detected as colliding"),
		AllReserved.Contains(CollidingBorder->GetBrushColor()));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
