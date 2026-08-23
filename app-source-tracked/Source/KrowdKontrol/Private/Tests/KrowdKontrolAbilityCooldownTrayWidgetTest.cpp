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
#include "AbilityLockoutComponent.h"
#include "AbilityCooldownComponent.h"
#include "AbilityData.h"
#include "ReservedGameplayColours.h"
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

	// (f3) Default-unlocked state - every slot reports IsSlotLocked() == false
	// immediately after construction (mirrors the "(b) Initial placeholder state"
	// loop style above).
	for (int32 Index = 0; Index < UAbilityCooldownTrayWidget::NumAbilitySlots; ++Index)
	{
		TestFalse(*FString::Printf(TEXT("Slot %d should start unlocked"), Index),
			Widget->IsSlotLocked(static_cast<EAbilitySlot>(Index)));
	}

	// (f4) Locked-vs-cooldown visual diff on the same slot (the issue's explicit test
	// requirement) - Root was already cleared by the (f2) block above, so start it on
	// a fresh cooldown here to prove locking suppresses the countdown display without
	// clearing the underlying cooldown state.
	Widget->StartCooldown(EAbilitySlot::Root, 5.0f);
	TestEqual(TEXT("Root label before locking should be the ability abbreviation"),
		Widget->GetSlotIconLabelText(EAbilitySlot::Root).ToString(), FString(TEXT("ROT")));
	TestEqual(TEXT("Root cooldown display before locking should show ceil(5.0) = 5"),
		Widget->GetSlotCooldownDisplayText(EAbilitySlot::Root).ToString(), FString(TEXT("5")));
	const FLinearColor RootBorderColourBeforeLock = Widget->SlotIconBorders[static_cast<int32>(EAbilitySlot::Root)]->GetBrushColor();

	Widget->SetSlotLocked(EAbilitySlot::Root, true);
	TestTrue(TEXT("Root should report locked after SetSlotLocked(true)"), Widget->IsSlotLocked(EAbilitySlot::Root));
	TestEqual(TEXT("Root label should swap to LCK while locked"),
		Widget->GetSlotIconLabelText(EAbilitySlot::Root).ToString(), FString(TEXT("LCK")));
	const FLinearColor RootBorderColourLocked = Widget->SlotIconBorders[static_cast<int32>(EAbilitySlot::Root)]->GetBrushColor();
	TestNotEqual(TEXT("Root border colour should change while locked"), RootBorderColourLocked, RootBorderColourBeforeLock);
	TestTrue(TEXT("Root cooldown display should be suppressed while locked"),
		Widget->GetSlotCooldownDisplayText(EAbilitySlot::Root).IsEmpty());
	TestEqual(TEXT("Root remaining cooldown time should be unchanged by locking (suppression is display-only)"),
		Widget->GetSlotRemainingSeconds(EAbilitySlot::Root), 5.0f);
	TestEqual(TEXT("Root icon tint colour should be unaffected by locking"),
		Widget->GetSlotIconTintColour(EAbilitySlot::Root), AbilityData::Get(EAbilitySlot::Root).Colour);

	// (f5) Locked border colour is reserved-colour-safe (MISSION.md Hard Invariant 3 /
	// PRD 13 REQ-4).
	const TArray<FLinearColor> AllReservedForLockCheck = ReservedGameplayColours::GetAll();
	TestFalse(TEXT("Locked border colour should not collide with a reserved gameplay colour"),
		AllReservedForLockCheck.Contains(RootBorderColourLocked));

	// (f6) Unlock reverts the label and border, without clearing the cooldown that was
	// still counting down underneath while locked.
	Widget->SetSlotLocked(EAbilitySlot::Root, false);
	TestFalse(TEXT("Root should report unlocked after SetSlotLocked(false)"), Widget->IsSlotLocked(EAbilitySlot::Root));
	TestEqual(TEXT("Root label should revert to ROT after unlocking"),
		Widget->GetSlotIconLabelText(EAbilitySlot::Root).ToString(), FString(TEXT("ROT")));
	TestEqual(TEXT("Root border colour should revert to the chrome background constant after unlocking"),
		Widget->SlotIconBorders[static_cast<int32>(EAbilitySlot::Root)]->GetBrushColor(), RootBorderColourBeforeLock);
	TestEqual(TEXT("Root cooldown display should show the still-running cooldown again after unlocking"),
		Widget->GetSlotCooldownDisplayText(EAbilitySlot::Root).ToString(), FString(TEXT("5")));
	TestEqual(TEXT("Root remaining cooldown time should still be 5s - unlocking must not have cleared it"),
		Widget->GetSlotRemainingSeconds(EAbilitySlot::Root), 5.0f);
	TestEqual(TEXT("Root icon tint colour should still be unaffected after unlocking"),
		Widget->GetSlotIconTintColour(EAbilitySlot::Root), AbilityData::Get(EAbilitySlot::Root).Colour);

	// (f7) AdvanceCooldowns() while locked - underlying timer keeps ticking silently
	// (locked suppresses only the display, per UpdateSlotVisual's locked branch), and
	// unlocking reveals the updated remaining time rather than a stale pre-lock value.
	Widget->StartCooldown(EAbilitySlot::Sleep, 5.0f);
	Widget->SetSlotLocked(EAbilitySlot::Sleep, true);
	Widget->AdvanceCooldowns(2.0f);
	TestEqual(TEXT("Locked slot's underlying cooldown should still advance"),
		Widget->GetSlotRemainingSeconds(EAbilitySlot::Sleep), 3.0f);
	TestTrue(TEXT("Locked slot's cooldown display should stay suppressed while advancing"),
		Widget->GetSlotCooldownDisplayText(EAbilitySlot::Sleep).IsEmpty());
	Widget->SetSlotLocked(EAbilitySlot::Sleep, false);
	TestEqual(TEXT("Unlocking after AdvanceCooldowns() should reveal the updated remaining time"),
		Widget->GetSlotCooldownDisplayText(EAbilitySlot::Sleep).ToString(), FString(TEXT("3")));

	// (f8) Underlying cooldown can fully exhaust while locked; unlocking should then show
	// "ready", not a stale non-zero display.
	Widget->StartCooldown(EAbilitySlot::Sleep, 1.0f);
	Widget->SetSlotLocked(EAbilitySlot::Sleep, true);
	Widget->AdvanceCooldowns(5.0f);
	Widget->SetSlotLocked(EAbilitySlot::Sleep, false);
	TestFalse(TEXT("Slot whose cooldown exhausted while locked should not be on cooldown after unlocking"),
		Widget->IsSlotOnCooldown(EAbilitySlot::Sleep));
	TestTrue(TEXT("Slot whose cooldown exhausted while locked should show empty display after unlocking"),
		Widget->GetSlotCooldownDisplayText(EAbilitySlot::Sleep).IsEmpty());

	// (f9) Locking a slot that isn't currently on cooldown (idle) - distinct branch
	// combination from (f4)-(f8), which all lock a slot with an active cooldown. Fear
	// was cleared by block (e)'s large-delta AdvanceCooldowns(100.0f) above.
	TestFalse(TEXT("Fear should be idle (not on cooldown) before the idle-lock check"),
		Widget->IsSlotOnCooldown(EAbilitySlot::Fear));
	Widget->SetSlotLocked(EAbilitySlot::Fear, true);
	TestTrue(TEXT("Fear should report locked after locking an idle slot"), Widget->IsSlotLocked(EAbilitySlot::Fear));
	TestEqual(TEXT("Fear label should swap to LCK when locked while idle"),
		Widget->GetSlotIconLabelText(EAbilitySlot::Fear).ToString(), FString(TEXT("LCK")));
	TestTrue(TEXT("Fear cooldown display should stay empty when locked while idle"),
		Widget->GetSlotCooldownDisplayText(EAbilitySlot::Fear).IsEmpty());
	Widget->SetSlotLocked(EAbilitySlot::Fear, false);
	TestFalse(TEXT("Fear should report unlocked after unlocking"), Widget->IsSlotLocked(EAbilitySlot::Fear));
	TestEqual(TEXT("Fear label should revert to FER after unlocking an idle-locked slot"),
		Widget->GetSlotIconLabelText(EAbilitySlot::Fear).ToString(), FString(TEXT("FER")));

	// (f10) Idempotent SetSlotLocked() calls - locking an already-locked slot, or
	// unlocking an already-unlocked slot, should leave state/visuals unchanged.
	Widget->SetSlotLocked(EAbilitySlot::Fear, true);
	const FText FearLabelAfterFirstLock = Widget->GetSlotIconLabelText(EAbilitySlot::Fear);
	Widget->SetSlotLocked(EAbilitySlot::Fear, true);
	TestTrue(TEXT("Fear should still report locked after a repeated lock call"), Widget->IsSlotLocked(EAbilitySlot::Fear));
	TestEqual(TEXT("Fear label should be unchanged by a repeated lock call"),
		Widget->GetSlotIconLabelText(EAbilitySlot::Fear).ToString(), FearLabelAfterFirstLock.ToString());
	Widget->SetSlotLocked(EAbilitySlot::Fear, false);
	Widget->SetSlotLocked(EAbilitySlot::Fear, false);
	TestFalse(TEXT("Fear should still report unlocked after a repeated unlock call"), Widget->IsSlotLocked(EAbilitySlot::Fear));

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
	TestFalse(TEXT("Unbuilt widget should report not locked"), UnbuiltWidget->IsSlotLocked(EAbilitySlot::Stun));
	TestEqual(TEXT("Unbuilt widget should report Ready state"), UnbuiltWidget->GetSlotState(EAbilitySlot::Stun), EAbilityTileState::Ready);
	TestEqual(TEXT("Unbuilt widget should report 0 punishment-lockout remaining seconds"),
		UnbuiltWidget->GetSlotPunishmentLockoutRemainingSeconds(EAbilitySlot::Stun), 0.0f);
	UnbuiltWidget->AdvanceCooldowns(1.0f);
	UnbuiltWidget->StartCooldown(EAbilitySlot::Stun, 2.0f);
	UnbuiltWidget->SetSlotLocked(EAbilitySlot::Stun, true);
	TestTrue(TEXT("AdvanceCooldowns()/StartCooldown()/SetSlotLocked() on an unbuilt tree should not crash"), true);

	// (j) NativeTick actually drives AdvanceCooldowns() - the real per-frame code path a
	// live game session ticks, not just the direct AdvanceCooldowns() calls used above.
	// Calling the protected override directly (via friend-class access) sidesteps the
	// -nullrhi headless run's inability to drive live Slate ticking, while still
	// exercising the real call site rather than only its AdvanceCooldowns() proxy.
	Widget->StartCooldown(EAbilitySlot::Stun, 3.0f);
	Widget->NativeTick(FGeometry(), 1.0f);
	TestEqual(TEXT("NativeTick should advance cooldowns via AdvanceCooldowns()"),
		Widget->GetSlotRemainingSeconds(EAbilitySlot::Stun), 2.0f);

	// (k) BindAbilityLockoutComponent (issue #178, Punishment 1) drives the tray
	// through real activation and expiry via its own tracked state (issue #261) -
	// GetSlotState() reports PunishmentLockout, not merely IsSlotLocked(), and the
	// numeric readout tracks the component's live remaining time as it counts down,
	// not just at the start/expiry transitions.
	{
		UAbilityLockoutComponent* LockoutComponent = NewObject<UAbilityLockoutComponent>();
		if (!TestNotNull(TEXT("UAbilityLockoutComponent should construct"), LockoutComponent))
		{
			return false;
		}
		Widget->BindAbilityLockoutComponent(LockoutComponent);

		TestEqual(TEXT("Sleep should read Ready before any punishment trigger"),
			Widget->GetSlotState(EAbilitySlot::Sleep), EAbilityTileState::Ready);

		LockoutComponent->HandleAbilityCastApplied(EAbilitySlot::Sleep, nullptr);
		LockoutComponent->HandlePunishmentTriggered();
		TestEqual(TEXT("Sleep should read PunishmentLockout on the tray after a real punishment trigger"),
			Widget->GetSlotState(EAbilitySlot::Sleep), EAbilityTileState::PunishmentLockout);
		TestEqual(TEXT("Sleep's punishment-lockout remaining should seed from the component's full duration"),
			Widget->GetSlotPunishmentLockoutRemainingSeconds(EAbilitySlot::Sleep), UAbilityLockoutComponent::DefaultLockoutDurationSeconds);
		TestEqual(TEXT("Sleep's cooldown display text should show the punishment-lockout countdown"),
			Widget->GetSlotCooldownDisplayText(EAbilitySlot::Sleep).ToString(),
			FString::FromInt(FMath::CeilToInt(UAbilityLockoutComponent::DefaultLockoutDurationSeconds)));

		// Partial advance - the numeric readout must track the live remaining time,
		// not just show a value frozen at activation.
		LockoutComponent->AdvanceLockouts(3.0f);
		Widget->RefreshPunishmentLockoutReadouts();
		TestEqual(TEXT("Sleep's punishment-lockout remaining should reflect the partial advance"),
			Widget->GetSlotPunishmentLockoutRemainingSeconds(EAbilitySlot::Sleep), UAbilityLockoutComponent::DefaultLockoutDurationSeconds - 3.0f);
		TestEqual(TEXT("Sleep should still read PunishmentLockout mid-countdown"),
			Widget->GetSlotState(EAbilitySlot::Sleep), EAbilityTileState::PunishmentLockout);

		// Lock a second slot (Root) on the same component alongside Sleep, to prove
		// RefreshPunishmentLockoutReadouts()'s per-slot loop updates every active slot
		// in one call, not just the first it finds.
		LockoutComponent->HandleAbilityCastApplied(EAbilitySlot::Root, nullptr);
		LockoutComponent->HandlePunishmentTriggered();
		LockoutComponent->AdvanceLockouts(1.0f);
		Widget->RefreshPunishmentLockoutReadouts();
		TestEqual(TEXT("Sleep's readout should still update while a second slot is also locked out"),
			Widget->GetSlotPunishmentLockoutRemainingSeconds(EAbilitySlot::Sleep), UAbilityLockoutComponent::DefaultLockoutDurationSeconds - 4.0f);
		TestEqual(TEXT("Root's readout should update independently of Sleep's"),
			Widget->GetSlotPunishmentLockoutRemainingSeconds(EAbilitySlot::Root), UAbilityLockoutComponent::DefaultLockoutDurationSeconds - 1.0f);

		LockoutComponent->AdvanceLockouts(UAbilityLockoutComponent::DefaultLockoutDurationSeconds);
		TestEqual(TEXT("Sleep should read Ready on the tray again after the lockout expires"),
			Widget->GetSlotState(EAbilitySlot::Sleep), EAbilityTileState::Ready);
		TestEqual(TEXT("Sleep's punishment-lockout remaining should clear to 0 on expiry"),
			Widget->GetSlotPunishmentLockoutRemainingSeconds(EAbilitySlot::Sleep), 0.0f);
	}

	// (l) Null-guard branches on BindAbilityLockoutComponent/BindAbilityUnlockComponent
	// must degrade safely (log-and-return) rather than crash, and must leave the tray's
	// current state untouched.
	{
		TestEqual(TEXT("Sleep should still read Ready before the null-guard calls"),
			Widget->GetSlotState(EAbilitySlot::Sleep), EAbilityTileState::Ready);
		Widget->BindAbilityLockoutComponent(nullptr);
		Widget->BindAbilityUnlockComponent(nullptr);
		TestTrue(TEXT("BindAbilityLockoutComponent(nullptr)/BindAbilityUnlockComponent(nullptr) should not crash"), true);
		TestEqual(TEXT("Sleep should still read Ready after the null-guard calls"),
			Widget->GetSlotState(EAbilitySlot::Sleep), EAbilityTileState::Ready);
	}

	// (m) Four-state distinctness (issue #261's explicit acceptance criterion) - a
	// single slot (Snare, untouched and idle since block (e)'s large-delta clear)
	// walked through Ready -> Cooldown -> NotYetUnlocked -> PunishmentLockout,
	// asserting GetSlotState() reports the correct, distinct value at each step, and
	// that the two locked-style states (NotYetUnlocked, PunishmentLockout) - which
	// render with the same border/label treatment - are still distinguishable via
	// the numeric-readout-presence the issue calls out explicitly.
	{
		TestEqual(TEXT("Snare should read Ready before this block"), Widget->GetSlotState(EAbilitySlot::Snare), EAbilityTileState::Ready);
		TestTrue(TEXT("Snare should show empty display text while Ready"), Widget->GetSlotCooldownDisplayText(EAbilitySlot::Snare).IsEmpty());

		Widget->StartCooldown(EAbilitySlot::Snare, 4.0f);
		TestEqual(TEXT("Snare should read Cooldown after StartCooldown()"), Widget->GetSlotState(EAbilitySlot::Snare), EAbilityTileState::Cooldown);
		TestEqual(TEXT("Snare cooldown display should show ceil(4.0) = 4"),
			Widget->GetSlotCooldownDisplayText(EAbilitySlot::Snare).ToString(), FString(TEXT("4")));

		Widget->SetSlotLocked(EAbilitySlot::Snare, true);
		TestEqual(TEXT("Snare should read NotYetUnlocked once locked, overriding Cooldown"), Widget->GetSlotState(EAbilitySlot::Snare), EAbilityTileState::NotYetUnlocked);
		TestTrue(TEXT("Snare should show NO numeric readout while NotYetUnlocked"), Widget->GetSlotCooldownDisplayText(EAbilitySlot::Snare).IsEmpty());
		Widget->SetSlotLocked(EAbilitySlot::Snare, false);
		TestEqual(TEXT("Snare should revert to Cooldown after unlocking (its cooldown was still running underneath)"),
			Widget->GetSlotState(EAbilitySlot::Snare), EAbilityTileState::Cooldown);
		Widget->AdvanceCooldowns(100.0f);
		TestEqual(TEXT("Snare should read Ready again after its cooldown fully clears"), Widget->GetSlotState(EAbilitySlot::Snare), EAbilityTileState::Ready);

		UAbilityLockoutComponent* SnareLockoutComponent = NewObject<UAbilityLockoutComponent>();
		if (TestNotNull(TEXT("Second UAbilityLockoutComponent should construct for the distinctness block"), SnareLockoutComponent))
		{
			Widget->BindAbilityLockoutComponent(SnareLockoutComponent);

			// PunishmentLockout must win against an actually-active Cooldown, not just
			// a cleared one - direct proof of that precedence leg, independent of the
			// NotYetUnlocked simultaneity case tested below.
			Widget->StartCooldown(EAbilitySlot::Snare, 3.0f);
			TestEqual(TEXT("Snare should read Cooldown right before the punishment trigger"), Widget->GetSlotState(EAbilitySlot::Snare), EAbilityTileState::Cooldown);

			SnareLockoutComponent->HandleAbilityCastApplied(EAbilitySlot::Snare, nullptr);
			SnareLockoutComponent->HandlePunishmentTriggered();
			TestEqual(TEXT("PunishmentLockout must take precedence over a simultaneous active Cooldown"),
				Widget->GetSlotState(EAbilitySlot::Snare), EAbilityTileState::PunishmentLockout);
			TestEqual(TEXT("Snare should read PunishmentLockout, distinct from NotYetUnlocked and Cooldown"),
				Widget->GetSlotState(EAbilitySlot::Snare), EAbilityTileState::PunishmentLockout);
			TestFalse(TEXT("Snare should show a NON-empty numeric readout while PunishmentLockout - this is what distinguishes it from NotYetUnlocked"),
				Widget->GetSlotCooldownDisplayText(EAbilitySlot::Snare).IsEmpty());
			TestEqual(TEXT("Snare label should swap to LCK while PunishmentLockout, same as NotYetUnlocked"),
				Widget->GetSlotIconLabelText(EAbilitySlot::Snare).ToString(), FString(TEXT("LCK")));

			// Clear the cooldown started above so it doesn't linger underneath and
			// affect the state read once the punishment lockout itself expires below.
			Widget->AdvanceCooldowns(100.0f);

			// Simultaneity: locking Snare as not-yet-unlocked WHILE it is also
			// punishment-locked must keep PunishmentLockout as the reported state
			// (precedence), proving the two states are tracked independently rather
			// than collapsing into one bool.
			Widget->SetSlotLocked(EAbilitySlot::Snare, true);
			TestEqual(TEXT("PunishmentLockout must take precedence over a simultaneous NotYetUnlocked flag"),
				Widget->GetSlotState(EAbilitySlot::Snare), EAbilityTileState::PunishmentLockout);
			Widget->SetSlotLocked(EAbilitySlot::Snare, false);

			SnareLockoutComponent->AdvanceLockouts(UAbilityLockoutComponent::DefaultLockoutDurationSeconds);
			TestEqual(TEXT("Snare should read Ready once the punishment lockout expires"), Widget->GetSlotState(EAbilitySlot::Snare), EAbilityTileState::Ready);
		}
	}

	// (n) Rebind-to-a-different-live-component (issue #268 code-review Finding 1) -
	// BindAbilityLockoutComponent() must unbind from a previously-bound component
	// before subscribing to a new one, so a broadcast from the OLD component after
	// the rebind can't corrupt the tray via HandleAbilityLockoutChanged's
	// currently-bound-component read.
	{
		UAbilityLockoutComponent* FirstComponent = NewObject<UAbilityLockoutComponent>();
		UAbilityLockoutComponent* SecondComponent = NewObject<UAbilityLockoutComponent>();
		if (TestNotNull(TEXT("First UAbilityLockoutComponent should construct for the rebind test"), FirstComponent)
			&& TestNotNull(TEXT("Second UAbilityLockoutComponent should construct for the rebind test"), SecondComponent))
		{
			Widget->BindAbilityLockoutComponent(FirstComponent);
			FirstComponent->HandleAbilityCastApplied(EAbilitySlot::Fear, nullptr);
			FirstComponent->HandlePunishmentTriggered();
			TestEqual(TEXT("Fear should read PunishmentLockout while bound to the first component"),
				Widget->GetSlotState(EAbilitySlot::Fear), EAbilityTileState::PunishmentLockout);

			Widget->BindAbilityLockoutComponent(SecondComponent);

			// The old (first) component's lockout expiring should NOT reach the tray
			// anymore - if the widget were still subscribed, this broadcast would
			// spuriously clear Fear's PunishmentLockout state.
			FirstComponent->AdvanceLockouts(UAbilityLockoutComponent::DefaultLockoutDurationSeconds);
			TestEqual(TEXT("Fear should still read PunishmentLockout after rebinding - the old component's expiry broadcast must not reach the widget"),
				Widget->GetSlotState(EAbilitySlot::Fear), EAbilityTileState::PunishmentLockout);
		}
	}

	// (o) BindAbilityCooldownComponent (issue #259) drives the tray's fill/numeric
	// readout through real cooldown activation and expiry, sourced from the actual
	// UAbilityCooldownComponent - not the placeholder self-timer. Mirrors block (k)'s
	// BindAbilityLockoutComponent shape. Sleep is Ready here - block (k) locked and
	// then fully cleared it, block (l)/(m)/(n) never touch it again.
	{
		UAbilityCooldownComponent* CooldownComponent = NewObject<UAbilityCooldownComponent>();
		if (!TestNotNull(TEXT("UAbilityCooldownComponent should construct"), CooldownComponent))
		{
			return false;
		}
		Widget->BindAbilityCooldownComponent(CooldownComponent);

		TestEqual(TEXT("Sleep should read Ready before any real cast"),
			Widget->GetSlotState(EAbilitySlot::Sleep), EAbilityTileState::Ready);

		CooldownComponent->TryStartCooldown(EAbilitySlot::Sleep);
		TestTrue(TEXT("Sleep should be on cooldown on the tray immediately after the real component starts it"),
			Widget->IsSlotOnCooldown(EAbilitySlot::Sleep));
		TestEqual(TEXT("Sleep's tray remaining should seed from the real component's full duration"),
			Widget->GetSlotRemainingSeconds(EAbilitySlot::Sleep), UAbilityCooldownComponent::DefaultAbilityCooldownSeconds);
		TestEqual(TEXT("Sleep's fill fraction should be full (1.0) right after starting"),
			Widget->GetSlotCooldownFillFraction(EAbilitySlot::Sleep), 1.0f);
		TestEqual(TEXT("Sleep's numeric readout should show the real component's ceil'd duration"),
			Widget->GetSlotCooldownDisplayText(EAbilitySlot::Sleep).ToString(),
			FString::FromInt(FMath::CeilToInt(UAbilityCooldownComponent::DefaultAbilityCooldownSeconds)));

		// Partial advance - drive the real component directly (friend access, no live
		// tick under -nullrhi), then pull the tray's readout via the same poll path
		// NativeTick uses when bound.
		CooldownComponent->AdvanceCooldowns(1.0f);
		Widget->RefreshCooldownReadouts();
		TestEqual(TEXT("Sleep's tray remaining should reflect the partial advance"),
			Widget->GetSlotRemainingSeconds(EAbilitySlot::Sleep), UAbilityCooldownComponent::DefaultAbilityCooldownSeconds - 1.0f);
		const float ExpectedPartialFraction = (UAbilityCooldownComponent::DefaultAbilityCooldownSeconds - 1.0f) / UAbilityCooldownComponent::DefaultAbilityCooldownSeconds;
		TestEqual(TEXT("Sleep's fill fraction should reflect the partial advance"),
			Widget->GetSlotCooldownFillFraction(EAbilitySlot::Sleep), ExpectedPartialFraction);

		// Expiry - the real component's own AdvanceCooldowns broadcasts false, which the
		// tray is subscribed to directly (no RefreshCooldownReadouts call needed here).
		CooldownComponent->AdvanceCooldowns(UAbilityCooldownComponent::DefaultAbilityCooldownSeconds - 1.0f);
		TestEqual(TEXT("Sleep should read Ready on the tray again after the real cooldown expires"),
			Widget->GetSlotState(EAbilitySlot::Sleep), EAbilityTileState::Ready);
		TestEqual(TEXT("Sleep's fill fraction should clear to 0 on expiry"),
			Widget->GetSlotCooldownFillFraction(EAbilitySlot::Sleep), 0.0f);
		TestTrue(TEXT("Sleep's display text should be empty once the real cooldown clears"),
			Widget->GetSlotCooldownDisplayText(EAbilitySlot::Sleep).IsEmpty());
		TestTrue(TEXT("Sleep should show an active ready-flash immediately after real-cooldown expiry"),
			Widget->IsSlotReadyFlashActive(EAbilitySlot::Sleep));
		TestEqual(TEXT("Sleep's ready-flash remaining should equal the full flash duration right after expiry"),
			Widget->GetSlotReadyFlashRemainingSeconds(EAbilitySlot::Sleep), 0.15f);

		Widget->AdvanceReadyFlashTimers(0.2f); // exceeds the flash duration
		TestFalse(TEXT("Ready-flash should clear after its duration elapses"),
			Widget->IsSlotReadyFlashActive(EAbilitySlot::Sleep));
	}

	// (p) Null-guard on BindAbilityCooldownComponent must degrade safely, matching the
	// existing (l) block's coverage of the other two Bind* null-guards.
	{
		Widget->BindAbilityCooldownComponent(nullptr);
		TestTrue(TEXT("BindAbilityCooldownComponent(nullptr) should not crash"), true);
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
