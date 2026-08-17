// Confirms EliteEligibility::IsEligibleAtLevel() (issue #19, PRD 03 REQ-4) is the
// sole, correctly-bounded gate for "elite variants introduced starting level 4-5":
// levels 1-3 are ineligible, level MinEligibleLevel (4) and above are eligible, with
// the boundary at exactly MinEligibleLevel asserted directly (off-by-one is the
// obvious risk here).
//
// No UWorld/CreateNewMap() needed - IsEligibleAtLevel() is a pure function with no
// engine-object dependency, same rationale KrowdKontrolGameplayColoursAreLockedTest.cpp
// documents for ReservedGameplayColours.
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the other
// KrowdKontrol.Unit.* tests.

#include "Misc/AutomationTest.h"
#include "EliteEligibility.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolEliteEligibilityTest,
	"KrowdKontrol.Unit.EliteEligibility",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolEliteEligibilityTest::RunTest(const FString& Parameters)
{
	// (a) MinEligibleLevel is the documented threshold (4).
	TestEqual(TEXT("MinEligibleLevel should be 4"), EliteEligibility::MinEligibleLevel, 4);

	// (b) Levels 1-3 are ineligible.
	TestFalse(TEXT("Level 1 should be ineligible"), EliteEligibility::IsEligibleAtLevel(1));
	TestFalse(TEXT("Level 2 should be ineligible"), EliteEligibility::IsEligibleAtLevel(2));
	TestFalse(TEXT("Level 3 should be ineligible"), EliteEligibility::IsEligibleAtLevel(3));

	// (c) Level MinEligibleLevel (4) and above are eligible.
	TestTrue(TEXT("Level 4 should be eligible"), EliteEligibility::IsEligibleAtLevel(4));
	TestTrue(TEXT("Level 5 should be eligible"), EliteEligibility::IsEligibleAtLevel(5));
	TestTrue(TEXT("Level 100 should be eligible"), EliteEligibility::IsEligibleAtLevel(100));

	// (d) Boundary case, specifically at MinEligibleLevel - the direct off-by-one check.
	TestTrue(TEXT("Level exactly at MinEligibleLevel should be eligible"),
		EliteEligibility::IsEligibleAtLevel(EliteEligibility::MinEligibleLevel));
	TestFalse(TEXT("Level one below MinEligibleLevel should be ineligible"),
		EliteEligibility::IsEligibleAtLevel(EliteEligibility::MinEligibleLevel - 1));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
