// Confirms UPunishmentDebugMenuWidget (issue #26) - the menu-UI follow-up
// docs/prd-punishment-system.md REQ-5 deferred - correctly drives each punishment's
// existing CVar/instant-end surface: menu visibility toggling, checkboxes reflecting
// live CVar state on construction, unchecking a box both flipping its CVar to 0 and
// ending an already-active effect immediately, a subsequent trigger while still
// unchecked producing no effect, and rechecking restoring normal triggering. This is
// NOT a duplicate of each punishment's own CVar-gates-the-trigger proof, which lives
// in KrowdKontrolAbilityLockoutComponentTest.cpp/
// KrowdKontrolSpeedReductionPunishmentComponentTest.cpp/
// KrowdKontrolOvercrowdDetectionComponentTest.cpp - this test only proves the widget's
// own wiring (CVar read/write + instant-end call correctness).
//
// CreateWidget<T>(World, ...) - not CreateWidget<T>(PlayerController, ...) - since
// FAutomationEditorCommonUtils::CreateNewMap() test Worlds have no local player, and
// the controller-argument overload would CastChecked<ULocalPlayer>() and fail (same
// note as KrowdKontrolAbilityCooldownTrayWidgetTest.cpp).
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the other
// KrowdKontrol.Unit.* tests.

