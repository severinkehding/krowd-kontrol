// Confirms AbilityData (issue #63, PRD 02 REQ-3) exposes exactly the 5 locked
// per-ability base stats (duration, range, target type, colour) from the GDD table,
// that only Stun is colour-neutral (MISSION.md Hard Invariant 4 - no countered-enemy
// value), and that the 5 colours are mutually distinct.
//
// No UWorld/CreateNewMap() needed - AbilityData::Get/GetAll are pure functions with
// no engine-object dependency, unlike KrowdKontrolReservedGameplayColoursTest.cpp's
// widget audit.
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the other
// KrowdKontrol.Unit.* tests.

#include "Misc/AutomationTest.h"
#include "AbilityData.h"
#include "ReservedGameplayColours.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolAbilityDataTest,
	"KrowdKontrol.Unit.AbilityData",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolAbilityDataTest::RunTest(const FString& Parameters)
{
	// (1) GetAll() has exactly 5 entries - never 4 or 6 (MISSION.md Hard Invariant 4).
	const TArray<FAbilityData> AllAbilities = AbilityData::GetAll();
	TestEqual(TEXT("GetAll() should have exactly 5 entries"), AllAbilities.Num(), 5);

	// (2) Each real slot's Get(Slot).Ability matches the slot requested, and appears
	// exactly once in GetAll() - catches a copy-paste row-order mistake in the .cpp table.
	// Also asserts GetAll() returns entries in EAbilitySlot declaration order, backing the
	// ordering guarantee documented on AbilityData::GetAll()'s doc comment.
	const TArray<EAbilitySlot> AllSlots = { EAbilitySlot::Stun, EAbilitySlot::Sleep,
		EAbilitySlot::Root, EAbilitySlot::Fear, EAbilitySlot::Snare };

	for (int32 Index = 0; Index < AllSlots.Num(); ++Index)
	{
		const EAbilitySlot Slot = AllSlots[Index];

		TestEqual(*FString::Printf(TEXT("Get(%d).Ability should equal the requested slot"), static_cast<int32>(Slot)),
			static_cast<uint8>(AbilityData::Get(Slot).Ability), static_cast<uint8>(Slot));

		TestEqual(*FString::Printf(TEXT("GetAll()[%d] should be slot %d (declaration order)"), Index, static_cast<int32>(Slot)),
			static_cast<uint8>(AllAbilities[Index].Ability), static_cast<uint8>(Slot));

		int32 MatchCount = 0;
		for (const FAbilityData& Entry : AllAbilities)
		{
			if (Entry.Ability == Slot)
			{
				++MatchCount;
			}
		}
		TestEqual(*FString::Printf(TEXT("Slot %d should appear exactly once in GetAll()"), static_cast<int32>(Slot)), MatchCount, 1);
	}

	// (3) Each ability's stats exactly match the GDD table.
	const FAbilityData& Stun = AbilityData::Get(EAbilitySlot::Stun);
	TestEqual(TEXT("Stun BaseDurationSeconds should be 3.0"), Stun.BaseDurationSeconds, 3.0f);
	TestEqual(TEXT("Stun Range should be Short"), static_cast<uint8>(Stun.Range), static_cast<uint8>(EAbilityRange::Short));
	TestEqual(TEXT("Stun TargetType should be Single"), static_cast<uint8>(Stun.TargetType), static_cast<uint8>(EAbilityTargetType::Single));
	TestEqual(TEXT("Stun Colour should be the reserved White"), Stun.Colour, ReservedGameplayColours::GetWhite());

	const FAbilityData& Sleep = AbilityData::Get(EAbilitySlot::Sleep);
	TestEqual(TEXT("Sleep BaseDurationSeconds should be 5.0"), Sleep.BaseDurationSeconds, 5.0f);
	TestEqual(TEXT("Sleep Range should be Long"), static_cast<uint8>(Sleep.Range), static_cast<uint8>(EAbilityRange::Long));
	TestEqual(TEXT("Sleep TargetType should be Single"), static_cast<uint8>(Sleep.TargetType), static_cast<uint8>(EAbilityTargetType::Single));
	TestEqual(TEXT("Sleep Colour should be the reserved Blue"), Sleep.Colour, ReservedGameplayColours::GetBlue());

	const FAbilityData& Root = AbilityData::Get(EAbilitySlot::Root);
	TestEqual(TEXT("Root BaseDurationSeconds should be 5.0"), Root.BaseDurationSeconds, 5.0f);
	TestEqual(TEXT("Root Range should be Long"), static_cast<uint8>(Root.Range), static_cast<uint8>(EAbilityRange::Long));
	TestEqual(TEXT("Root TargetType should be Single"), static_cast<uint8>(Root.TargetType), static_cast<uint8>(EAbilityTargetType::Single));
	TestEqual(TEXT("Root Colour should be the reserved Teal"), Root.Colour, ReservedGameplayColours::GetTeal());

	const FAbilityData& Fear = AbilityData::Get(EAbilitySlot::Fear);
	TestEqual(TEXT("Fear BaseDurationSeconds should be 5.0"), Fear.BaseDurationSeconds, 5.0f);
	TestEqual(TEXT("Fear Range should be Short"), static_cast<uint8>(Fear.Range), static_cast<uint8>(EAbilityRange::Short));
	TestEqual(TEXT("Fear TargetType should be Area"), static_cast<uint8>(Fear.TargetType), static_cast<uint8>(EAbilityTargetType::Area));
	TestEqual(TEXT("Fear Colour should be the reserved Orange"), Fear.Colour, ReservedGameplayColours::GetOrange());

	const FAbilityData& Snare = AbilityData::Get(EAbilitySlot::Snare);
	TestEqual(TEXT("Snare BaseDurationSeconds should be 4.0"), Snare.BaseDurationSeconds, 4.0f);
	TestEqual(TEXT("Snare Range should be Medium"), static_cast<uint8>(Snare.Range), static_cast<uint8>(EAbilityRange::Medium));
	TestEqual(TEXT("Snare TargetType should be Cone"), static_cast<uint8>(Snare.TargetType), static_cast<uint8>(EAbilityTargetType::Cone));
	TestEqual(TEXT("Snare Colour should be the reserved Purple"), Snare.Colour, ReservedGameplayColours::GetPurple());

	// (4) Stun is the only colour-neutral ability - the acceptance criterion "Stun has
	// no countered-enemy value set."
	TestTrue(TEXT("Stun should be colour-neutral"), Stun.bIsColourNeutral);
	TestFalse(TEXT("Sleep should not be colour-neutral"), Sleep.bIsColourNeutral);
	TestFalse(TEXT("Root should not be colour-neutral"), Root.bIsColourNeutral);
	TestFalse(TEXT("Fear should not be colour-neutral"), Fear.bIsColourNeutral);
	TestFalse(TEXT("Snare should not be colour-neutral"), Snare.bIsColourNeutral);

	// (5) The 5 colours are mutually distinct - guards against a copy-paste that
	// accidentally reuses one colour for two abilities.
	for (int32 i = 0; i < AllAbilities.Num(); ++i)
	{
		for (int32 j = i + 1; j < AllAbilities.Num(); ++j)
		{
			TestNotEqual(*FString::Printf(TEXT("Ability colours %d and %d should be mutually distinct"), i, j),
				AllAbilities[i].Colour, AllAbilities[j].Colour);
		}
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
