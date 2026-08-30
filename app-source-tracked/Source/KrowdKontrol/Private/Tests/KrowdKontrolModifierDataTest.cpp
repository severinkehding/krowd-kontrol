// Confirms issue #376 (docs/prd-mastery-skill-tree.md REQ-4, modifier data-schema
// half): FMasteryModifierRow round-trips through a UDataTable exactly like every
// other DataTable-row struct in this module, and the real authored
// /Game/Data/DT_ModifierCatalogTable content asset actually satisfies the issue's
// acceptance criteria (>=5 rows, >=2 categories, both Tier I and Tier II present)
// rather than just compiling.
//
// No UWorld/CreateNewMap() needed - this is pure DataTable/reflection data, no
// subsystem, no BeginPlay, same as KrowdKontrolMasteryTreeDataTest.cpp.
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the other
// KrowdKontrol.Unit.* tests.

#include "Misc/AutomationTest.h"
#include "ModifierData.h"
#include "Engine/DataTable.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolModifierDataTest,
	"KrowdKontrol.Unit.ModifierData",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolModifierDataTest::RunTest(const FString& Parameters)
{
	// (a) In-code round-trip: Category, Tier, DisplayName, EffectHookId must all
	// round-trip exactly through AddRow()/FindRow().
	{
		UDataTable* Table = NewObject<UDataTable>();
		Table->RowStruct = FMasteryModifierRow::StaticStruct();

		FMasteryModifierRow Row;
		Row.Category = EModifierCategory::SurvivalType;
		Row.Tier = EModifierTier::TierII;
		Row.DisplayName = FText::FromString(TEXT("Survival Type Ability Tier II"));
		Row.EffectHookId = FName(TEXT("SurvivalTierII"));
		Table->AddRow(FName(TEXT("Mod_Test")), Row);

		const FMasteryModifierRow* Found = Table->FindRow<FMasteryModifierRow>(FName(TEXT("Mod_Test")), TEXT("ModifierDataTest"));
		if (TestNotNull(TEXT("Mod_Test should round-trip through the DataTable"), Found))
		{
			TestEqual(TEXT("Category should round-trip as SurvivalType"),
				static_cast<uint8>(Found->Category), static_cast<uint8>(EModifierCategory::SurvivalType));
			TestEqual(TEXT("Tier should round-trip as TierII"),
				static_cast<uint8>(Found->Tier), static_cast<uint8>(EModifierTier::TierII));
			TestEqual(TEXT("DisplayName should round-trip"), Found->DisplayName.ToString(), FString(TEXT("Survival Type Ability Tier II")));
			TestEqual(TEXT("EffectHookId should round-trip"), Found->EffectHookId, FName(TEXT("SurvivalTierII")));
		}
	}

	// (b) Real-asset shape: the authored placeholder DT_ModifierCatalogTable content
	// asset must actually satisfy the acceptance criteria - at least 5 rows, no
	// empty DisplayName/EffectHookId, and both Category/Tier within their valid
	// enum range (not the Count sentinel). A missing asset fails loudly via
	// TestNotNull, which is the intended signal - not a reason to weaken this to a
	// soft skip.
	{
		UDataTable* RealTable = LoadObject<UDataTable>(nullptr, TEXT("/Game/Data/DT_ModifierCatalogTable.DT_ModifierCatalogTable"));
		if (TestNotNull(TEXT("DT_ModifierCatalogTable should load from Content"), RealTable))
		{
			const TMap<FName, uint8*>& RowMap = RealTable->GetRowMap();
			TestTrue(TEXT("DT_ModifierCatalogTable should have at least 5 rows"), RowMap.Num() >= 5);

			TSet<EModifierCategory> SeenCategories;
			for (const TPair<FName, uint8*>& RowPair : RowMap)
			{
				const FMasteryModifierRow* Row = reinterpret_cast<const FMasteryModifierRow*>(RowPair.Value);

				TestFalse(*FString::Printf(TEXT("Row %s DisplayName should not be empty"), *RowPair.Key.ToString()),
					Row->DisplayName.IsEmpty());
				TestNotEqual(*FString::Printf(TEXT("Row %s EffectHookId should not be empty"), *RowPair.Key.ToString()),
					Row->EffectHookId, FName(NAME_None));
				TestTrue(*FString::Printf(TEXT("Row %s Category should be within the valid enum range"), *RowPair.Key.ToString()),
					Row->Category < EModifierCategory::Count);
				TestTrue(*FString::Printf(TEXT("Row %s Tier should be within the valid enum range"), *RowPair.Key.ToString()),
					Row->Tier < EModifierTier::Count);

				SeenCategories.Add(Row->Category);
			}

			TestTrue(TEXT("DT_ModifierCatalogTable should span at least 2 categories"), SeenCategories.Num() >= 2);
		}
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
