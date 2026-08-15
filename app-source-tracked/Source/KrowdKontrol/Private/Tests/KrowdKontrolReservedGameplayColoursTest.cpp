// Confirms ReservedGameplayColours (issue #70) is internally consistent - GetAll()
// has exactly 5 entries matching the 5 named accessors, all mutually distinct - and
// then audits UEnergyMeterWidget's and UAbilityCooldownTrayWidget's known chrome
// colours against it, asserting none collide with a reserved value. The audit checks
// all 5 of the tray's slots, not just index 0 like KrowdKontrolAbilityCooldownTrayWidgetTest.cpp's
// own precedent check, since this test's entire purpose is the audit's completeness.
//
// Needs friend-class access to both widgets' private BackgroundBorder/SlotIconBorders
// members (see FKrowdKontrolReservedGameplayColoursTest friend declarations in
// EnergyMeterWidget.h/AbilityCooldownTrayWidget.h) rather than widening either
// widget's public API.
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the other
// KrowdKontrol.Unit.* tests.

#include "Misc/AutomationTest.h"
#include "ReservedGameplayColours.h"
#include "EnergyMeterWidget.h"
#include "AbilityCooldownTrayWidget.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"
#include "Components/Border.h"

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

	// (2) Energy meter audit.
	UEnergyMeterWidget* EnergyMeterWidget = CreateWidget<UEnergyMeterWidget>(World, UEnergyMeterWidget::StaticClass());
	if (TestNotNull(TEXT("UEnergyMeterWidget should construct"), EnergyMeterWidget)
		&& TestNotNull(TEXT("EnergyMeterWidget->BackgroundBorder should be non-null"), ToRawPtr(EnergyMeterWidget->BackgroundBorder)))
	{
		TestFalse(TEXT("EnergyMeterWidget background border colour should not collide with a reserved gameplay colour"),
			AllReserved.Contains(EnergyMeterWidget->BackgroundBorder->GetBrushColor()));
	}

	// (3) Ability tray audit - all 5 slots, not just index 0.
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
		}
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
