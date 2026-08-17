// Confirms UAbilityCooldownTrayWidget (issue #66) renders all 5 ability slots
// simultaneously with independent placeholder cooldown timers, that each slot's
// cooldown overlay updates as AdvanceCooldowns() advances it and clears independently
// once its remaining time hits zero, that StartCooldown() re-triggers a single slot in
// isolation, and that the tray is actually anchored to the bottom-right corner (not
// just visually assumed). Also confirms an unbuilt widget tree degrades to safe
// defaults rather than crashing, and the Initialize() safety net's "already built,
// skip" branch doesn't rebuild the tree when NativeOnInitialized() already ran - same
// reasoning as KrowdKontrolPostRunSummaryWidgetTest.cpp (issue #74).
//
// CreateWidget() calls Initialize() synchronously, which fires
// NativeOnInitialized() - no TakeWidget()/AddToViewport()/Slate realization needed, so
// this works under the -nullrhi flag KrowdKontrol.Unit.* tests run with (see
// harness/run_ue_automation.sh). Needs a real UWorld (CreateWidget's first argument).
// The Initialize()-guard and unbuilt-tree cases use a bare NewObject() instead (no
// World needed) via this test class's friend-class access, matching
// KrowdKontrolPostRunSummaryWidgetTest.cpp's precedent. Most of this test calls
// AdvanceCooldowns() directly, since NativeTick's usual driver - live Slate ticking -
// isn't available under -nullrhi; the NativeTick() override itself is still called
// directly once (via friend-class access, bypassing Slate) to cover the real per-frame
// call site rather than relying solely on the AdvanceCooldowns() proxy.
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the other
// KrowdKontrol.Unit.* tests.

