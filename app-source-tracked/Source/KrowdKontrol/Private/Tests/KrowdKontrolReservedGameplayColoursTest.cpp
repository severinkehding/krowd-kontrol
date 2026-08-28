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
// (issue #74), UOnScreenPromptWidget (issue #34), UBriefingCardWidget (issue #246),
// and UMainMenuWidget (issue #324). UEnergyMeterWidget is issue #64/PR #92's
// deliverable and still isn't audited here - a pre-existing gap, not introduced by
// issue #74, tracked as a separate follow-up. Add any future HUD widget's chrome to
// this list.
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
#include "AbilityCooldownComponent.h"
#include "PostRunSummaryWidget.h"
#include "OnScreenPromptWidget.h"
#include "BriefingCardWidget.h"
#include "MainMenuWidget.h"
#include "MainMenuLevelButtonWidget.h"
#include "LevelSequenceSubsystem.h"
#include "LevelSequenceData.h"
#include "AbilityTooltipWidget.h"
#include "AbilityData.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"

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

		// (2c) Cooldown-fill and ready-flash colours (issue #259) - the two colour-
		// producing paths this widget's real UAbilityCooldownComponent binding drives,
		// audited the same way as the normal/locked chrome above rather than trusting
		// the changelog's prose claim that the flash never reaches reserved White.
		UAbilityCooldownComponent* AuditCooldownComponent = NewObject<UAbilityCooldownComponent>();
		if (TestNotNull(TEXT("UAbilityCooldownComponent should construct for the fill/flash audit"), AuditCooldownComponent))
		{
			TrayWidget->BindAbilityCooldownComponent(AuditCooldownComponent);
			for (int32 Index = 0; Index < UAbilityCooldownTrayWidget::NumAbilitySlots; ++Index)
			{
				const EAbilitySlot Slot = static_cast<EAbilitySlot>(Index);
				AuditCooldownComponent->TryStartCooldown(Slot);
				if (TestNotNull(*FString::Printf(TEXT("TrayWidget->SlotCooldownFillBars[%d] should be non-null"), Index),
					ToRawPtr(TrayWidget->SlotCooldownFillBars[Index])))
				{
					TestFalse(*FString::Printf(TEXT("TrayWidget slot %d fill colour should not collide with a reserved gameplay colour"), Index),
						AllReserved.Contains(TrayWidget->SlotCooldownFillBars[Index]->GetFillColorAndOpacity()));
				}

				AuditCooldownComponent->AdvanceCooldowns(UAbilityCooldownComponent::DefaultAbilityCooldownSeconds);
				if (TestNotNull(*FString::Printf(TEXT("TrayWidget->SlotIconBorders[%d] should be non-null during the ready-flash"), Index),
					ToRawPtr(TrayWidget->SlotIconBorders[Index])))
				{
					TestFalse(*FString::Printf(TEXT("TrayWidget slot %d ready-flash border colour should not collide with a reserved gameplay colour"), Index),
						AllReserved.Contains(TrayWidget->SlotIconBorders[Index]->GetBrushColor()));
				}
			}
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
		TestFalse(TEXT("Next-level button label colour should not collide with a reserved gameplay colour"),
			AllReserved.Contains(SummaryWidget->NextLevelButtonLabel->GetColorAndOpacity().GetSpecifiedColor()));
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

	// (5) Briefing card widget audit (issue #246) - background border and all three
	// text fields, mirroring the other widgets' audits above.
	UBriefingCardWidget* BriefingWidget =
		CreateWidget<UBriefingCardWidget>(World, UBriefingCardWidget::StaticClass());
	if (TestNotNull(TEXT("UBriefingCardWidget should construct"), BriefingWidget))
	{
		TestFalse(TEXT("Briefing root border colour should not collide with a reserved gameplay colour"),
			AllReserved.Contains(BriefingWidget->RootBorder->GetBrushColor()));
		TestFalse(TEXT("Level name text colour should not collide with a reserved gameplay colour"),
			AllReserved.Contains(BriefingWidget->LevelNameText->GetColorAndOpacity().GetSpecifiedColor()));
		TestFalse(TEXT("Objective text colour should not collide with a reserved gameplay colour"),
			AllReserved.Contains(BriefingWidget->ObjectiveText->GetColorAndOpacity().GetSpecifiedColor()));
		TestFalse(TEXT("New-ability text colour should not collide with a reserved gameplay colour"),
			AllReserved.Contains(BriefingWidget->NewAbilityText->GetColorAndOpacity().GetSpecifiedColor()));
	}

	// (6) Ability tooltip widget audit (issue #260) - root border + all 6 text rows
	// must not collide with a reserved gameplay colour; the swatch border is
	// deliberately excluded (see comment below) since it is SUPPOSED to render one
	// of the 5 reserved colours (or White, for Stun) - same exclusion rationale as
	// AbilityCooldownTrayWidget's SlotIconLabels above.
	UAbilityTooltipWidget* TooltipWidget =
		CreateWidget<UAbilityTooltipWidget>(World, UAbilityTooltipWidget::StaticClass());
	if (TestNotNull(TEXT("UAbilityTooltipWidget should construct"), TooltipWidget))
	{
		TooltipWidget->SetAbility(EAbilitySlot::Sleep);
		TestFalse(TEXT("Tooltip root border colour should not collide with a reserved gameplay colour"),
			AllReserved.Contains(TooltipWidget->RootBorder->GetBrushColor()));
		TestFalse(TEXT("Tooltip name text colour should not collide with a reserved gameplay colour"),
			AllReserved.Contains(TooltipWidget->NameText->GetColorAndOpacity().GetSpecifiedColor()));
		TestFalse(TEXT("Tooltip key bind text colour should not collide with a reserved gameplay colour"),
			AllReserved.Contains(TooltipWidget->KeyBindText->GetColorAndOpacity().GetSpecifiedColor()));
		TestFalse(TEXT("Tooltip description text colour should not collide with a reserved gameplay colour"),
			AllReserved.Contains(TooltipWidget->DescriptionText->GetColorAndOpacity().GetSpecifiedColor()));
		TestFalse(TEXT("Tooltip duration text colour should not collide with a reserved gameplay colour"),
			AllReserved.Contains(TooltipWidget->DurationText->GetColorAndOpacity().GetSpecifiedColor()));
		TestFalse(TEXT("Tooltip range/shape text colour should not collide with a reserved gameplay colour"),
			AllReserved.Contains(TooltipWidget->RangeShapeText->GetColorAndOpacity().GetSpecifiedColor()));
		TestFalse(TEXT("Tooltip enemy type text colour should not collide with a reserved gameplay colour"),
			AllReserved.Contains(TooltipWidget->EnemyTypeText->GetColorAndOpacity().GetSpecifiedColor()));

		// (6a) The swatch DOES collide by design - proves the exclusion above isn't
		// hiding a real bug, mirroring check (2b)'s locked-border-colour precedent of
		// asserting the *opposite* case explicitly rather than only omitting a check.
		TestTrue(TEXT("Tooltip swatch colour SHOULD match AbilityData's reserved colour for the bound ability"),
			TooltipWidget->SwatchBorder->GetBrushColor() == AbilityData::Get(EAbilitySlot::Sleep).Colour);
	}

	// (7) Proves the audit's TestFalse(...Contains(...)) shape actually goes red on a
	// genuine collision, not just on the (never-colliding) real widget colours above -
	// guards against an inverted/typo'd assertion silently passing forever.
	UBorder* CollidingBorder = NewObject<UBorder>(GetTransientPackage());
	CollidingBorder->SetBrushColor(ReservedGameplayColours::GetPurple());
	TestTrue(TEXT("A border deliberately set to a reserved colour should be detected as colliding"),
		AllReserved.Contains(CollidingBorder->GetBrushColor()));

	// (8) Main menu widget audit (issue #324) - root border and title/Quit-label text
	// colours, mirroring the other widgets' audits above. MasteryDisplayAnchor has no
	// colour of its own (an empty USizeBox, no border/brush) - nothing to audit there.
	// Extended for issue #325: inject a single-row LevelSequenceTable before
	// construction so LevelSelectButtons is non-empty, then audit the first button's
	// label colour the same way QuitButtonLabel already is above.
	ULevelSequenceSubsystem* SequenceSubsystem = World->GetSubsystem<ULevelSequenceSubsystem>();
	if (TestNotNull(TEXT("UWorld should auto-instantiate ULevelSequenceSubsystem"), SequenceSubsystem))
	{
		UDataTable* Table = NewObject<UDataTable>();
		Table->RowStruct = FLevelSequenceRow::StaticStruct();
		FLevelSequenceRow Row;
		Row.NextLevelMapName = NAME_None;
		Table->AddRow(FName(TEXT("L_Level01")), Row);
		SequenceSubsystem->LevelSequenceTable = Table;
	}

	UMainMenuWidget* MenuWidget =
		CreateWidget<UMainMenuWidget>(World, UMainMenuWidget::StaticClass());
	if (TestNotNull(TEXT("UMainMenuWidget should construct"), MenuWidget))
	{
		TestFalse(TEXT("Main menu root border colour should not collide with a reserved gameplay colour"),
			AllReserved.Contains(MenuWidget->RootBorder->GetBrushColor()));
		TestFalse(TEXT("Main menu title text colour should not collide with a reserved gameplay colour"),
			AllReserved.Contains(MenuWidget->TitleText->GetColorAndOpacity().GetSpecifiedColor()));
		TestFalse(TEXT("Main menu Quit button label colour should not collide with a reserved gameplay colour"),
			AllReserved.Contains(MenuWidget->QuitButtonLabel->GetColorAndOpacity().GetSpecifiedColor()));

		if (TestEqual(TEXT("Main menu should build exactly one level-select button from the injected table"),
			MenuWidget->LevelSelectButtons.Num(), 1))
		{
			TestFalse(TEXT("Main menu level-select button label colour should not collide with a reserved gameplay colour"),
				AllReserved.Contains(MenuWidget->LevelSelectButtons[0]->LevelButtonLabel->GetColorAndOpacity().GetSpecifiedColor()));
		}
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
