// Issue #315 / docs/prd-colour-coded-herding.md REQ-1: proves
// AbilityData::GetChainColourForEnemyType derives the correct chain colour for
// each of the 4 locked enemy types from the existing PR #303 colour-match data
// (CounteredEnemyType/Colour on each non-neutral FAbilityData), that White is
// never assigned (Stun has no enemy counter, MISSION.md Hard Invariant 4), and
// that all 4 resolved colours are mutually distinct.

#include "Misc/AutomationTest.h"
#include "AbilityData.h"
#include "ReservedGameplayColours.h"
#include "EnemyType.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolChainColourTest,
	"KrowdKontrol.Unit.ChainColour",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolChainColourTest::RunTest(const FString& Parameters)
{
	const TArray<FLinearColor> AllReserved = ReservedGameplayColours::GetAll();

	// Ground truth: PR #303's colour-match matchups, independently restated here
	// (not derived from the function under test) - mirrors
	// KrowdKontrolAbilityColourMatchTest.cpp cases (a)/(b)/(f)/(g).
	const FLinearColor RunnerExpected = AbilityData::Get(EAbilitySlot::Snare).Colour; // RU-NNR <- Snare
	const FLinearColor TrooperExpected = AbilityData::Get(EAbilitySlot::Root).Colour;  // TR-UPR <- Root
	const FLinearColor BomberExpected = AbilityData::Get(EAbilitySlot::Fear).Colour;   // B0-0MR <- Fear
	const FLinearColor SniperExpected = AbilityData::Get(EAbilitySlot::Sleep).Colour;  // SN-1PR <- Sleep

	const FLinearColor RunnerActual = AbilityData::GetChainColourForEnemyType(EEnemyType::RU_NNR);
	const FLinearColor TrooperActual = AbilityData::GetChainColourForEnemyType(EEnemyType::TR_UPR);
	const FLinearColor BomberActual = AbilityData::GetChainColourForEnemyType(EEnemyType::B0_0MR);
	const FLinearColor SniperActual = AbilityData::GetChainColourForEnemyType(EEnemyType::SN_1PR);

	TestEqual(TEXT("RU-NNR's chain colour should equal Snare's colour-match colour"), RunnerActual, RunnerExpected);
	TestEqual(TEXT("TR-UPR's chain colour should equal Root's colour-match colour"), TrooperActual, TrooperExpected);
	TestEqual(TEXT("B0-0MR's chain colour should equal Fear's colour-match colour"), BomberActual, BomberExpected);
	TestEqual(TEXT("SN-1PR's chain colour should equal Sleep's colour-match colour"), SniperActual, SniperExpected);

	for (const FLinearColor& Actual : { RunnerActual, TrooperActual, BomberActual, SniperActual })
	{
		TestTrue(TEXT("Every enemy type's chain colour should be one of the 5 reserved gameplay colours"),
			AllReserved.Contains(Actual));
		TestNotEqual(TEXT("White should never be assigned as a chain colour (Stun has no enemy counter)"),
			Actual, ReservedGameplayColours::GetWhite());
	}

	const TArray<FLinearColor> AllChainColours = { RunnerActual, TrooperActual, BomberActual, SniperActual };
	for (int32 i = 0; i < AllChainColours.Num(); ++i)
	{
		for (int32 j = i + 1; j < AllChainColours.Num(); ++j)
		{
			TestNotEqual(*FString::Printf(TEXT("Chain colours %d and %d should be mutually distinct"), i, j),
				AllChainColours[i], AllChainColours[j]);
		}
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
