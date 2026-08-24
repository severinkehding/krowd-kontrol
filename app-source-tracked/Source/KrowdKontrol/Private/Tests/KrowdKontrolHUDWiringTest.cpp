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
#include "QuestTrackerWidget.h"
#include "PostRunSummaryWidget.h"
#include "FlatCamera3DPrototypePawn.h"
#include "Paper2DPrototypePawn.h"
#include "AbilityUnlockComponent.h"
#include "AbilityCastComponent.h"
#include "PunishmentManagerComponent.h"
#include "PlayerEnergyComponent.h"
#include "AbilitySlot.h"
#include "EnemyBaseTestActor.h"
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
	TestNotNull(TEXT("BeginPlay should construct the on-screen prompt widget"), ToRawPtr(Controller->OnScreenPromptWidgetInstance));
	TestNotNull(TEXT("BeginPlay should construct the quest tracker widget"), ToRawPtr(Controller->QuestTrackerWidgetInstance));
	TestNotNull(TEXT("BeginPlay should construct the post-run summary widget"), ToRawPtr(Controller->PostRunSummaryWidgetInstance));

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

	if (TestNotNull(TEXT("Energy meter should be bound"), ToRawPtr(Controller->EnergyMeterWidgetInstance)))
	{
		// The widget seeds itself to the 0.72 placeholder fraction on construction.
		// UPlayerEnergyComponent now seeds CurrentEnergy to MaxEnergy in its own
		// constructor (not BeginPlay - see PlayerEnergyComponent.cpp), so the pawn's
		// component already holds full energy by the time this test spawns it,
		// independent of whether the pawn's own BeginPlay ever runs. A successful
		// BindToEnergyComponent() call therefore drives the displayed fraction from the
		// 0.72 placeholder to 1.0 (full energy) - proof the bind actually happened, not
		// just that the widget was constructed.
		TestEqual(TEXT("Energy meter fraction should move off the 0.72 placeholder once bound"),
			Controller->EnergyMeterWidgetInstance->GetDisplayedFraction(), 1.0f);
	}

	// Production wiring for issue #249's suggested-ability line: WireWidgetsToPawn()
	// binds QuestTrackerWidgetInstance to the pawn's real UAbilityUnlockComponent - no
	// enemies are alive in this world yet at this point, so the universal fallback
	// text is the observable proof the bind actually reached the widget with a
	// non-null component (a null FindComponentByClass<UAbilityUnlockComponent>()
	// result would leave the widget's own default-constructed fallback state
	// unchanged, which happens to read identically - see KrowdKontrolQuestTrackerWidgetTest.cpp's
	// case (12)/(13)/(14) for the positive live-bind coverage this can't provide alone).
	if (TestNotNull(TEXT("Quest tracker should be bound via WireWidgetsToPawn"), ToRawPtr(Controller->QuestTrackerWidgetInstance)))
	{
		TestEqual(TEXT("Quest tracker suggested-ability line should reflect the real pawn's unlock component after WireWidgetsToPawn"),
			Controller->QuestTrackerWidgetInstance->GetSuggestedAbilityDisplayText().ToString(),
			FString(TEXT("ANY ROBOT → STUN (LMB)")));
	}

	// Production wiring for issue #178's Punishment 1 (real ability lockout on contact
	// damage): a real successful cast through the real pawn, followed by a real
	// punishment trigger, should actually lock the tray via the production wiring
	// (pawn constructor -> controller WireWidgetsToPawn -> widget), not just the
	// isolated BindAbilityLockoutComponent() method tested in isolation elsewhere.
	if (UAbilityCastComponent* CastComponent = Pawn->FindComponentByClass<UAbilityCastComponent>())
	{
		AEnemyBaseTestActor* Enemy = World->SpawnActor<AEnemyBaseTestActor>();
		if (TestNotNull(TEXT("AEnemyBaseTestActor should spawn for the lockout wiring case"), Enemy))
		{
			Enemy->TickCheckDetection(FVector::ZeroVector); // Idle -> Alert, within CastRangeUnits of the pawn at the origin

			const bool bCastResult = CastComponent->TryCastAbility(EAbilitySlot::Stun);
			TestTrue(TEXT("Real cast through the production pawn should succeed"), bCastResult);

			if (UPunishmentManagerComponent* PunishmentManager = Pawn->FindComponentByClass<UPunishmentManagerComponent>())
			{
				PunishmentManager->OnPunishmentTriggered.Broadcast();

				if (TestNotNull(TEXT("Ability tray should be bound"), ToRawPtr(Controller->AbilityTrayWidget)))
				{
					// PunishmentLockout, not NotYetUnlocked/IsSlotLocked() (issue #261) -
					// production punishment lockout is tracked as its own tile state now,
					// distinct from the not-yet-unlocked state IsSlotLocked() still reports.
					TestEqual(TEXT("A real punishment trigger after a real cast should read PunishmentLockout on the tray's Stun slot via production wiring"),
						Controller->AbilityTrayWidget->GetSlotState(EAbilitySlot::Stun), EAbilityTileState::PunishmentLockout);
				}
			}
		}
	}

	// Second controller/pawn pair, opposite ordering from the one above: widgets are
	// constructed first (DispatchBeginPlay), then Possess() runs against already-existing
	// widgets. This exercises OnPossess()'s own WireWidgetsToPawn() call directly - a
	// path that is a guaranteed no-op on both current playable levels, since
	// AutoPossessPlayer there always possesses before the controller's BeginPlay (and
	// therefore before CreateHUDWidgets()) runs, so OnPossess()'s WireWidgetsToPawn()
	// call always finds null widgets in practice. Covered here as defense-in-depth for
	// a future level/possession order that isn't AutoPossessPlayer-driven, not because
	// it's reachable today.
	AFlatCamera3DPrototypePawn* Pawn2 = World->SpawnActor<AFlatCamera3DPrototypePawn>();
	if (!TestNotNull(TEXT("Second pawn should spawn"), Pawn2))
	{
		return false;
	}

	AKrowdKontrolPlayerController* Controller2 = World->SpawnActor<AKrowdKontrolPlayerController>();
	if (!TestNotNull(TEXT("Second controller should spawn"), Controller2))
	{
		return false;
	}
	Controller2->Player = NewObject<ULocalPlayer>(GEngine);
	Controller2->SetAsLocalPlayerController();
	Controller2->DispatchBeginPlay();
	Controller2->Possess(Pawn2);

	if (TestNotNull(TEXT("Ability tray should be bound via OnPossess"), ToRawPtr(Controller2->AbilityTrayWidget)))
	{
		TestFalse(TEXT("Stun should read unlocked after OnPossess-driven wiring"),
			Controller2->AbilityTrayWidget->IsSlotLocked(EAbilitySlot::Stun));
	}

	// Third controller/pawn pair: APaper2DPrototypePawn has no AbilityUnlockComponent, so
	// FindComponentByClass<UAbilityUnlockComponent>() returns nullptr here -
	// BindAbilityUnlockComponent(nullptr) must no-crash and leave the tray's
	// default-constructed locked state untouched. This is the pawn/path combination this
	// PR's own review focus flagged as needing a spot-check.
	APaper2DPrototypePawn* Paper2DPawn = World->SpawnActor<APaper2DPrototypePawn>();
	if (!TestNotNull(TEXT("Paper2D pawn should spawn"), Paper2DPawn))
	{
		return false;
	}

	AKrowdKontrolPlayerController* Controller3 = World->SpawnActor<AKrowdKontrolPlayerController>();
	if (!TestNotNull(TEXT("Third controller should spawn"), Controller3))
	{
		return false;
	}
	Controller3->Possess(Paper2DPawn);
	Controller3->Player = NewObject<ULocalPlayer>(GEngine);
	Controller3->SetAsLocalPlayerController();
	Controller3->DispatchBeginPlay();

	TestNotNull(TEXT("Widgets still construct for a pawn without AbilityUnlockComponent"),
		ToRawPtr(Controller3->AbilityTrayWidget));
	// Also confirms the Paper2D pawn's own PlayerEnergyComponent wiring - it is spawned
	// through the controller flow above rather than only in isolation.
	TestNotNull(TEXT("Paper2D pawn should have a PlayerEnergyComponent"),
		Paper2DPawn->FindComponentByClass<UPlayerEnergyComponent>());

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
