// Confirms UAbilityMatchupSignalComponent (issue #37, PRD 09 REQ-5): every time
// UAbilityCastComponent::OnAbilityCastApplied fires, the component classifies the
// cast as colour-matched or not against the target's real EEnemyType (via
// UEnemyTypeIndicatorComponent) and broadcasts FOnAbilityMatchupSignal exactly once,
// with Stun (colour-neutral) never reporting a match and a target missing
// UEnemyTypeIndicatorComponent degrading safely (no crash, no broadcast).
//
// Each case uses its own FAutomationEditorCommonUtils::CreateNewMap() World, per
// KrowdKontrolAbilityCastComponentTest.cpp's per-scenario World isolation rationale.
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the other
// KrowdKontrol.Unit.* tests.

#include "Misc/AutomationTest.h"
#include "AbilityMatchupSignalComponent.h"
#include "AbilityMatchupSignalTestListener.h"
#include "AbilityCastComponent.h"
#include "AbilityUnlockComponent.h"
#include "AbilityCooldownComponent.h"
#include "EnemyBaseTestActor.h"
#include "SniperEnemy.h"
#include "RunnerEnemy.h"
#include "FlatCamera3DPrototypePawn.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolAbilityMatchupSignalComponentTest,
	"KrowdKontrol.Unit.AbilityMatchupSignalComponent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolAbilityMatchupSignalComponentTest::RunTest(const FString& Parameters)
{
	// (a) Matched case: Sleep's real counter is SN_1PR, and ASniperEnemy carries an
	// EnemyTypeIndicatorComponent set to SN_1PR by construction.
	{
		UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
		if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
		{
			return false;
		}
		APawn* Owner = World->SpawnActor<APawn>();
		if (!TestNotNull(TEXT("APawn should spawn into the test World"), Owner))
		{
			return false;
		}
		UAbilityUnlockComponent* UnlockComponent = NewObject<UAbilityUnlockComponent>(Owner);
		UnlockComponent->RegisterComponent();
		UnlockComponent->NotifyLevelReached(2); // unlocks Sleep
		UAbilityCooldownComponent* CooldownComponent = NewObject<UAbilityCooldownComponent>(Owner);
		CooldownComponent->RegisterComponent();
		UAbilityCastComponent* CastComponent = NewObject<UAbilityCastComponent>(Owner);
		CastComponent->RegisterComponent();
		UAbilityMatchupSignalComponent* MatchupComponent = NewObject<UAbilityMatchupSignalComponent>(Owner);
		MatchupComponent->RegisterComponent();
		CastComponent->OnAbilityCastApplied.AddDynamic(MatchupComponent, &UAbilityMatchupSignalComponent::HandleAbilityCastApplied);

		UAbilityMatchupSignalTestListener* Listener = NewObject<UAbilityMatchupSignalTestListener>();
		MatchupComponent->OnAbilityMatchupSignal.AddDynamic(Listener, &UAbilityMatchupSignalTestListener::HandleAbilityMatchupSignal);

		ASniperEnemy* Sniper = World->SpawnActor<ASniperEnemy>();
		if (!TestNotNull(TEXT("ASniperEnemy should spawn into the test World"), Sniper))
		{
			return false;
		}
		Sniper->TickCheckDetection(FVector::ZeroVector); // Idle -> Alert

		const bool bCastResult = CastComponent->TryCastAbility(EAbilitySlot::Sleep);
		TestTrue(TEXT("TryCastAbility(Sleep) should succeed against an eligible in-range enemy"), bCastResult);
		TestEqual(TEXT("The matchup signal should fire exactly once"), Listener->CallCount, 1);
		TestEqual(TEXT("The broadcast should carry the cast ability"),
			static_cast<uint8>(Listener->LastAbility), static_cast<uint8>(EAbilitySlot::Sleep));
		TestEqual(TEXT("The broadcast should carry the actual target"), Listener->LastTarget.Get(), static_cast<AEnemyBase*>(Sniper));
		TestTrue(TEXT("Sleep against SN-1PR should be reported as colour-matched"), Listener->LastWasColourMatched);
	}

	// (b) Mismatched case: Sleep's real counter is SN_1PR, not RU_NNR - casting Sleep
	// on an ARunnerEnemy (RU_NNR) must report a mismatch.
	{
		UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
		if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
		{
			return false;
		}
		APawn* Owner = World->SpawnActor<APawn>();
		UAbilityUnlockComponent* UnlockComponent = NewObject<UAbilityUnlockComponent>(Owner);
		UnlockComponent->RegisterComponent();
		UnlockComponent->NotifyLevelReached(2); // unlocks Sleep
		UAbilityCooldownComponent* CooldownComponent = NewObject<UAbilityCooldownComponent>(Owner);
		CooldownComponent->RegisterComponent();
		UAbilityCastComponent* CastComponent = NewObject<UAbilityCastComponent>(Owner);
		CastComponent->RegisterComponent();
		UAbilityMatchupSignalComponent* MatchupComponent = NewObject<UAbilityMatchupSignalComponent>(Owner);
		MatchupComponent->RegisterComponent();
		CastComponent->OnAbilityCastApplied.AddDynamic(MatchupComponent, &UAbilityMatchupSignalComponent::HandleAbilityCastApplied);

		UAbilityMatchupSignalTestListener* Listener = NewObject<UAbilityMatchupSignalTestListener>();
		MatchupComponent->OnAbilityMatchupSignal.AddDynamic(Listener, &UAbilityMatchupSignalTestListener::HandleAbilityMatchupSignal);

		ARunnerEnemy* Runner = World->SpawnActor<ARunnerEnemy>();
		if (!TestNotNull(TEXT("ARunnerEnemy should spawn into the test World"), Runner))
		{
			return false;
		}
		Runner->TickCheckDetection(FVector::ZeroVector); // Idle -> Alert

		const bool bCastResult = CastComponent->TryCastAbility(EAbilitySlot::Sleep);
		TestTrue(TEXT("TryCastAbility(Sleep) should succeed against an eligible in-range enemy"), bCastResult);
		TestEqual(TEXT("The matchup signal should fire exactly once"), Listener->CallCount, 1);
		TestFalse(TEXT("Sleep against RU-NNR should be reported as mismatched"), Listener->LastWasColourMatched);
	}

	// (c) Stun-is-never-matched: Stun's CounteredEnemyType defaults to RU_NNR (unset by
	// GetStun()), so the target must be an ARunnerEnemy (RU_NNR) - the one type that
	// WOULD register as a match on Data.CounteredEnemyType == Indicator->EnemyType
	// alone. This makes the assertion actually depend on the bIsColourNeutral
	// short-circuit, not just an accidental type mismatch (SN_1PR was previously used
	// here and would pass whether or not the short-circuit exists).
	{
		UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
		if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
		{
			return false;
		}
		APawn* Owner = World->SpawnActor<APawn>();
		UAbilityUnlockComponent* UnlockComponent = NewObject<UAbilityUnlockComponent>(Owner);
		UnlockComponent->RegisterComponent(); // Stun is unlocked by default
		UAbilityCooldownComponent* CooldownComponent = NewObject<UAbilityCooldownComponent>(Owner);
		CooldownComponent->RegisterComponent();
		UAbilityCastComponent* CastComponent = NewObject<UAbilityCastComponent>(Owner);
		CastComponent->RegisterComponent();
		UAbilityMatchupSignalComponent* MatchupComponent = NewObject<UAbilityMatchupSignalComponent>(Owner);
		MatchupComponent->RegisterComponent();
		CastComponent->OnAbilityCastApplied.AddDynamic(MatchupComponent, &UAbilityMatchupSignalComponent::HandleAbilityCastApplied);

		UAbilityMatchupSignalTestListener* Listener = NewObject<UAbilityMatchupSignalTestListener>();
		MatchupComponent->OnAbilityMatchupSignal.AddDynamic(Listener, &UAbilityMatchupSignalTestListener::HandleAbilityMatchupSignal);

		ARunnerEnemy* Runner = World->SpawnActor<ARunnerEnemy>();
		if (!TestNotNull(TEXT("ARunnerEnemy should spawn into the test World"), Runner))
		{
			return false;
		}
		Runner->TickCheckDetection(FVector::ZeroVector); // Idle -> Alert

		const bool bCastResult = CastComponent->TryCastAbility(EAbilitySlot::Stun);
		TestTrue(TEXT("TryCastAbility(Stun) should succeed against an eligible in-range enemy"), bCastResult);
		TestEqual(TEXT("The matchup signal should fire exactly once"), Listener->CallCount, 1);
		TestFalse(TEXT("Stun must never be reported as colour-matched, even against RU-NNR (its own CounteredEnemyType default)"), Listener->LastWasColourMatched);
	}

	// (d) Missing-indicator defensive case: a bare AEnemyBaseTestActor has no
	// UEnemyTypeIndicatorComponent attached. Calling the handler directly (bypassing
	// TryCastAbility, since the point here is exercising the missing-component branch,
	// not the cast gate) must not crash and must not broadcast.
	{
		UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
		if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
		{
			return false;
		}
		APawn* Owner = World->SpawnActor<APawn>();
		UAbilityMatchupSignalComponent* MatchupComponent = NewObject<UAbilityMatchupSignalComponent>(Owner);
		MatchupComponent->RegisterComponent();

		UAbilityMatchupSignalTestListener* Listener = NewObject<UAbilityMatchupSignalTestListener>();
		MatchupComponent->OnAbilityMatchupSignal.AddDynamic(Listener, &UAbilityMatchupSignalTestListener::HandleAbilityMatchupSignal);

		AEnemyBaseTestActor* Enemy = World->SpawnActor<AEnemyBaseTestActor>();
		if (!TestNotNull(TEXT("AEnemyBaseTestActor should spawn into the test World"), Enemy))
		{
			return false;
		}

		MatchupComponent->HandleAbilityCastApplied(EAbilitySlot::Stun, Enemy);
		TestEqual(TEXT("A target with no UEnemyTypeIndicatorComponent must not produce a broadcast"), Listener->CallCount, 0);
		TestTrue(TEXT("HandleAbilityCastApplied must not crash when no UEnemyTypeIndicatorComponent can be resolved"), true);
	}

	// (e) Real-pawn wiring: AFlatCamera3DPrototypePawn's constructor binds
	// AbilityCastComponent->OnAbilityCastApplied to its own AbilityMatchupSignalComponent
	// via AddDynamic, directly below the other OnAbilityCastApplied subscribers - a
	// copy-paste slip there would compile cleanly and every other case above would
	// still pass, since none of them go through the real pawn.
	{
		UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
		if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
		{
			return false;
		}

		AFlatCamera3DPrototypePawn* WiringPawn = World->SpawnActor<AFlatCamera3DPrototypePawn>();
		if (!TestNotNull(TEXT("AFlatCamera3DPrototypePawn should spawn into the test World"), WiringPawn))
		{
			return false;
		}
		if (!TestNotNull(TEXT("The real pawn's AbilityMatchupSignalComponent should be constructed"),
			ToRawPtr(WiringPawn->AbilityMatchupSignalComponent)))
		{
			return false;
		}

		WiringPawn->AbilityUnlockComponent->NotifyLevelReached(2); // unlocks Sleep

		UAbilityMatchupSignalTestListener* WiringListener = NewObject<UAbilityMatchupSignalTestListener>();
		WiringPawn->AbilityMatchupSignalComponent->OnAbilityMatchupSignal.AddDynamic(
			WiringListener, &UAbilityMatchupSignalTestListener::HandleAbilityMatchupSignal);

		ASniperEnemy* WiringSniper = World->SpawnActor<ASniperEnemy>();
		if (!TestNotNull(TEXT("A real ASniperEnemy should spawn into the test World"), WiringSniper))
		{
			return false;
		}
		WiringSniper->TickCheckDetection(FVector::ZeroVector); // Idle -> Alert

		const bool bWiringCastResult = WiringPawn->AbilityCastComponent->TryCastAbility(EAbilitySlot::Sleep);
		TestTrue(TEXT("TryCastAbility(Sleep) should succeed against an eligible in-range enemy via the real pawn"),
			bWiringCastResult);
		TestEqual(TEXT("The pawn's real constructor-time AddDynamic binding must reach AbilityMatchupSignalComponent"),
			WiringListener->CallCount, 1);
		TestTrue(TEXT("The real-pawn cast should be reported as colour-matched"), WiringListener->LastWasColourMatched);
	}

	// (f) Null-target defensive case: HandleAbilityCastApplied must not crash if ever
	// called with a null TargetEnemy (not reachable via the real
	// UAbilityCastComponent::TryCastAbility broadcast today, which already gates on a
	// non-null Target, but the handler is public specifically so tests can call it
	// directly - same rationale as case (d)).
	{
		UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
		if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
		{
			return false;
		}
		APawn* Owner = World->SpawnActor<APawn>();
		UAbilityMatchupSignalComponent* MatchupComponent = NewObject<UAbilityMatchupSignalComponent>(Owner);
		MatchupComponent->RegisterComponent();

		UAbilityMatchupSignalTestListener* Listener = NewObject<UAbilityMatchupSignalTestListener>();
		MatchupComponent->OnAbilityMatchupSignal.AddDynamic(Listener, &UAbilityMatchupSignalTestListener::HandleAbilityMatchupSignal);

		MatchupComponent->HandleAbilityCastApplied(EAbilitySlot::Sleep, nullptr);
		TestEqual(TEXT("A null TargetEnemy must not produce a broadcast"), Listener->CallCount, 0);
		TestTrue(TEXT("HandleAbilityCastApplied must not crash when TargetEnemy is null"), true);
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
