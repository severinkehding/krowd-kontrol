// Confirms AKrowdKontrolPlayerController (issue #132) actually constructs and wires
// the project's persistent HUD widgets (ability tray, energy meter) on level start -
// the acceptance bar the issue itself sets. Mirrors the spawn/possess pattern from
// KrowdKontrolFlatCamera3DPipelineSmokeTest.cpp's movement test and the
// AddToViewport() no-crash-only convention from KrowdKontrolEnergyMeterWidgetTest.cpp
// (this project's Automation run is -nullrhi, so there's no live UGameViewportSubsystem
// target - IsInViewport()==true is not assertable here).

#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "KrowdKontrolPlayerController.h"
#include "AbilityCooldownTrayWidget.h"
#include "EnergyMeterWidget.h"
#include "FlatCamera3DPrototypePawn.h"
#include "AbilityUnlockComponent.h"
#include "PlayerEnergyComponent.h"
#include "AbilitySlot.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Engine.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolHUDWiringTest,
	"KrowdKontrol.Unit.HUDWiring",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolHUDWiringTest::RunTest(const FString& Parameters)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
	{
		return false;
	}

	AFlatCamera3DPrototypePawn* Pawn = World->SpawnActor<AFlatCamera3DPrototypePawn>();
	if (!TestNotNull(TEXT("Pawn should spawn"), Pawn))
	{
		return false;
	}

	AKrowdKontrolPlayerController* Controller = World->SpawnActor<AKrowdKontrolPlayerController>();
	if (!TestNotNull(TEXT("Controller should spawn"), Controller))
	{
		return false;
	}
	Controller->Possess(Pawn);
	// SetAsLocalPlayerController() alone only flips bIsLocalPlayerController - it does
	// NOT attach a UPlayer. CreateWidget<T>(APlayerController*, ...) (what
	// CreateHUDWidgets() calls) hard-requires OwnerPC.Player to already be a real
	// ULocalPlayer (UserWidget.cpp's CreateWidgetInstance CastChecked<ULocalPlayer>()s
	// it), so a bare ULocalPlayer is attached here to satisfy that - nothing else in
	// this test needs it to be more than non-null. ULocalPlayer's ClassWithin is
	// UEngine, so it must be constructed with GEngine as its outer, not the default
	// transient package, or NewObject asserts.
	Controller->Player = NewObject<ULocalPlayer>(GEngine);
	Controller->SetAsLocalPlayerController();

	// This harness's editor world never calls BeginPlay() on spawned actors. Calling
	// the virtual BeginPlay() directly is not an option - UE 5.8's AActor::BeginPlay()
	// asserts ActorHasBegunPlay is already in the BeginningPlay state, which only a
	// real DispatchBeginPlay() sets up first; the direct call trips that ensure.
	// AActor::DispatchBeginPlay() is the public, legal route - see
	// KrowdKontrolWaveSpawnerComponentTest.cpp's identical precedent in this module.
	Controller->DispatchBeginPlay();

	TestNotNull(TEXT("BeginPlay should construct the ability tray widget"), ToRawPtr(Controller->AbilityTrayWidget));
	TestNotNull(TEXT("BeginPlay should construct the energy meter widget"), ToRawPtr(Controller->EnergyMeterWidgetInstance));

	// AddToViewport() is a documented no-op under this project's -nullrhi Automation
	// run (no UGameViewportSubsystem target) - assert no-crash only, matching
	// KrowdKontrolEnergyMeterWidgetTest.cpp's established convention, not
	// IsInViewport()==true.
	TestTrue(TEXT("HUD widget construction + AddToViewport should not crash"), true);

	// Possess() already ran (before BeginPlay in this test, matching the controller's
	// own fallback path for that ordering) - the ability tray should reflect the
	// pawn's AbilityUnlockComponent state: only Stun starts unlocked.
	if (TestNotNull(TEXT("Ability tray should be bound"), ToRawPtr(Controller->AbilityTrayWidget)))
	{
		TestFalse(TEXT("Stun should read unlocked (bound to pawn's AbilityUnlockComponent)"),
			Controller->AbilityTrayWidget->IsSlotLocked(EAbilitySlot::Stun));
		TestTrue(TEXT("Sleep should read locked (not yet unlocked this run)"),
			Controller->AbilityTrayWidget->IsSlotLocked(EAbilitySlot::Sleep));
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
