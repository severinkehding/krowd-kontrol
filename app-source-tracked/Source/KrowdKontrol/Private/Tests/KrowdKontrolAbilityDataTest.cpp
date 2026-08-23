// Confirms AbilityData (issue #63, PRD 02 REQ-3) exposes exactly the 5 locked
// per-ability base stats (duration, range, target type, colour) from the GDD table,
// that only Stun is colour-neutral (MISSION.md Hard Invariant 4 - Stun's countered-
// enemy value is a don't-care), that the other 4 abilities' CounteredEnemyType
// (issue #37) matches ReservedGameplayColours.h's locked pairing, and that the 5
// colours are mutually distinct.
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
	TestEqual(TEXT("Stun TargetType should be ThrownCircle"), static_cast<uint8>(Stun.TargetType), static_cast<uint8>(EAbilityTargetType::ThrownCircle));
	TestEqual(TEXT("Stun Colour should be the reserved White"), Stun.Colour, ReservedGameplayColours::GetWhite());
	TestEqual(TEXT("Stun ColourTag should be White"), Stun.ColourTag, ReservedGameplayColours::GetWhiteTag());

	const FAbilityData& Sleep = AbilityData::Get(EAbilitySlot::Sleep);
	TestEqual(TEXT("Sleep BaseDurationSeconds should be 5.0"), Sleep.BaseDurationSeconds, 5.0f);
	TestEqual(TEXT("Sleep Range should be Long"), static_cast<uint8>(Sleep.Range), static_cast<uint8>(EAbilityRange::Long));
	TestEqual(TEXT("Sleep TargetType should be ThrownCircle"), static_cast<uint8>(Sleep.TargetType), static_cast<uint8>(EAbilityTargetType::ThrownCircle));
	TestEqual(TEXT("Sleep Colour should be the reserved Blue"), Sleep.Colour, ReservedGameplayColours::GetBlue());
	TestEqual(TEXT("Sleep ColourTag should be Blue"), Sleep.ColourTag, ReservedGameplayColours::GetBlueTag());
	TestEqual(TEXT("Sleep CounteredEnemyType should be SN-1PR"), static_cast<uint8>(Sleep.CounteredEnemyType), static_cast<uint8>(EEnemyType::SN_1PR));

	const FAbilityData& Root = AbilityData::Get(EAbilitySlot::Root);
	TestEqual(TEXT("Root BaseDurationSeconds should be 5.0"), Root.BaseDurationSeconds, 5.0f);
	TestEqual(TEXT("Root Range should be Long"), static_cast<uint8>(Root.Range), static_cast<uint8>(EAbilityRange::Long));
	TestEqual(TEXT("Root TargetType should be Line"), static_cast<uint8>(Root.TargetType), static_cast<uint8>(EAbilityTargetType::Line));
	TestEqual(TEXT("Root Colour should be the reserved Teal"), Root.Colour, ReservedGameplayColours::GetTeal());
	TestEqual(TEXT("Root ColourTag should be Teal"), Root.ColourTag, ReservedGameplayColours::GetTealTag());
	TestEqual(TEXT("Root CounteredEnemyType should be TR-UPR"), static_cast<uint8>(Root.CounteredEnemyType), static_cast<uint8>(EEnemyType::TR_UPR));

	const FAbilityData& Fear = AbilityData::Get(EAbilitySlot::Fear);
	TestEqual(TEXT("Fear BaseDurationSeconds should be 5.0"), Fear.BaseDurationSeconds, 5.0f);
	TestEqual(TEXT("Fear Range should be Short"), static_cast<uint8>(Fear.Range), static_cast<uint8>(EAbilityRange::Short));
	TestEqual(TEXT("Fear TargetType should be SelfCircle"), static_cast<uint8>(Fear.TargetType), static_cast<uint8>(EAbilityTargetType::SelfCircle));
	TestEqual(TEXT("Fear Colour should be the reserved Orange"), Fear.Colour, ReservedGameplayColours::GetOrange());
	TestEqual(TEXT("Fear ColourTag should be Orange"), Fear.ColourTag, ReservedGameplayColours::GetOrangeTag());
	TestEqual(TEXT("Fear CounteredEnemyType should be B0-0MR"), static_cast<uint8>(Fear.CounteredEnemyType), static_cast<uint8>(EEnemyType::B0_0MR));

	const FAbilityData& Snare = AbilityData::Get(EAbilitySlot::Snare);
	TestEqual(TEXT("Snare BaseDurationSeconds should be 4.0"), Snare.BaseDurationSeconds, 4.0f);
	TestEqual(TEXT("Snare Range should be Medium"), static_cast<uint8>(Snare.Range), static_cast<uint8>(EAbilityRange::Medium));
	TestEqual(TEXT("Snare TargetType should be Cone"), static_cast<uint8>(Snare.TargetType), static_cast<uint8>(EAbilityTargetType::Cone));
	TestEqual(TEXT("Snare Colour should be the reserved Purple"), Snare.Colour, ReservedGameplayColours::GetPurple());
	TestEqual(TEXT("Snare ColourTag should be Purple"), Snare.ColourTag, ReservedGameplayColours::GetPurpleTag());
	TestEqual(TEXT("Snare CounteredEnemyType should be RU-NNR"), static_cast<uint8>(Snare.CounteredEnemyType), static_cast<uint8>(EEnemyType::RU_NNR));

	// (4) Stun is the only colour-neutral ability - the acceptance criterion "Stun has
	// no countered-enemy value set." (Stun.CounteredEnemyType is deliberately not
	// asserted here - it's a don't-care default, not a real counter.)
	TestTrue(TEXT("Stun should be colour-neutral"), Stun.bIsColourNeutral);
	TestFalse(TEXT("Sleep should not be colour-neutral"), Sleep.bIsColourNeutral);
	TestFalse(TEXT("Root should not be colour-neutral"), Root.bIsColourNeutral);
	TestFalse(TEXT("Fear should not be colour-neutral"), Fear.bIsColourNeutral);
	TestFalse(TEXT("Snare should not be colour-neutral"), Snare.bIsColourNeutral);

	// (4b) Issue #257: only Sleep flags bWakesEarlyOnOtherAbilityHit - being hit by a
	// different ability while Controlled ends that Controlled window immediately.
	TestFalse(TEXT("Stun should not wake early on other-ability hit"), Stun.bWakesEarlyOnOtherAbilityHit);
	TestTrue(TEXT("Sleep should wake early on other-ability hit"), Sleep.bWakesEarlyOnOtherAbilityHit);
	TestFalse(TEXT("Root should not wake early on other-ability hit"), Root.bWakesEarlyOnOtherAbilityHit);
	TestFalse(TEXT("Fear should not wake early on other-ability hit"), Fear.bWakesEarlyOnOtherAbilityHit);
	TestFalse(TEXT("Snare should not wake early on other-ability hit"), Snare.bWakesEarlyOnOtherAbilityHit);

	// (5) The 5 colours (and their FName tags) are mutually distinct - guards against a
	// copy-paste that accidentally reuses one colour, or one colour's tag, for two
	// abilities.
	for (int32 i = 0; i < AllAbilities.Num(); ++i)
	{
		for (int32 j = i + 1; j < AllAbilities.Num(); ++j)
		{
			TestNotEqual(*FString::Printf(TEXT("Ability colours %d and %d should be mutually distinct"), i, j),
				AllAbilities[i].Colour, AllAbilities[j].Colour);
			TestNotEqual(*FString::Printf(TEXT("Ability ColourTags %d and %d should be mutually distinct"), i, j),
				AllAbilities[i].ColourTag, AllAbilities[j].ColourTag);
		}
	}

	// (6) Default-constructed FAbilityData's TargetType is SelfCircle - not reachable
	// through AbilityData::Get()/GetAll() today (all 5 rows override it explicitly),
	// but pins the value so a future change to the default isn't silent.
	TestEqual(TEXT("Default FAbilityData TargetType should be SelfCircle"),
		static_cast<uint8>(FAbilityData{}.TargetType), static_cast<uint8>(EAbilityTargetType::SelfCircle));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