#include "Misc/AutomationTest.h"
#include "AbilityCooldownTrayWidget.h"
#include "AbilityData.h"
#include "HUDChromeColours.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolAbilityCooldownTrayWidgetTest,
	"KrowdKontrol.Unit.AbilityCooldownTrayWidget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolAbilityCooldownTrayWidgetTest::RunTest(const FString& Parameters)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
	{
		return false;
	}

	UAbilityCooldownTrayWidget* Widget =
		CreateWidget<UAbilityCooldownTrayWidget>(World, UAbilityCooldownTrayWidget::StaticClass());
	if (!TestNotNull(TEXT("UAbilityCooldownTrayWidget should construct"), Widget))
	{
		return false;
	}

	// (a) All 5 icon slots render simultaneously immediately on construction, each
	// labelled in the same order as EAbilitySlot (Stun, Sleep, Root, Fear, Snare) - the
	// label text isn't compiler-linked to the enum, so this loop also guards against a
	// future independent reorder of either silently mislabelling a slot.
	TestEqual(TEXT("SlotIconBorders should have exactly 5 entries"), Widget->SlotIconBorders.Num(), UAbilityCooldownTrayWidget::NumAbilitySlots);
	TestEqual(TEXT("SlotCooldownTexts should have exactly 5 entries"), Widget->SlotCooldownTexts.Num(), UAbilityCooldownTrayWidget::NumAbilitySlots);
	const TCHAR* ExpectedSlotLabels[] = { TEXT("STN"), TEXT("SLP"), TEXT("ROT"), TEXT("FER"), TEXT("SNR") };
	for (int32 Index = 0; Index < UAbilityCooldownTrayWidget::NumAbilitySlots; ++Index)
	{
		TestNotNull(*FString::Printf(TEXT("SlotIconBorders[%d] should be non-null"), Index), ToRawPtr(Widget->SlotIconBorders[Index]));
		TestNotNull(*FString::Printf(TEXT("SlotCooldownTexts[%d] should be non-null"), Index), ToRawPtr(Widget->SlotCooldownTexts[Index]));

		UTextBlock* IconLabel = Cast<UTextBlock>(Widget->SlotIconBorders[Index]->GetContent());
		if (TestNotNull(*FString::Printf(TEXT("SlotIconBorders[%d] content should be a UTextBlock"), Index), IconLabel))
		{
			TestEqual(*FString::Printf(TEXT("Slot %d label text"), Index), IconLabel->GetText().ToString(), FString(ExpectedSlotLabels[Index]));
			TestEqual(*FString::Printf(TEXT("Slot %d icon label colour should match AbilityData's reserved colour"), Index),
				IconLabel->GetColorAndOpacity().GetSpecifiedColor(), AbilityData::Get(static_cast<EAbilitySlot>(Index)).Colour);
		}
	}

	// (a2) Chrome palette compliance - MISSION.md Hard Invariant 3 reserves
	// Purple/Teal/Orange/Blue/White for gameplay information; the tray's background
	// must not drift onto one of those values.
	TestEqual(TEXT("Icon border background should match the reserved-colour-safe chrome constant"),
		Widget->SlotIconBorders[0]->GetBrushColor(), HUDChromeColours::GetBackground());

	// (a3) Shape/glyph distinctness (PRD 13 REQ-7 / issue #76) - each slot's label
	// text differs from every other slot's, independent of whatever colour is
	// applied, so a colourblind player can identify a slot from text alone.
	for (int32 i = 0; i < UAbilityCooldownTrayWidget::NumAbilitySlots; ++i)
	{
		for (int32 j = i + 1; j < UAbilityCooldownTrayWidget::NumAbilitySlots; ++j)
		{
			TestNotEqual(*FString::Printf(TEXT("Slot %d and %d label text should be distinct"), i, j),
				Widget->GetSlotIconLabelText(static_cast<EAbilitySlot>(i)).ToString(),
				Widget->GetSlotIconLabelText(static_cast<EAbilitySlot>(j)).ToString());
		}
	}
	for (int32 Index = 0; Index < UAbilityCooldownTrayWidget::NumAbilitySlots; ++Index)
	{
		const EAbilitySlot Slot = static_cast<EAbilitySlot>(Index);
		TestEqual(*FString::Printf(TEXT("Slot %d GetSlotIconTintColour should match AbilityData"), Index),
			Widget->GetSlotIconTintColour(Slot), AbilityData::Get(Slot).Colour);
	}

	// (b) Initial placeholder state - all 5 slots seeded on cooldown, with distinct
	// durations, immediately after construction.
	TestTrue(TEXT("Stun should start on cooldown"), Widget->IsSlotOnCooldown(EAbilitySlot::Stun));
	TestTrue(TEXT("Sleep should start on cooldown"), Widget->IsSlotOnCooldown(EAbilitySlot::Sleep));
	TestTrue(TEXT("Root should start on cooldown"), Widget->IsSlotOnCooldown(EAbilitySlot::Root));
	TestTrue(TEXT("Fear should start on cooldown"), Widget->IsSlotOnCooldown(EAbilitySlot::Fear));
	TestTrue(TEXT("Snare should start on cooldown"), Widget->IsSlotOnCooldown(EAbilitySlot::Snare));
	TestEqual(TEXT("Stun placeholder duration"), Widget->GetSlotRemainingSeconds(EAbilitySlot::Stun), 2.0f);
	TestEqual(TEXT("Sleep placeholder duration"), Widget->GetSlotRemainingSeconds(EAbilitySlot::Sleep), 3.0f);
	TestEqual(TEXT("Root placeholder duration"), Widget->GetSlotRemainingSeconds(EAbilitySlot::Root), 4.0f);
	TestEqual(TEXT("Fear placeholder duration"), Widget->GetSlotRemainingSeconds(EAbilitySlot::Fear), 5.0f);
	TestEqual(TEXT("Snare placeholder duration"), Widget->GetSlotRemainingSeconds(EAbilitySlot::Snare), 6.0f);

	// (c) Overlay updates as the timer advances.
	Widget->AdvanceCooldowns(1.0f);
	TestEqual(TEXT("Stun remaining after 1s"), Widget->GetSlotRemainingSeconds(EAbilitySlot::Stun), 1.0f);
	TestEqual(TEXT("Sleep remaining after 1s"), Widget->GetSlotRemainingSeconds(EAbilitySlot::Sleep), 2.0f);
	TestEqual(TEXT("Root remaining after 1s"), Widget->GetSlotRemainingSeconds(EAbilitySlot::Root), 3.0f);
	TestEqual(TEXT("Fear remaining after 1s"), Widget->GetSlotRemainingSeconds(EAbilitySlot::Fear), 4.0f);
	TestEqual(TEXT("Snare remaining after 1s"), Widget->GetSlotRemainingSeconds(EAbilitySlot::Snare), 5.0f);
	TestEqual(TEXT("Stun cooldown text should show ceil(1.0) = 1"),
		Widget->GetSlotCooldownDisplayText(EAbilitySlot::Stun).ToString(), FString(TEXT("1")));

	// (d) Clearing is independent per-slot - Stun clears while Sleep doesn't.
	Widget->AdvanceCooldowns(1.0f);
	TestFalse(TEXT("Stun should have cleared"), Widget->IsSlotOnCooldown(EAbilitySlot::Stun));
	TestTrue(TEXT("Sleep should still be on cooldown"), Widget->IsSlotOnCooldown(EAbilitySlot::Sleep));
	TestTrue(TEXT("Stun display text should be empty once cleared, not stale"),
		Widget->GetSlotCooldownDisplayText(EAbilitySlot::Stun).IsEmpty());

	// (e) A large delta clears every slot, and remaining never goes negative.
	Widget->AdvanceCooldowns(100.0f);
	for (int32 Index = 0; Index < UAbilityCooldownTrayWidget::NumAbilitySlots; ++Index)
	{
		const EAbilitySlot Slot = static_cast<EAbilitySlot>(Index);
		TestFalse(*FString::Printf(TEXT("Slot %d should have cleared after a large delta"), Index), Widget->IsSlotOnCooldown(Slot));
		TestEqual(*FString::Printf(TEXT("Slot %d remaining should clamp at 0"), Index), Widget->GetSlotRemainingSeconds(Slot), 0.0f);
	}

	// (f) StartCooldown() re-triggers only the targeted slot - the wiring point a
	// future real ability-cast system (issue #71) will call.
	Widget->StartCooldown(EAbilitySlot::Fear, 5.0f);
	TestTrue(TEXT("Fear should be on cooldown after StartCooldown()"), Widget->IsSlotOnCooldown(EAbilitySlot::Fear));
	TestEqual(TEXT("Fear remaining should equal the StartCooldown() duration"), Widget->GetSlotRemainingSeconds(EAbilitySlot::Fear), 5.0f);
	for (int32 Index = 0; Index < UAbilityCooldownTrayWidget::NumAbilitySlots; ++Index)
	{
		const EAbilitySlot Slot = static_cast<EAbilitySlot>(Index);
		if (Slot == EAbilitySlot::Fear)
		{
			continue;
		}
		TestFalse(*FString::Printf(TEXT("Slot %d should be unaffected by StartCooldown(Fear, ...)"), Index), Widget->IsSlotOnCooldown(Slot));
		TestEqual(*FString::Printf(TEXT("Slot %d remaining should be unaffected by StartCooldown(Fear, ...)"), Index), Widget->GetSlotRemainingSeconds(Slot), 0.0f);
	}
	// Icon tint/text are informational (PRD 13 REQ-7), not cooldown state - a
	// StartCooldown() call must not leak into UpdateSlotVisual()'s icon-label path.
	TestEqual(TEXT("Fear icon tint should be unaffected by StartCooldown()"),
		Widget->GetSlotIconTintColour(EAbilitySlot::Fear), AbilityData::Get(EAbilitySlot::Fear).Colour);
	TestEqual(TEXT("Fear icon label text should be unaffected by StartCooldown()"),
		Widget->GetSlotIconLabelText(EAbilitySlot::Fear).ToString(), FString(TEXT("FER")));

	// (f2) Negative duration clamps to 0 rather than leaving the slot on an
	// effectively-infinite or negative-time cooldown.
	Widget->StartCooldown(EAbilitySlot::Root, -5.0f);
	TestFalse(TEXT("Negative StartCooldown duration should clamp to not-on-cooldown"), Widget->IsSlotOnCooldown(EAbilitySlot::Root));
	TestEqual(TEXT("Negative StartCooldown duration should clamp remaining to 0"), Widget->GetSlotRemainingSeconds(EAbilitySlot::Root), 0.0f);

	// (g) Corner anchoring - the tray's single canvas child is actually anchored
	// bottom-right, not just visually eyeballed.
	UCanvasPanel* RootCanvas = Cast<UCanvasPanel>(Widget->WidgetTree->RootWidget);
	if (TestNotNull(TEXT("Widget root should be a UCanvasPanel"), RootCanvas))
	{
		TestEqual(TEXT("Root canvas should have exactly one child"), RootCanvas->GetChildrenCount(), 1);
		if (RootCanvas->GetChildrenCount() == 1)
		{
			UCanvasPanelSlot* TraySlot = Cast<UCanvasPanelSlot>(RootCanvas->GetChildAt(0)->Slot);
			if (TestNotNull(TEXT("Tray child's slot should be a UCanvasPanelSlot"), TraySlot))
			{
				TestTrue(TEXT("Tray should be anchored to the bottom-right corner"),
					TraySlot->GetAnchors() == FAnchors(1.0f, 1.0f, 1.0f, 1.0f));
				TestTrue(TEXT("Tray should be aligned to its own bottom-right corner"),
					TraySlot->GetAlignment() == FVector2D(1.0f, 1.0f));
			}
		}
	}

	// (h) Initialize() guard - must not rebuild the tree when NativeOnInitialized()
	// already ran.
	UAbilityCooldownTrayWidget* GuardWidget = NewObject<UAbilityCooldownTrayWidget>();
	if (!TestNotNull(TEXT("UAbilityCooldownTrayWidget should construct for guard test"), GuardWidget))
	{
		return false;
	}
	GuardWidget->NativeOnInitialized();
	if (!TestTrue(TEXT("SlotIconBorders should be populated after NativeOnInitialized()"), GuardWidget->SlotIconBorders.Num() == UAbilityCooldownTrayWidget::NumAbilitySlots))
	{
		return false;
	}
	UBorder* FirstIconBorder = GuardWidget->SlotIconBorders[0];
	GuardWidget->Initialize();
	TestEqual(TEXT("Initialize() must not rebuild the tree when already built"),
		ToRawPtr(GuardWidget->SlotIconBorders[0]), FirstIconBorder);

	// (i) Unbuilt-tree safety - a widget whose tree was never built (bare
	// NewObject(), neither NativeOnInitialized() nor Initialize() called) should
	// degrade safely rather than crashing.
	UAbilityCooldownTrayWidget* UnbuiltWidget = NewObject<UAbilityCooldownTrayWidget>();
	if (!TestNotNull(TEXT("UAbilityCooldownTrayWidget should construct for unbuilt-tree test"), UnbuiltWidget))
	{
		return false;
	}
	TestEqual(TEXT("Unbuilt widget should report 0 remaining seconds"), UnbuiltWidget->GetSlotRemainingSeconds(EAbilitySlot::Stun), 0.0f);
	TestFalse(TEXT("Unbuilt widget should report not on cooldown"), UnbuiltWidget->IsSlotOnCooldown(EAbilitySlot::Stun));
	TestTrue(TEXT("Unbuilt widget should report empty display text"), UnbuiltWidget->GetSlotCooldownDisplayText(EAbilitySlot::Stun).IsEmpty());
	TestTrue(TEXT("Unbuilt widget should report empty icon label text"), UnbuiltWidget->GetSlotIconLabelText(EAbilitySlot::Stun).IsEmpty());
	TestEqual(TEXT("Unbuilt widget should report black icon tint colour"), UnbuiltWidget->GetSlotIconTintColour(EAbilitySlot::Stun), FLinearColor::Black);
	UnbuiltWidget->AdvanceCooldowns(1.0f);
	UnbuiltWidget->StartCooldown(EAbilitySlot::Stun, 2.0f);
	TestTrue(TEXT("AdvanceCooldowns()/StartCooldown() on an unbuilt tree should not crash"), true);

	// (j) NativeTick actually drives AdvanceCooldowns() - the real per-frame code path a
	// live game session ticks, not just the direct AdvanceCooldowns() calls used above.
	// Calling the protected override directly (via friend-class access) sidesteps the
	// -nullrhi headless run's inability to drive live Slate ticking, while still
	// exercising the real call site rather than only its AdvanceCooldowns() proxy.
	Widget->StartCooldown(EAbilitySlot::Stun, 3.0f);
	Widget->NativeTick(FGeometry(), 1.0f);
	TestEqual(TEXT("NativeTick should advance cooldowns via AdvanceCooldowns()"),
		Widget->GetSlotRemainingSeconds(EAbilitySlot::Stun), 2.0f);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
