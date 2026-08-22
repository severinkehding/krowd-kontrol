// Confirms UEnergyMeterWidget (issue #64) renders a placeholder energy value
// immediately on construction, that SetEnergy() clamps and formats correctly
// (including the MaxEnergy <= 0 divide-by-zero guard), that BindToEnergyComponent()
// syncs immediately to a real UPlayerEnergyComponent's current state and then follows
// its OnEnergyChanged broadcasts live (via a real ApplyContactDamage() call, not a
// direct SetEnergy() call), that rebinding to a different component actually
// unsubscribes from the old one, and that the meter is genuinely anchored to the
// top-left corner with reserved-colour-safe chrome. Also confirms an unbuilt widget
// tree degrades to safe defaults rather than crashing, and the Initialize() safety
// net's "already built, skip" branch doesn't rebuild the tree when
// NativeOnInitialized() already ran - same reasoning as
// KrowdKontrolAbilityCooldownTrayWidgetTest.cpp (issue #66).
//
// CreateWidget() calls Initialize() synchronously, which fires NativeOnInitialized()
// - no TakeWidget()/Slate realization needed for most of this test, so it works under
// the -nullrhi flag KrowdKontrol.Unit.* tests run with (see
// harness/run_ue_automation.sh). Needs a real UWorld (CreateWidget's first argument).
// The Initialize()-guard and unbuilt-tree cases use a bare NewObject() instead (no
// World needed), matching KrowdKontrolAbilityCooldownTrayWidgetTest.cpp's precedent.
// The bound UPlayerEnergyComponent uses a bare NewObject() too, mirroring
// KrowdKontrolPlayerEnergyComponentTest.cpp's own precedent (ApplyContactDamage()
// needs neither GetWorld() nor GetOwner()), and its CurrentEnergy is friend-seeded
// (FKrowdKontrolEnergyMeterWidgetTest, see PlayerEnergyComponent.h) since
// ApplyContactDamage() only ever decreases energy and BeginPlay() needs a live actor
// this test deliberately avoids.
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the other
// KrowdKontrol.Unit.* tests.

