// Adds KrowdKontrol.PIE.DefeatRestartRoundTrip (PRD docs/prd-functional-pie-tests.md
// REQ-3 item 3, issue #240) - drives a real defeat->restart round trip inside the
// KrowdKontrol.PIE.* group (issue #236) that KrowdKontrolPIESessionTest.cpp stood up.
//
// AKrowdKontrolPlayerController::RequestLevelRestart()'s real UGameplayStatics::
// OpenLevel() call only fires when World->IsGameWorld() is true, so the existing
// KrowdKontrol.Unit.LevelRestart / KrowdKontrol.Unit.BossCheckpointRestart tests -
// which run in CreateNewMap() Editor-type worlds specifically to avoid hanging on a
// real map load (see KrowdKontrolLevelRestartTest.cpp's file comment) - never exercise
// the actual reload, including issue #223's PIE map-name-mangling stripping fix. Only
// a real PIE session reproduces the UEDPIE_0_ prefix that caused that original bug.
//
// This test opens L_Level01 in a real PIE session, drives the possessed pawn's
// UPlayerEnergyComponent to 0 through the code-side Cheat_ZeroPlayerEnergy() cheat
// (never simulated input), waits for the resulting hard level-reload
// (HandleLevelFailed -> RequestLevelRestart -> UGameplayStatics::OpenLevel()) to
// complete, then asserts the reloaded world is L_Level01 - not the engine's OpenWorld
// template - with player energy restored to its starting value. This pins issue #223's
// fix in the only environment that bug can reproduce in.
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the other
// KrowdKontrol.* automation tests.

#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "KrowdKontrolPlayerController.h"
#include "PlayerEnergyComponent.h"
#include "Engine/World.h"

#if WITH_DEV_AUTOMATION_TESTS

DEFINE_LATENT_AUTOMATION_COMMAND_TWO_PARAMETER(FKrowdKontrolTriggerDefeatRestartCommand, FAutomationTestBase*, Test, TSharedRef<float>, OutExpectedFullEnergy);

bool FKrowdKontrolTriggerDefeatRestartCommand::Update()
{
	UWorld* PIEWorld = AutomationCommon::GetAnyGameWorld();
	AKrowdKontrolPlayerController* Controller = PIEWorld ? Cast<AKrowdKontrolPlayerController>(PIEWorld->GetFirstPlayerController()) : nullptr;
	APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
	UPlayerEnergyComponent* Energy = Pawn ? Pawn->FindComponentByClass<UPlayerEnergyComponent>() : nullptr;

	if (Test->TestNotNull(TEXT("The possessed pawn should have a UPlayerEnergyComponent before triggering the defeat-restart"), Energy))
	{
		*OutExpectedFullEnergy = Energy->MaxEnergy;
		Test->TestEqual(TEXT("Player energy should start at MaxEnergy before the trigger"), Energy->GetCurrentEnergy(), Energy->MaxEnergy);
		Controller->Cheat_ZeroPlayerEnergy();
	}
	return true;
}

DEFINE_LATENT_AUTOMATION_COMMAND_TWO_PARAMETER(FKrowdKontrolAssertDefeatRestartCompletedCommand, FAutomationTestBase*, Test, TSharedRef<float>, ExpectedFullEnergy);

bool FKrowdKontrolAssertDefeatRestartCompletedCommand::Update()
{
	UWorld* PIEWorld = AutomationCommon::GetAnyGameWorld();
	if (Test->TestNotNull(TEXT("A live PIE world should exist after the defeat-restart reload"), PIEWorld))
	{
		// UWorld::RemovePIEPrefix strips the UEDPIE_0_ mangling issue #223's fix also
		// strips internally (StripPIEPrefixFromMapName) - used here directly since it's
		// public engine API and every member this test needs is already public.
		Test->TestEqual(TEXT("The reloaded map should be L_Level01, not some other map"),
			UWorld::RemovePIEPrefix(PIEWorld->GetMapName()), FString(TEXT("L_Level01")));
		Test->TestFalse(TEXT("The reloaded map should never be the engine's OpenWorld template (issue #223's regression symptom)"),
			PIEWorld->GetMapName().Contains(TEXT("OpenWorld")));

		AKrowdKontrolPlayerController* Controller = Cast<AKrowdKontrolPlayerController>(PIEWorld->GetFirstPlayerController());
		APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
		UPlayerEnergyComponent* Energy = Pawn ? Pawn->FindComponentByClass<UPlayerEnergyComponent>() : nullptr;
		if (Test->TestNotNull(TEXT("The possessed pawn should have a UPlayerEnergyComponent after the restart"), Energy))
		{
			Test->TestEqual(TEXT("Player energy should be restored to its starting value after the restart"),
				Energy->GetCurrentEnergy(), *ExpectedFullEnergy);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolPIEDefeatRestartRoundTripTest,
	"KrowdKontrol.PIE.DefeatRestartRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolPIEDefeatRestartRoundTripTest::RunTest(const FString& Parameters)
{
	TSharedRef<float> ExpectedFullEnergy = MakeShared<float>(0.0f);

	AutomationOpenMap(TEXT("/Game/Maps/L_Level01"));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForEngineFramesCommand(5));
	ADD_LATENT_AUTOMATION_COMMAND(FKrowdKontrolTriggerDefeatRestartCommand(this, ExpectedFullEnergy));
	ADD_LATENT_AUTOMATION_COMMAND(FUntilCommand(
		[this, ExpectedFullEnergy]() -> bool
		{
			// Only a *new* (post-reload) UPlayerEnergyComponent can read >= ExpectedFullEnergy
			// here - the old, about-to-be-destroyed world's component stays at 0 for the rest
			// of its lifetime after Cheat_ZeroPlayerEnergy() runs, so a null-only check would
			// false-positive on stale state before the real reload completes. This works
			// because the component's constructor seeds CurrentEnergy = MaxEnergy
			// immediately, not in BeginPlay. Requiring the full restored value (not just > 0)
			// also means this command's own success condition already proves what
			// FKrowdKontrolAssertDefeatRestartCompletedCommand re-asserts a tick later,
			// closing the one-tick window where an enemy could otherwise chip the freshly
			// restored energy between the two commands.
			UWorld* PIEWorld = AutomationCommon::GetAnyGameWorld();
			AKrowdKontrolPlayerController* Controller = PIEWorld ? Cast<AKrowdKontrolPlayerController>(PIEWorld->GetFirstPlayerController()) : nullptr;
			APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
			UPlayerEnergyComponent* Energy = Pawn ? Pawn->FindComponentByClass<UPlayerEnergyComponent>() : nullptr;
			return Energy && Energy->GetCurrentEnergy() >= *ExpectedFullEnergy;
		},
		[this]() -> bool
		{
			AddError(TEXT("Timed out waiting for the defeat-restart level reload to complete and restore player energy"));
			return true;
		},
		30.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FKrowdKontrolAssertDefeatRestartCompletedCommand(this, ExpectedFullEnergy));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
