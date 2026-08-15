// Confirms ReservedGameplayColours (issue #70) is internally consistent - GetAll()
// has exactly 5 entries matching the 5 named accessors, all mutually distinct - and
// then audits UAbilityCooldownTrayWidget's known chrome colours against it, asserting
// none collide with a reserved value. The audit checks all 5 of the tray's slots, not
// just index 0 like KrowdKontrolAbilityCooldownTrayWidgetTest.cpp's own precedent
// check, since this test's entire purpose is the audit's completeness. Also covers
// SlotCooldownTexts[i] (not just SlotIconBorders[i]) since the widget's own
// BuildWidgetTree() comment claims text colour is reserved-colour-safe chrome too.
//
// Widgets currently audited here: UAbilityCooldownTrayWidget only. UEnergyMeterWidget
// is issue #64/PR #92's deliverable and isn't part of this branch yet - its audit is a
// follow-up once that PR merges. Add any future HUD widget's chrome to this list.
//
// Needs friend-class access to the widget's private SlotIconBorders/SlotCooldownTexts
// members (see FKrowdKontrolReservedGameplayColoursTest friend declaration in
// AbilityCooldownTrayWidget.h) rather than widening its public API.
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the other
// KrowdKontrol.Unit.* tests.

#include "Misc/AutomationTest.h"
#include "ReservedGameplayColours.h"
#include "AbilityCooldownTrayWidget.h"
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
	// Deliberately does not audit each slot's IconLabel UTextBlock (SlotIconBorders[i]'s
	// content): it shares SlotCooldownTexts[i]'s TextColor at construction today but
	// isn't independently tracked by this widget, so there is no member to assert
	// against without widening UAbilityCooldownTrayWidget's private surface - an
	// intentional scope boundary, not an oversight.
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
		}
	}

	// (3) Proves the audit's TestFalse(...Contains(...)) shape actually goes red on a
	// genuine collision, not just on the (never-colliding) real widget colours above -
	// guards against an inverted/typo'd assertion silently passing forever.
	UBorder* CollidingBorder = NewObject<UBorder>(GetTransientPackage());
	CollidingBorder->SetBrushColor(ReservedGameplayColours::GetPurple());
	TestTrue(TEXT("A border deliberately set to a reserved colour should be detected as colliding"),
		AllReserved.Contains(CollidingBorder->GetBrushColor()));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