#include "Misc/AutomationTest.h"
#include "EnergyMeterWidget.h"
#include "PlayerEnergyComponent.h"
#include "HUDChromeColours.h"
#include "ReservedGameplayColours.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolEnergyMeterWidgetTest,
	"KrowdKontrol.Unit.EnergyMeterWidget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolEnergyMeterWidgetTest::RunTest(const FString& Parameters)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
	{
		return false;
	}

	UEnergyMeterWidget* Widget = CreateWidget<UEnergyMeterWidget>(World, UEnergyMeterWidget::StaticClass());
	if (!TestNotNull(TEXT("UEnergyMeterWidget should construct"), Widget))
	{
		return false;
	}

	// (1)/(2) Placeholder state renders immediately on construction.
	TestNotNull(TEXT("FillBar should be non-null"), ToRawPtr(Widget->FillBar));
	TestNotNull(TEXT("ValueText should be non-null"), ToRawPtr(Widget->ValueText));
	TestNotNull(TEXT("BackgroundBorder should be non-null"), ToRawPtr(Widget->BackgroundBorder));
	TestEqual(TEXT("Placeholder fraction should be 0.72"), Widget->GetDisplayedFraction(), 0.72f);
	TestEqual(TEXT("Placeholder display text"), Widget->GetEnergyDisplayText().ToString(), FString(TEXT("72/100")));

	// (3) SetEnergy() updates fraction and text.
	Widget->SetEnergy(50.0f, 100.0f);
	TestEqual(TEXT("Fraction after SetEnergy(50, 100)"), Widget->GetDisplayedFraction(), 0.5f);
	TestEqual(TEXT("Display text after SetEnergy(50, 100)"), Widget->GetEnergyDisplayText().ToString(), FString(TEXT("50/100")));

	// (4) Clamping - above max and below zero.
	Widget->SetEnergy(150.0f, 100.0f);
	TestEqual(TEXT("Fraction should clamp to 1.0 when CurrentEnergy exceeds MaxEnergy"), Widget->GetDisplayedFraction(), 1.0f);
	TestEqual(TEXT("Display text should clamp to 100/100"), Widget->GetEnergyDisplayText().ToString(), FString(TEXT("100/100")));
	Widget->SetEnergy(-20.0f, 100.0f);
	TestEqual(TEXT("Fraction should clamp to 0.0 when CurrentEnergy is negative"), Widget->GetDisplayedFraction(), 0.0f);
	TestEqual(TEXT("Display text should clamp to 0/100"), Widget->GetEnergyDisplayText().ToString(), FString(TEXT("0/100")));

	// (5) Divide-by-zero guard.
	Widget->SetEnergy(10.0f, 0.0f);
	TestEqual(TEXT("Fraction should be 0.0 when MaxEnergy <= 0, not a crash or NaN"), Widget->GetDisplayedFraction(), 0.0f);
	TestEqual(TEXT("Display text should be 0/0 when MaxEnergy <= 0"), Widget->GetEnergyDisplayText().ToString(), FString(TEXT("0/0")));

	// (6) BindToEnergyComponent() syncs immediately to the bound component's state.
	UPlayerEnergyComponent* EnergyComponent = NewObject<UPlayerEnergyComponent>();
	if (!TestNotNull(TEXT("UPlayerEnergyComponent should construct"), EnergyComponent))
	{
		return false;
	}
	EnergyComponent->MaxEnergy = 100.0f;
	EnergyComponent->MaxDamagePerHit = 50.0f;
	EnergyComponent->CurrentEnergy = 80.0f;

	Widget->BindToEnergyComponent(EnergyComponent);
	TestEqual(TEXT("Fraction should sync to bound component's state"), Widget->GetDisplayedFraction(), 0.8f);
	TestEqual(TEXT("Display text should sync to bound component's state"), Widget->GetEnergyDisplayText().ToString(), FString(TEXT("80/100")));

	// (7) Live update via the real OnEnergyChanged broadcast, not a direct SetEnergy() call.
	EnergyComponent->ApplyContactDamage(30.0f, nullptr);
	TestEqual(TEXT("Fraction should follow OnEnergyChanged after ApplyContactDamage"), Widget->GetDisplayedFraction(), 0.5f);
	TestEqual(TEXT("Display text should follow OnEnergyChanged after ApplyContactDamage"), Widget->GetEnergyDisplayText().ToString(), FString(TEXT("50/100")));

	// (7b) Same-frame damage-flash reaction (issue #222, PRD "Level Progression &
	// Teaching Arc" REQ-4) - HandleEnergyChanged's PlayDamageFlash() activates
	// synchronously off the exact ApplyContactDamage() broadcast used in (7), before
	// any NativeTick. AdvanceDamageFlashTimer() is called directly afterward to
	// simulate the countdown expiring, mirroring
	// KrowdKontrolAbilityVFXColourTest.cpp's direct ClearCastFlash() call - this
	// Automation Unit test never drives a live NativeTick()/tick loop.
	TestTrue(TEXT("Damage flash should be active immediately after a real ApplyContactDamage() broadcast"),
		Widget->IsDamageFlashActive());
	if (TestNotNull(TEXT("DamageFlashOverlay should exist"), ToRawPtr(Widget->DamageFlashOverlay)))
	{
		TestEqual(TEXT("Damage flash overlay should be visible immediately after the broadcast"),
			Widget->DamageFlashOverlay->GetVisibility(), ESlateVisibility::HitTestInvisible);
		const FLinearColor FlashColour = Widget->DamageFlashOverlay->GetBrushColor();
		TestFalse(TEXT("Damage flash overlay colour should not collide with a reserved gameplay colour"),
			ReservedGameplayColours::GetAll().ContainsByPredicate(
				[FlashColour](const FLinearColor& Reserved) { return Reserved.Equals(FlashColour, 0.01f); }));
	}

	Widget->AdvanceDamageFlashTimer(UEnergyMeterWidget::DamageFlashDurationSeconds + 0.01f);
	TestFalse(TEXT("Damage flash should clear once its duration has fully elapsed"), Widget->IsDamageFlashActive());
	if (Widget->DamageFlashOverlay)
	{
		TestEqual(TEXT("Damage flash overlay should be collapsed again after it clears"),
			Widget->DamageFlashOverlay->GetVisibility(), ESlateVisibility::Collapsed);
	}

	// A direct SetEnergy() call (not via the OnEnergyChanged path) must NOT trigger
	// the flash - the reaction hook is tied specifically to the energy-decrease
	// broadcast, not to every display update.
	Widget->SetEnergy(40.0f, 100.0f);
	TestFalse(TEXT("A direct SetEnergy() call should not itself trigger the damage flash"), Widget->IsDamageFlashActive());

	// (8) Rebind isolation - switching to a second component unsubscribes from the first.
	UPlayerEnergyComponent* SecondEnergyComponent = NewObject<UPlayerEnergyComponent>();
	if (!TestNotNull(TEXT("Second UPlayerEnergyComponent should construct"), SecondEnergyComponent))
	{
		return false;
	}
	SecondEnergyComponent->MaxEnergy = 100.0f;
	SecondEnergyComponent->MaxDamagePerHit = 50.0f;
	SecondEnergyComponent->CurrentEnergy = 40.0f;

	Widget->BindToEnergyComponent(SecondEnergyComponent);
	TestEqual(TEXT("Fraction should switch to the second component's state"), Widget->GetDisplayedFraction(), 0.4f);

	EnergyComponent->ApplyContactDamage(50.0f, nullptr);
	TestEqual(TEXT("Fraction should NOT move when the now-unbound first component takes damage"), Widget->GetDisplayedFraction(), 0.4f);

	// (8b) Unbind via nullptr from an actively-bound component actually unsubscribes -
	// the null-check/pointer-equality ordering in BindToEnergyComponent() must still
	// call RemoveDynamic() on the previously-bound component, not just skip re-subscribing.
	Widget->BindToEnergyComponent(nullptr);
	SecondEnergyComponent->ApplyContactDamage(10.0f, nullptr);
	TestEqual(TEXT("Fraction should NOT move when unbound via nullptr and the old component takes damage"),
		Widget->GetDisplayedFraction(), 0.4f);

	// (9) Corner anchoring - the meter's single canvas child is actually anchored
	// top-left, not just visually eyeballed. Diagonally opposite the tray's
	// FAnchors(1,1,1,1)/FVector2D(1,1).
	UCanvasPanel* RootCanvas = Cast<UCanvasPanel>(Widget->WidgetTree->RootWidget);
	if (TestNotNull(TEXT("Widget root should be a UCanvasPanel"), RootCanvas))
	{
		TestEqual(TEXT("Root canvas should have exactly one child"), RootCanvas->GetChildrenCount(), 1);
		if (RootCanvas->GetChildrenCount() == 1)
		{
			UCanvasPanelSlot* MeterSlot = Cast<UCanvasPanelSlot>(RootCanvas->GetChildAt(0)->Slot);
			if (TestNotNull(TEXT("Meter child's slot should be a UCanvasPanelSlot"), MeterSlot))
			{
				TestTrue(TEXT("Meter should be anchored to the top-left corner"),
					MeterSlot->GetAnchors() == FAnchors(0.0f, 0.0f, 0.0f, 0.0f));
				TestTrue(TEXT("Meter should be aligned to its own top-left corner"),
					MeterSlot->GetAlignment() == FVector2D(0.0f, 0.0f));
			}
		}
	}

	// (9b) Resolution safety - the meter's pixel footprint stays inside a small
	// top-left corner region rather than creeping toward the central play area, at
	// both ends of this project's target resolution range. MISSION.md does not pin
	// down an exact resolution list yet, so this uses the same low/high bounds
	// (1280x720 minimum, 3840x2160 / 4K maximum) this genre's HUD-safe-area
	// reasoning typically assumes. Since the footprint is a fixed pixel size
	// anchored to the corner, the low (smallest) resolution is the binding case: if
	// the corner region is safe there, every larger resolution in the range is
	// strictly safer (the same fixed-size box occupies a smaller fraction of a
	// bigger screen) - both are still checked explicitly rather than relying on
	// that argument alone.
	const float MeterFootprintWidthPx = UEnergyMeterWidget::MeterMarginPx + UEnergyMeterWidget::MeterWidthPx;
	const float MeterFootprintHeightPx = UEnergyMeterWidget::MeterMarginPx + UEnergyMeterWidget::MeterHeightPx;
	const float SafeCornerFraction = 0.25f;
	const FVector2D TargetResolutions[] = { FVector2D(1280.0f, 720.0f), FVector2D(3840.0f, 2160.0f) };
	for (const FVector2D& TargetResolution : TargetResolutions)
	{
		TestTrue(*FString::Printf(TEXT("Meter footprint width should stay within the top-left %.0f%% corner region at %dx%d"),
			SafeCornerFraction * 100.0f, (int32)TargetResolution.X, (int32)TargetResolution.Y),
			MeterFootprintWidthPx <= TargetResolution.X * SafeCornerFraction);
		TestTrue(*FString::Printf(TEXT("Meter footprint height should stay within the top-left %.0f%% corner region at %dx%d"),
			SafeCornerFraction * 100.0f, (int32)TargetResolution.X, (int32)TargetResolution.Y),
			MeterFootprintHeightPx <= TargetResolution.Y * SafeCornerFraction);
	}

	// (10) Chrome compliance - MISSION.md Hard Invariant 3 reserves
	// Purple/Teal/Orange/Blue/White for gameplay information; the meter's background
	// must not drift onto one of those values.
	TestEqual(TEXT("Background border colour should match the reserved-colour-safe chrome constant"),
		Widget->BackgroundBorder->GetBrushColor(), HUDChromeColours::GetBackground());

	// (11) Initialize() guard - must not rebuild the tree when NativeOnInitialized()
	// already ran.
	UEnergyMeterWidget* GuardWidget = NewObject<UEnergyMeterWidget>();
	if (!TestNotNull(TEXT("UEnergyMeterWidget should construct for guard test"), GuardWidget))
	{
		return false;
	}
	GuardWidget->NativeOnInitialized();
	if (!TestNotNull(TEXT("FillBar should be populated after NativeOnInitialized()"), ToRawPtr(GuardWidget->FillBar)))
	{
		return false;
	}
	UProgressBar* FirstFillBar = GuardWidget->FillBar;
	GuardWidget->Initialize();
	TestEqual(TEXT("Initialize() must not rebuild the tree when already built"),
		ToRawPtr(GuardWidget->FillBar), FirstFillBar);

	// (12) Unbuilt-tree safety - a widget whose tree was never built (bare
	// NewObject(), neither NativeOnInitialized() nor Initialize() called) should
	// degrade safely rather than crashing.
	UEnergyMeterWidget* UnbuiltWidget = NewObject<UEnergyMeterWidget>();
	if (!TestNotNull(TEXT("UEnergyMeterWidget should construct for unbuilt-tree test"), UnbuiltWidget))
	{
		return false;
	}
	TestEqual(TEXT("Unbuilt widget should report 0.0 fraction"), UnbuiltWidget->GetDisplayedFraction(), 0.0f);
	TestTrue(TEXT("Unbuilt widget should report empty display text"), UnbuiltWidget->GetEnergyDisplayText().IsEmpty());
	UnbuiltWidget->SetEnergy(50.0f, 100.0f);
	UnbuiltWidget->BindToEnergyComponent(nullptr);
	TestTrue(TEXT("SetEnergy()/BindToEnergyComponent(nullptr) on an unbuilt tree should not crash"), true);

	// (13) AddToViewport()/IsInViewport() - CreateNewMap() gives an editor-context
	// UWorld with no PIE session and no live game viewport (see
	// KrowdKontrolAbilityCooldownTrayWidgetTest.cpp/KrowdKontrolPostRunSummaryWidgetTest.cpp,
	// neither of which attempt this). Under -nullrhi there is no
	// UGameViewportSubsystem target for this editor world to attach to, so
	// AddToViewport() is expected to no-op rather than crash; downgraded per Task 3's
	// GOTCHA to a no-crash check rather than asserting IsInViewport() == true. A
	// PIE-driven Smoke-tier test is the correct place to verify real viewport
	// realization, tracked as a follow-up rather than forced in here.
	Widget->AddToViewport();
	TestTrue(TEXT("AddToViewport() should not crash even with no live game viewport target"), true);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
