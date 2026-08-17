// Confirms the ReservedGameplayColours constant SET itself (issue #11, PRD 11
// REQ-1/REQ-2) is locked: exactly 5 info-colour constants exist, all 5 plus
// Background are mutually distinct, Background is near-black and fully opaque, and
// Background stays distinct from the pre-existing HUDChromeColours::GetBackground().
// This complements KrowdKontrolReservedGameplayColoursTest.cpp, which audits real
// widget consumers against the 5 reserved colours - this test locks the constant
// set's own shape instead.
//
// No UWorld/CreateNewMap() needed - GetAll()/GetBackground() are pure functions with
// no engine-object dependency.
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the other
// KrowdKontrol.Unit.* tests.

#include "Misc/AutomationTest.h"
#include "HUDChromeColours.h"
#include "ReservedGameplayColours.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolGameplayColoursAreLockedTest,
	"KrowdKontrol.Unit.GameplayColoursAreLocked",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolGameplayColoursAreLockedTest::RunTest(const FString& Parameters)
{
	// (1) Exactly 5 info-colour constants exist.
	const TArray<FLinearColor> AllReserved = ReservedGameplayColours::GetAll();
	TestEqual(TEXT("GetAll() should have exactly 5 entries"), AllReserved.Num(), 5);

	// (1b) Background is deliberately excluded from GetAll() - it's the backdrop the
	// 5 reserved colours are locked against, not a 6th reserved colour.
	TestFalse(TEXT("GetAll() should not contain Background"),
		AllReserved.Contains(ReservedGameplayColours::GetBackground()));

	// (2) All 5 info colours plus Background are mutually distinct.
	TArray<FLinearColor> AllWithBackground = AllReserved;
	AllWithBackground.Add(ReservedGameplayColours::GetBackground());

	for (int32 i = 0; i < AllWithBackground.Num(); ++i)
	{
		for (int32 j = i + 1; j < AllWithBackground.Num(); ++j)
		{
			TestNotEqual(*FString::Printf(TEXT("Reserved colours %d and %d should be mutually distinct"), i, j),
				AllWithBackground[i], AllWithBackground[j]);
		}
	}

	// (3) Background is near-black - each of R, G, B independently below a tight
	// threshold, not a single averaged/luminance value - and fully opaque, since this
	// is an environment backdrop, not a translucent HUD panel (contrast
	// HUDChromeColours::GetBackground()'s 0.92 alpha).
	const FLinearColor Background = ReservedGameplayColours::GetBackground();
	TestTrue(TEXT("Background R should be near-black"), Background.R <= 0.1f);
	TestTrue(TEXT("Background G should be near-black"), Background.G <= 0.1f);
	TestTrue(TEXT("Background B should be near-black"), Background.B <= 0.1f);
	TestEqual(TEXT("Background should be fully opaque"), Background.A, 1.0f);

	// (4) ReservedGameplayColours::GetBackground() (environment backdrop) must stay
	// distinct from HUDChromeColours::GetBackground() (HUD panel chrome, issue #93) -
	// both are near-black by design, but a future "de-duplication" that aliases one to
	// the other would be a real regression: an opaque world backdrop and a translucent
	// HUD panel are not interchangeable.
	TestNotEqual(TEXT("ReservedGameplayColours::GetBackground() should stay distinct from HUDChromeColours::GetBackground()"),
		Background, HUDChromeColours::GetBackground());

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