#include "Misc/AutomationTest.h"
#include "PunishmentDebugMenuWidget.h"
#include "AbilityLockoutComponent.h"
#include "SpeedReductionPunishmentComponent.h"
#include "OvercrowdDetectionComponent.h"
#include "EnemyBaseTestActor.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "HAL/IConsoleManager.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolPunishmentDebugMenuWidgetTest,
	"KrowdKontrol.Unit.PunishmentDebugMenuWidget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolPunishmentDebugMenuWidgetTest::RunTest(const FString& Parameters)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
	{
		return false;
	}

	UPunishmentDebugMenuWidget* Widget =
		CreateWidget<UPunishmentDebugMenuWidget>(World, UPunishmentDebugMenuWidget::StaticClass());
	if (!TestNotNull(TEXT("UPunishmentDebugMenuWidget should construct"), Widget))
	{
		return false;
	}

	// (a) Hidden by default; ToggleMenuVisibility() flips visible, calling again flips
	// back to hidden.
	TestFalse(TEXT("Menu should be hidden immediately after construction"), Widget->IsMenuVisible());
	Widget->ToggleMenuVisibility();
	TestTrue(TEXT("ToggleMenuVisibility should show the menu"), Widget->IsMenuVisible());
	Widget->ToggleMenuVisibility();
	TestFalse(TEXT("A second ToggleMenuVisibility should hide the menu again"), Widget->IsMenuVisible());

	// (b) All three toggle accessors read true by default (CVars default to 1).
	TestTrue(TEXT("Lockout toggle should read checked by default"), Widget->IsLockoutToggleChecked());
	TestTrue(TEXT("Speed reduction toggle should read checked by default"), Widget->IsSpeedReductionToggleChecked());
	TestTrue(TEXT("Overcrowd toggle should read checked by default"), Widget->IsOvercrowdToggleChecked());

	AActor* Owner = World->SpawnActor<AActor>();
	if (!TestNotNull(TEXT("AActor owner should spawn"), Owner))
	{
		return false;
	}

	UAbilityLockoutComponent* Lockout = NewObject<UAbilityLockoutComponent>(Owner);
	Lockout->RegisterComponent();

	UFloatingPawnMovement* Movement = NewObject<UFloatingPawnMovement>(Owner);
	Movement->RegisterComponent();
	Movement->MaxSpeed = 1200.0f;

	USpeedReductionPunishmentComponent* SpeedReduction = NewObject<USpeedReductionPunishmentComponent>(Owner);
	SpeedReduction->RegisterComponent();
	SpeedReduction->MovementComponent = Movement;

	UOvercrowdDetectionComponent* Overcrowd = NewObject<UOvercrowdDetectionComponent>(Owner);
	Overcrowd->RegisterComponent();

	Widget->BindPunishmentComponents(Lockout, SpeedReduction, Overcrowd);

	// (c) Ability Lockout: unchecking ends an active lockout immediately and suppresses
	// a subsequent trigger; rechecking restores normal triggering. The
	// kk.Punishment.LockoutEnabled CVar is process-wide state shared by every
	// KrowdKontrol.Unit.* test in the same Automation pass, so this always ends by
	// leaving it at 1, regardless of which assertion below fails first.
	{
		IConsoleVariable* LockoutCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("kk.Punishment.LockoutEnabled"));
		if (!TestNotNull(TEXT("kk.Punishment.LockoutEnabled CVar should be registered"), LockoutCVar))
		{
			return false;
		}

		Lockout->HandlePunishmentTriggered();
		TestTrue(TEXT("Stun should be locked before the checkbox is unchecked"), Lockout->IsAbilityLocked(EAbilitySlot::Stun));

		Widget->HandleLockoutCheckStateChanged(false);
		TestFalse(TEXT("Unchecking the lockout checkbox should end the active lockout immediately"), Lockout->IsAbilityLocked(EAbilitySlot::Stun));
		TestEqual(TEXT("Unchecking the lockout checkbox should flip kk.Punishment.LockoutEnabled to 0"), LockoutCVar->GetInt(), 0);

		Lockout->HandlePunishmentTriggered();
		TestFalse(TEXT("A trigger while the lockout checkbox is unchecked should produce no lockout"), Lockout->IsAbilityLocked(EAbilitySlot::Stun));

		Widget->HandleLockoutCheckStateChanged(true);
		TestEqual(TEXT("Rechecking the lockout checkbox should restore kk.Punishment.LockoutEnabled to 1"), LockoutCVar->GetInt(), 1);

		Lockout->HandlePunishmentTriggered();
		TestTrue(TEXT("A trigger after rechecking should lock normally again"), Lockout->IsAbilityLocked(EAbilitySlot::Stun));

		Lockout->EndAllLockouts();
	}

	// (d) Speed Reduction: same shape as (c).
	{
		IConsoleVariable* SpeedReductionCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("kk.Punishment.SpeedReductionEnabled"));
		if (!TestNotNull(TEXT("kk.Punishment.SpeedReductionEnabled CVar should be registered"), SpeedReductionCVar))
		{
			return false;
		}

		SpeedReduction->HandlePunishmentTriggered();
		TestTrue(TEXT("Speed reduction should be active before the checkbox is unchecked"), SpeedReduction->IsSpeedReductionTimerActive());
		TestEqual(TEXT("MaxSpeed should be reduced before the checkbox is unchecked"), Movement->MaxSpeed, 600.0f);

		Widget->HandleSpeedReductionCheckStateChanged(false);
		TestFalse(TEXT("Unchecking the speed reduction checkbox should end the active reduction immediately"), SpeedReduction->IsSpeedReductionTimerActive());
		TestEqual(TEXT("MaxSpeed should be restored immediately on uncheck"), Movement->MaxSpeed, 1200.0f);
		TestEqual(TEXT("Unchecking the speed reduction checkbox should flip kk.Punishment.SpeedReductionEnabled to 0"), SpeedReductionCVar->GetInt(), 0);

		SpeedReduction->HandlePunishmentTriggered();
		TestFalse(TEXT("A trigger while the speed reduction checkbox is unchecked should produce no reduction"), SpeedReduction->IsSpeedReductionTimerActive());

		Widget->HandleSpeedReductionCheckStateChanged(true);
		TestEqual(TEXT("Rechecking the speed reduction checkbox should restore kk.Punishment.SpeedReductionEnabled to 1"), SpeedReductionCVar->GetInt(), 1);

		SpeedReduction->HandlePunishmentTriggered();
		TestTrue(TEXT("A trigger after rechecking should reduce speed normally again"), SpeedReduction->IsSpeedReductionTimerActive());

		SpeedReduction->EndSpeedReduction();
	}

	// (e) Overcrowd: same shape as (c)/(d), driving to real Active state via friend
	// access to AdvancePanicOverloadState (same recipe as
	// KrowdKontrolPunishmentArbitrationComponentTest.cpp).
	{
		IConsoleVariable* OvercrowdCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("kk.Punishment.OvercrowdEnabled"));
		if (!TestNotNull(TEXT("kk.Punishment.OvercrowdEnabled CVar should be registered"), OvercrowdCVar))
		{
			return false;
		}

		for (int32 Index = 0; Index < Overcrowd->OvercrowdCrowdThreshold; ++Index)
		{
			AEnemyBaseTestActor* Enemy = World->SpawnActor<AEnemyBaseTestActor>();
			if (!TestNotNull(TEXT("AEnemyBaseTestActor should spawn"), Enemy))
			{
				return false;
			}
			Enemy->TickCheckDetection(FVector::ZeroVector); // Idle -> Alert
		}
		Overcrowd->AdvancePanicOverloadState(Overcrowd->OvercrowdUncontrolledDurationSeconds + 10.0f);
		TestEqual(TEXT("Overcrowd should be Active before the checkbox is unchecked"),
			static_cast<uint8>(Overcrowd->GetPanicOverloadState()), static_cast<uint8>(EPanicOverloadState::Active));

		Widget->HandleOvercrowdCheckStateChanged(false);
		TestEqual(TEXT("Unchecking the Overcrowd checkbox should end the active Panic Overload immediately"),
			static_cast<uint8>(Overcrowd->GetPanicOverloadState()), static_cast<uint8>(EPanicOverloadState::Inactive));
		TestEqual(TEXT("Unchecking the Overcrowd checkbox should flip kk.Punishment.OvercrowdEnabled to 0"), OvercrowdCVar->GetInt(), 0);

		// The qualifying enemies from above are still hot-and-uncontrolled, so this
		// re-proves the trigger condition being met again produces no effect while disabled.
		Overcrowd->AdvancePanicOverloadState(Overcrowd->OvercrowdUncontrolledDurationSeconds + 10.0f);
		TestEqual(TEXT("A trigger condition met while the Overcrowd checkbox is unchecked should produce no effect"),
			static_cast<uint8>(Overcrowd->GetPanicOverloadState()), static_cast<uint8>(EPanicOverloadState::Inactive));

		Widget->HandleOvercrowdCheckStateChanged(true);
		TestEqual(TEXT("Rechecking the Overcrowd checkbox should restore kk.Punishment.OvercrowdEnabled to 1"), OvercrowdCVar->GetInt(), 1);

		Overcrowd->AdvancePanicOverloadState(Overcrowd->OvercrowdUncontrolledDurationSeconds + 10.0f);
		TestEqual(TEXT("A trigger after rechecking should flip to Active normally again"),
			static_cast<uint8>(Overcrowd->GetPanicOverloadState()), static_cast<uint8>(EPanicOverloadState::Active));

		Overcrowd->ForceEndPanicOverload();
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
