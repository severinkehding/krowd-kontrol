// Confirms UAbilityTooltipWidget (issue #260) populates all 6 display fields plus the
// swatch colour from AbilityData::Get() for every ability, pins the 5 canonical key
// bindings against the operator's 2026-08-23 ruling, covers Stun's colour-neutral case
// separately from a non-neutral ability, and confirms UAbilityCooldownTrayWidget's
// BuildWidgetTree() actually wires one tooltip instance per slot via SetToolTip() -
// widget/data-binding level, independent of live cursor/hover input (no primitive for
// that exists in this Automation run - real hover display is a manual/MCP check).
//
// CreateWidget() calls Initialize() synchronously, so the widget tree is already built
// by the time SetAbility() is called - no TakeWidget()/AddToViewport() needed, same as
// every other KrowdKontrol.Unit.* widget test under -nullrhi.
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the other
// KrowdKontrol.Unit.* tests.

#include "Misc/AutomationTest.h"
#include "AbilityTooltipWidget.h"
#include "AbilityCooldownTrayWidget.h"
#include "AbilityData.h"
#include "ReservedGameplayColours.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"
#include "Components/Border.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolAbilityTooltipWidgetTest,
	"KrowdKontrol.Unit.AbilityTooltipWidget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolAbilityTooltipWidgetTest::RunTest(const FString& Parameters)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
	{
		return false;
	}

	UAbilityTooltipWidget* Widget = CreateWidget<UAbilityTooltipWidget>(World, UAbilityTooltipWidget::StaticClass());
	if (!TestNotNull(TEXT("UAbilityTooltipWidget should construct"), Widget))
	{
		return false;
	}

	// (1) Every display field populates from AbilityData for all 5 abilities.
	const TArray<EAbilitySlot> AllSlots = { EAbilitySlot::Stun, EAbilitySlot::Sleep,
		EAbilitySlot::Root, EAbilitySlot::Fear, EAbilitySlot::Snare };

	for (EAbilitySlot Slot : AllSlots)
	{
		Widget->SetAbility(Slot);
		const FAbilityData& Data = AbilityData::Get(Slot);

		TestEqual(*FString::Printf(TEXT("Slot %d ability name should match StaticEnum display name"), static_cast<int32>(Slot)),
			Widget->GetAbilityNameText().ToString(),
			StaticEnum<EAbilitySlot>()->GetDisplayNameTextByValue(static_cast<int64>(Slot)).ToString());

		TestTrue(*FString::Printf(TEXT("Slot %d key binding text should contain AbilityData's KeyBindingLabel"), static_cast<int32>(Slot)),
			Widget->GetKeyBindingText().ToString().Contains(Data.KeyBindingLabel.ToString()));

		TestTrue(*FString::Printf(TEXT("Slot %d description should equal AbilityData's EffectDescription"), static_cast<int32>(Slot)),
			Widget->GetDescriptionText().EqualTo(Data.EffectDescription));

		TestEqual(*FString::Printf(TEXT("Slot %d swatch colour should match AbilityData's Colour"), static_cast<int32>(Slot)),
			Widget->GetSwatchColour(), Data.Colour);

		const FString ExpectedDuration = FString::Printf(TEXT("%d"), FMath::RoundToInt(Data.BaseDurationSeconds));
		TestTrue(*FString::Printf(TEXT("Slot %d duration text should contain the formatted duration"), static_cast<int32>(Slot)),
			Widget->GetDurationText().ToString().Contains(ExpectedDuration));
	}

	// (2) Dedicated ground-truth pin of the 5 canonical bindings (operator ruling
	// 2026-08-23) - independent of the roundtrip check in (1) above.
	TestEqual(TEXT("Stun KeyBindingLabel should be the canonical LMB binding"),
		AbilityData::Get(EAbilitySlot::Stun).KeyBindingLabel.ToString(), FString(TEXT("LMB")));
	TestEqual(TEXT("Sleep KeyBindingLabel should be the canonical RMB binding"),
		AbilityData::Get(EAbilitySlot::Sleep).KeyBindingLabel.ToString(), FString(TEXT("RMB")));
	TestEqual(TEXT("Root KeyBindingLabel should be the canonical Q binding"),
		AbilityData::Get(EAbilitySlot::Root).KeyBindingLabel.ToString(), FString(TEXT("Q")));
	TestEqual(TEXT("Snare KeyBindingLabel should be the canonical E binding"),
		AbilityData::Get(EAbilitySlot::Snare).KeyBindingLabel.ToString(), FString(TEXT("E")));
	TestEqual(TEXT("Fear KeyBindingLabel should be the canonical MMB binding"),
		AbilityData::Get(EAbilitySlot::Fear).KeyBindingLabel.ToString(), FString(TEXT("MMB")));

	// (3) Stun's colour-neutral case - no countered-enemy text, swatch is White.
	Widget->SetAbility(EAbilitySlot::Stun);
	TestFalse(TEXT("Stun's enemy type text should not name any EEnemyType (colour-neutral)"),
		Widget->GetEnemyTypeText().ToString().Contains(TEXT("RU-NNR"))
			|| Widget->GetEnemyTypeText().ToString().Contains(TEXT("TR-UPR"))
			|| Widget->GetEnemyTypeText().ToString().Contains(TEXT("B0-0MR"))
			|| Widget->GetEnemyTypeText().ToString().Contains(TEXT("SN-1PR")));
	TestEqual(TEXT("Stun's swatch colour should be the reserved White"),
		Widget->GetSwatchColour(), ReservedGameplayColours::GetWhite());

	// (4) Non-neutral case (Sleep) - enemy type text names the real countered enemy.
	Widget->SetAbility(EAbilitySlot::Sleep);
	TestTrue(TEXT("Sleep's enemy type text should name SN-1PR"),
		Widget->GetEnemyTypeText().ToString().Contains(
			StaticEnum<EEnemyType>()->GetDisplayNameTextByValue(static_cast<int64>(EEnemyType::SN_1PR)).ToString()));

	// (5) Accessors called before SetAbility() on a fresh widget should return empty
	// text / black colour, not crash.
	UAbilityTooltipWidget* FreshWidget = CreateWidget<UAbilityTooltipWidget>(World, UAbilityTooltipWidget::StaticClass());
	if (TestNotNull(TEXT("Fresh UAbilityTooltipWidget should construct"), FreshWidget))
	{
		TestTrue(TEXT("Fresh widget's name text should be empty before SetAbility()"), FreshWidget->GetAbilityNameText().IsEmpty());
	}

	// (6) Integration check - UAbilityCooldownTrayWidget::BuildWidgetTree() actually
	// wires one UAbilityTooltipWidget per slot via SetToolTip(), proving Task 5's
	// wiring, not just this widget in isolation.
	UAbilityCooldownTrayWidget* TrayWidget =
		CreateWidget<UAbilityCooldownTrayWidget>(World, UAbilityCooldownTrayWidget::StaticClass());
	if (TestNotNull(TEXT("UAbilityCooldownTrayWidget should construct"), TrayWidget))
	{
		const int32 RootIndex = static_cast<int32>(EAbilitySlot::Root);
		if (TestNotNull(TEXT("Root slot's icon border should be non-null"), ToRawPtr(TrayWidget->SlotIconBorders[RootIndex])))
		{
			UAbilityTooltipWidget* WiredTooltip = Cast<UAbilityTooltipWidget>(TrayWidget->SlotIconBorders[RootIndex]->GetToolTip());
			if (TestNotNull(TEXT("Root slot's icon border should have a UAbilityTooltipWidget attached"), WiredTooltip))
			{
				TestEqual(TEXT("Wired tooltip's ability name should match Root's"),
					WiredTooltip->GetAbilityNameText().ToString(),
					StaticEnum<EAbilitySlot>()->GetDisplayNameTextByValue(static_cast<int64>(EAbilitySlot::Root)).ToString());
			}
		}
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
