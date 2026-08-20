// Confirms UAbilityCastComponent::TryCastAbility (issue #138) - the single production
// entry point that finally calls AEnemyBase::ReceiveControl() - gates correctly
// (locked ability, on-cooldown), targets correctly (nearest hot enemy in range, out-
// of-range exclusion, wrong-state exclusion), and only mutates state/starts the
// cooldown/broadcasts once a target is actually confirmed (a whiff never consumes the
// cooldown).
//
// Each case uses its own FAutomationEditorCommonUtils::CreateNewMap() World, since
// FindNearestValidTarget() iterates every AEnemyBase in the component's GetWorld() -
// reusing one World across cases would let an earlier case's enemies leak into a
// later target search, mirroring KrowdKontrolOvercrowdDetectionComponentTest.cpp's
// same per-scenario World isolation.
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the other
// KrowdKontrol.Unit.* tests.

#include "Misc/AutomationTest.h"
#include "AbilityCastComponent.h"
#include "AbilityCastAppliedTestListener.h"
#include "AbilityUnlockComponent.h"
#include "AbilityCooldownComponent.h"
#include "AbilityLockoutComponent.h"
#include "EnemyBaseTestActor.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolAbilityCastComponentTest,
	"KrowdKontrol.Unit.AbilityCastComponent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolAbilityCastComponentTest::RunTest(const FString& Parameters)
{
	// (a) locked ability: default UAbilityUnlockComponent state only unlocks Stun, so
	// casting Sleep must fail and change nothing, even with an eligible target present.
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
		UAbilityCooldownComponent* CooldownComponent = NewObject<UAbilityCooldownComponent>(Owner);
		CooldownComponent->RegisterComponent();
		UAbilityCastComponent* CastComponent = NewObject<UAbilityCastComponent>(Owner);
		CastComponent->RegisterComponent();

		AEnemyBaseTestActor* Enemy = World->SpawnActor<AEnemyBaseTestActor>();
		if (!TestNotNull(TEXT("AEnemyBaseTestActor should spawn into the test World"), Enemy))
		{
			return false;
		}
		Enemy->TickCheckDetection(FVector::ZeroVector); // Idle -> Alert

		const bool bCastResult = CastComponent->TryCastAbility(EAbilitySlot::Sleep);
		TestFalse(TEXT("TryCastAbility for a locked ability should fail"), bCastResult);
		TestEqual(TEXT("A locked-ability cast attempt should not change the enemy's state"),
			static_cast<uint8>(Enemy->GetEnemyState()), static_cast<uint8>(EEnemyState::Alert));
	}

	// (b) unlocked but on cooldown: TryStartCooldown pre-armed before the cast attempt.
	{
		UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
		if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
		{
			return false;
		}
		APawn* Owner = World->SpawnActor<APawn>();
		UAbilityUnlockComponent* UnlockComponent = NewObject<UAbilityUnlockComponent>(Owner);
		UnlockComponent->RegisterComponent();
		UAbilityCooldownComponent* CooldownComponent = NewObject<UAbilityCooldownComponent>(Owner);
		CooldownComponent->RegisterComponent();
		UAbilityCastComponent* CastComponent = NewObject<UAbilityCastComponent>(Owner);
		CastComponent->RegisterComponent();

		AEnemyBaseTestActor* Enemy = World->SpawnActor<AEnemyBaseTestActor>();
		if (!TestNotNull(TEXT("AEnemyBaseTestActor should spawn into the test World"), Enemy))
		{
			return false;
		}
		Enemy->TickCheckDetection(FVector::ZeroVector); // Idle -> Alert

		CooldownComponent->TryStartCooldown(EAbilitySlot::Stun);
		const bool bCastResult = CastComponent->TryCastAbility(EAbilitySlot::Stun);
		TestFalse(TEXT("TryCastAbility while on cooldown should fail"), bCastResult);
		TestEqual(TEXT("An on-cooldown cast attempt should not change the enemy's state"),
			static_cast<uint8>(Enemy->GetEnemyState()), static_cast<uint8>(EEnemyState::Alert));
	}

	// (c) unlocked, off cooldown, no eligible target anywhere in the world: fails, and
	// crucially does NOT consume the cooldown (a whiff must not start it).
	{
		UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
		if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
		{
			return false;
		}
		APawn* Owner = World->SpawnActor<APawn>();
		UAbilityUnlockComponent* UnlockComponent = NewObject<UAbilityUnlockComponent>(Owner);
		UnlockComponent->RegisterComponent();
		UAbilityCooldownComponent* CooldownComponent = NewObject<UAbilityCooldownComponent>(Owner);
		CooldownComponent->RegisterComponent();
		UAbilityCastComponent* CastComponent = NewObject<UAbilityCastComponent>(Owner);
		CastComponent->RegisterComponent();

		const bool bCastResult = CastComponent->TryCastAbility(EAbilitySlot::Stun);
		TestFalse(TEXT("TryCastAbility with no eligible target should fail"), bCastResult);
		TestFalse(TEXT("A whiff (no target) must not consume the cooldown"),
			CooldownComponent->IsOnCooldown(EAbilitySlot::Stun));
	}

	// (d) successful cast: unlocked, off cooldown, one Alert enemy within
	// CastRangeUnits - state changes to Controlled, cooldown starts, the delegate
	// fires exactly once with (Stun, Target).
	{
		UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
		if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
		{
			return false;
		}
		APawn* Owner = World->SpawnActor<APawn>();
		UAbilityUnlockComponent* UnlockComponent = NewObject<UAbilityUnlockComponent>(Owner);
		UnlockComponent->RegisterComponent();
		UAbilityCooldownComponent* CooldownComponent = NewObject<UAbilityCooldownComponent>(Owner);
		CooldownComponent->RegisterComponent();
		UAbilityCastComponent* CastComponent = NewObject<UAbilityCastComponent>(Owner);
		CastComponent->RegisterComponent();

		UAbilityCastAppliedTestListener* Listener = NewObject<UAbilityCastAppliedTestListener>();
		CastComponent->OnAbilityCastApplied.AddDynamic(Listener, &UAbilityCastAppliedTestListener::HandleAbilityCastApplied);

		AEnemyBaseTestActor* Enemy = World->SpawnActor<AEnemyBaseTestActor>();
		if (!TestNotNull(TEXT("AEnemyBaseTestActor should spawn into the test World"), Enemy))
		{
			return false;
		}
		Enemy->TickCheckDetection(FVector::ZeroVector); // Idle -> Alert

		const bool bCastResult = CastComponent->TryCastAbility(EAbilitySlot::Stun);
		TestTrue(TEXT("TryCastAbility should succeed with an eligible target in range"), bCastResult);
		TestEqual(TEXT("The target should be Controlled after a successful cast"),
			static_cast<uint8>(Enemy->GetEnemyState()), static_cast<uint8>(EEnemyState::Controlled));
		TestTrue(TEXT("A successful cast should start the cooldown"),
			CooldownComponent->IsOnCooldown(EAbilitySlot::Stun));
		TestEqual(TEXT("OnAbilityCastApplied should have fired exactly once"), Listener->CallCount, 1);
		TestEqual(TEXT("The broadcast should carry the cast ability"),
			static_cast<uint8>(Listener->LastAbility), static_cast<uint8>(EAbilitySlot::Stun));
		TestEqual(TEXT("The broadcast should carry the actual target"), Listener->LastTarget.Get(), static_cast<AEnemyBase*>(Enemy));
	}

	// (e) nearest-of-two: two eligible enemies at different distances - the nearer one
	// is chosen, the farther one is left untouched.
	{
		UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
		if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
		{
			return false;
		}
		APawn* Owner = World->SpawnActor<APawn>();
		UAbilityUnlockComponent* UnlockComponent = NewObject<UAbilityUnlockComponent>(Owner);
		UnlockComponent->RegisterComponent();
		UAbilityCooldownComponent* CooldownComponent = NewObject<UAbilityCooldownComponent>(Owner);
		CooldownComponent->RegisterComponent();
		UAbilityCastComponent* CastComponent = NewObject<UAbilityCastComponent>(Owner);
		CastComponent->RegisterComponent();

		AEnemyBaseTestActor* NearEnemy = World->SpawnActor<AEnemyBaseTestActor>();
		AEnemyBaseTestActor* FarEnemy = World->SpawnActor<AEnemyBaseTestActor>();
		if (!TestNotNull(TEXT("Near AEnemyBaseTestActor should spawn"), NearEnemy)
			|| !TestNotNull(TEXT("Far AEnemyBaseTestActor should spawn"), FarEnemy))
		{
			return false;
		}
		NearEnemy->SetActorLocation(FVector(100.0f, 0.0f, 0.0f));
		FarEnemy->SetActorLocation(FVector(500.0f, 0.0f, 0.0f));
		NearEnemy->TickCheckDetection(NearEnemy->GetActorLocation()); // Idle -> Alert
		FarEnemy->TickCheckDetection(FarEnemy->GetActorLocation()); // Idle -> Alert

		const bool bCastResult = CastComponent->TryCastAbility(EAbilitySlot::Stun);
		TestTrue(TEXT("TryCastAbility should succeed when a nearer eligible target exists"), bCastResult);
		TestEqual(TEXT("The nearer enemy should be the one Controlled"),
			static_cast<uint8>(NearEnemy->GetEnemyState()), static_cast<uint8>(EEnemyState::Controlled));
		TestEqual(TEXT("The farther enemy should be left untouched"),
			static_cast<uint8>(FarEnemy->GetEnemyState()), static_cast<uint8>(EEnemyState::Alert));
	}

	// (f) out-of-range exclusion: an enemy beyond CastRangeUnits is ignored even
	// though it is otherwise eligible; a farther-but-still-in-range enemy is chosen
	// instead.
	{
		UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
		if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
		{
			return false;
		}
		APawn* Owner = World->SpawnActor<APawn>();
		UAbilityUnlockComponent* UnlockComponent = NewObject<UAbilityUnlockComponent>(Owner);
		UnlockComponent->RegisterComponent();
		UAbilityCooldownComponent* CooldownComponent = NewObject<UAbilityCooldownComponent>(Owner);
		CooldownComponent->RegisterComponent();
		UAbilityCastComponent* CastComponent = NewObject<UAbilityCastComponent>(Owner);
		CastComponent->RegisterComponent();

		AEnemyBaseTestActor* InRangeEnemy = World->SpawnActor<AEnemyBaseTestActor>();
		AEnemyBaseTestActor* OutOfRangeEnemy = World->SpawnActor<AEnemyBaseTestActor>();
		if (!TestNotNull(TEXT("In-range AEnemyBaseTestActor should spawn"), InRangeEnemy)
			|| !TestNotNull(TEXT("Out-of-range AEnemyBaseTestActor should spawn"), OutOfRangeEnemy))
		{
			return false;
		}
		// Drive both to Alert via a zero-distance detection check (matching this file's
		// other cases) BEFORE relocating OutOfRangeEnemy - TickCheckDetection measures
		// distance from the enemy's actual GetActorLocation(), so if the relocation
		// happened first, OutOfRangeEnemy would never even reach Alert and this case
		// would accidentally prove nothing about FindNearestValidTarget's own range
		// check specifically.
		InRangeEnemy->TickCheckDetection(FVector::ZeroVector); // Idle -> Alert
		OutOfRangeEnemy->TickCheckDetection(FVector::ZeroVector); // Idle -> Alert
		InRangeEnemy->SetActorLocation(FVector(200.0f, 0.0f, 0.0f));
		OutOfRangeEnemy->SetActorLocation(FVector(CastComponent->CastRangeUnits * 10.0f, 0.0f, 0.0f));

		const bool bCastResult = CastComponent->TryCastAbility(EAbilitySlot::Stun);
		TestTrue(TEXT("TryCastAbility should succeed via the in-range enemy"), bCastResult);
		TestEqual(TEXT("The in-range enemy should be Controlled"),
			static_cast<uint8>(InRangeEnemy->GetEnemyState()), static_cast<uint8>(EEnemyState::Controlled));
		TestEqual(TEXT("The out-of-range enemy should be left untouched"),
			static_cast<uint8>(OutOfRangeEnemy->GetEnemyState()), static_cast<uint8>(EEnemyState::Alert));
	}

	// (g) wrong-state exclusion: Idle/Controlled/Banked enemies are ignored even when
	// closer than the one valid Alert enemy.
	{
		UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
		if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
		{
			return false;
		}
		APawn* Owner = World->SpawnActor<APawn>();
		UAbilityUnlockComponent* UnlockComponent = NewObject<UAbilityUnlockComponent>(Owner);
		UnlockComponent->RegisterComponent();
		UAbilityCooldownComponent* CooldownComponent = NewObject<UAbilityCooldownComponent>(Owner);
		CooldownComponent->RegisterComponent();
		UAbilityCastComponent* CastComponent = NewObject<UAbilityCastComponent>(Owner);
		CastComponent->RegisterComponent();

		AEnemyBaseTestActor* IdleEnemy = World->SpawnActor<AEnemyBaseTestActor>();
		AEnemyBaseTestActor* ControlledEnemy = World->SpawnActor<AEnemyBaseTestActor>();
		AEnemyBaseTestActor* BankedEnemy = World->SpawnActor<AEnemyBaseTestActor>();
		AEnemyBaseTestActor* ValidEnemy = World->SpawnActor<AEnemyBaseTestActor>();
		if (!TestNotNull(TEXT("Idle AEnemyBaseTestActor should spawn"), IdleEnemy)
			|| !TestNotNull(TEXT("Controlled AEnemyBaseTestActor should spawn"), ControlledEnemy)
			|| !TestNotNull(TEXT("Banked AEnemyBaseTestActor should spawn"), BankedEnemy)
			|| !TestNotNull(TEXT("Valid AEnemyBaseTestActor should spawn"), ValidEnemy))
		{
			return false;
		}

		// All three excluded enemies sit at the origin (distance 0) - closer than
		// ValidEnemy - so a targeting bug that ignores state entirely would pick one
		// of these instead.
		ControlledEnemy->TickCheckDetection(FVector::ZeroVector); // Idle -> Alert
		ControlledEnemy->ReceiveControl(EAbilitySlot::Stun); // Alert -> Controlled

		BankedEnemy->TickCheckDetection(FVector::ZeroVector); // Idle -> Alert
		BankedEnemy->ReceiveControl(EAbilitySlot::Stun); // Alert -> Controlled
		BankedEnemy->TransitionToBanked(); // Controlled -> Banked

		// IdleEnemy is left at its default Idle state deliberately.

		ValidEnemy->SetActorLocation(FVector(300.0f, 0.0f, 0.0f));
		ValidEnemy->TickCheckDetection(ValidEnemy->GetActorLocation()); // Idle -> Alert

		const bool bCastResult = CastComponent->TryCastAbility(EAbilitySlot::Stun);
		TestTrue(TEXT("TryCastAbility should succeed via the only valid (Alert) enemy"), bCastResult);
		TestEqual(TEXT("The valid Alert enemy should be Controlled"),
			static_cast<uint8>(ValidEnemy->GetEnemyState()), static_cast<uint8>(EEnemyState::Controlled));
		TestEqual(TEXT("The Idle enemy must remain untouched"),
			static_cast<uint8>(IdleEnemy->GetEnemyState()), static_cast<uint8>(EEnemyState::Idle));
		TestEqual(TEXT("The already-Controlled enemy must remain untouched"),
			static_cast<uint8>(ControlledEnemy->GetEnemyState()), static_cast<uint8>(EEnemyState::Controlled));
		TestEqual(TEXT("The Banked enemy must remain untouched"),
			static_cast<uint8>(BankedEnemy->GetEnemyState()), static_cast<uint8>(EEnemyState::Banked));
	}

	// (h) locked ability (issue #178 Punishment 1): a UAbilityLockoutComponent present
	// and reporting Stun locked must block TryCastAbility exactly like an on-cooldown
	// attempt, even with an eligible target present, and change nothing.
	{
		UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
		if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
		{
			return false;
		}
		APawn* Owner = World->SpawnActor<APawn>();
		UAbilityUnlockComponent* UnlockComponent = NewObject<UAbilityUnlockComponent>(Owner);
		UnlockComponent->RegisterComponent();
		UAbilityCooldownComponent* CooldownComponent = NewObject<UAbilityCooldownComponent>(Owner);
		CooldownComponent->RegisterComponent();
		UAbilityLockoutComponent* LockoutComponent = NewObject<UAbilityLockoutComponent>(Owner);
		LockoutComponent->RegisterComponent();
		UAbilityCastComponent* CastComponent = NewObject<UAbilityCastComponent>(Owner);
		CastComponent->RegisterComponent();

		AEnemyBaseTestActor* Enemy = World->SpawnActor<AEnemyBaseTestActor>();
		if (!TestNotNull(TEXT("AEnemyBaseTestActor should spawn into the test World"), Enemy))
		{
			return false;
		}
		Enemy->TickCheckDetection(FVector::ZeroVector); // Idle -> Alert

		// Locks Stun directly - this tests TryCastAbility's gate, not the lockout
		// component's own trigger logic (already covered by
		// KrowdKontrolAbilityLockoutComponentTest.cpp).
		LockoutComponent->HandleAbilityCastApplied(EAbilitySlot::Stun, nullptr);
		LockoutComponent->HandlePunishmentTriggered();

		const bool bCastResult = CastComponent->TryCastAbility(EAbilitySlot::Stun);
		TestFalse(TEXT("TryCastAbility for a locked-out ability should fail"), bCastResult);
		TestEqual(TEXT("A locked-out cast attempt should not change the enemy's state"),
			static_cast<uint8>(Enemy->GetEnemyState()), static_cast<uint8>(EEnemyState::Alert));
	}

	// (i) missing UAbilityLockoutComponent: TryCastAbility must succeed normally - the
	// lockout gate is optional, unlike Unlock/Cooldown. Proves the gate's
	// optionality doesn't regress the existing cases above, none of which construct a
	// UAbilityLockoutComponent at all.
	{
		UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
		if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
		{
			return false;
		}
		APawn* Owner = World->SpawnActor<APawn>();
		UAbilityUnlockComponent* UnlockComponent = NewObject<UAbilityUnlockComponent>(Owner);
		UnlockComponent->RegisterComponent();
		UAbilityCooldownComponent* CooldownComponent = NewObject<UAbilityCooldownComponent>(Owner);
		CooldownComponent->RegisterComponent();
		UAbilityCastComponent* CastComponent = NewObject<UAbilityCastComponent>(Owner);
		CastComponent->RegisterComponent();

		AEnemyBaseTestActor* Enemy = World->SpawnActor<AEnemyBaseTestActor>();
		if (!TestNotNull(TEXT("AEnemyBaseTestActor should spawn into the test World"), Enemy))
		{
			return false;
		}
		Enemy->TickCheckDetection(FVector::ZeroVector); // Idle -> Alert

		const bool bCastResult = CastComponent->TryCastAbility(EAbilitySlot::Stun);
		TestTrue(TEXT("TryCastAbility should succeed with no UAbilityLockoutComponent present"), bCastResult);
		TestEqual(TEXT("The target should be Controlled after a successful cast"),
			static_cast<uint8>(Enemy->GetEnemyState()), static_cast<uint8>(EEnemyState::Controlled));
	}

	// (j) A different, unlocked ability must still be castable while another slot is
	// locked - proves the gate is keyed to the requested Ability, not a global block.
	{
		UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
		if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
		{
			return false;
		}
		APawn* Owner = World->SpawnActor<APawn>();
		UAbilityUnlockComponent* UnlockComponent = NewObject<UAbilityUnlockComponent>(Owner);
		UnlockComponent->RegisterComponent();
		UnlockComponent->NotifyLevelReached(3); // unlocks Root - only Stun is unlocked by default
		UAbilityCooldownComponent* CooldownComponent = NewObject<UAbilityCooldownComponent>(Owner);
		CooldownComponent->RegisterComponent();
		UAbilityLockoutComponent* LockoutComponent = NewObject<UAbilityLockoutComponent>(Owner);
		LockoutComponent->RegisterComponent();
		UAbilityCastComponent* CastComponent = NewObject<UAbilityCastComponent>(Owner);
		CastComponent->RegisterComponent();

		AEnemyBaseTestActor* Enemy = World->SpawnActor<AEnemyBaseTestActor>();
		if (!TestNotNull(TEXT("AEnemyBaseTestActor should spawn into the test World"), Enemy))
		{
			return false;
		}
		Enemy->TickCheckDetection(FVector::ZeroVector); // Idle -> Alert

		LockoutComponent->HandleAbilityCastApplied(EAbilitySlot::Stun, nullptr);
		LockoutComponent->HandlePunishmentTriggered(); // locks Stun only

		const bool bCastResult = CastComponent->TryCastAbility(EAbilitySlot::Root);
		TestTrue(TEXT("Casting an unlocked ability should succeed while a different ability is locked"), bCastResult);
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
