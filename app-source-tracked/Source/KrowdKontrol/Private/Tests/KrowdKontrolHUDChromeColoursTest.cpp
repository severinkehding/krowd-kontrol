// Pins HUDChromeColours (issue #93) GetBackground()/GetText() against independent
// literals, mirroring KrowdKontrolReservedGameplayColoursTest.cpp's pattern for
// ReservedGameplayColours. Without this, the widget tests that compare against
// HUDChromeColours::GetBackground()/GetText() directly are tautological with respect
// to drift in the shared constant itself - a typo here would silently pass every
// consumer test.

#include "Misc/AutomationTest.h"
#include "HUDChromeColours.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolHUDChromeColoursTest,
	"KrowdKontrol.Unit.HUDChromeColours",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolHUDChromeColoursTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("GetBackground() should be the desaturated near-black chrome colour"),
		HUDChromeColours::GetBackground(), FLinearColor(0.05f, 0.05f, 0.05f, 0.92f));
	TestEqual(TEXT("GetText() should be the light-gray (not pure white) chrome text colour"),
		HUDChromeColours::GetText(), FLinearColor(0.85f, 0.85f, 0.85f, 1.0f));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
