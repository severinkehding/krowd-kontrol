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
#include "Herdable.h"
#include "KrowdKontrolPlayerController.h"
#include "BriefingCardWidget.h"
#include "LevelBriefingData.h"
#include "TargetZone.h"
#include "RunnerEnemy.h"
#include "RoomActor.h"
#include "EnemyType.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Engine.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/WorldSettings.h"

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

	// (k) world-paused gate (issue #246): the pre-level briefing card pauses the
	// world while shown, and TryCastAbility must fail while World->IsPaused() is
	// true - even with an eligible target present - so no stray ability cast can
	// land while the briefing is up.
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

		// UGameplayStatics::SetGamePaused()/APlayerController::SetPause() both require
		// a live AGameModeBase (World->GetAuthGameMode()), which CreateNewMap() test
		// Worlds never spawn - going through either would silently no-op here. Setting
		// AWorldSettings::PauserPlayerState directly is what actually latches
		// World->IsPaused() (see UWorld::IsPaused()'s own check), bypassing the
		// GameMode requirement entirely - this test only needs IsPaused() to read
		// true, not a full real-gameplay pause flow.
		APlayerState* PauserPlayerState = NewObject<APlayerState>(Owner);
		World->GetWorldSettings()->SetPauserPlayerState(PauserPlayerState);
		if (!TestTrue(TEXT("World should report paused after SetPauserPlayerState()"), World->IsPaused()))
		{
			return false;
		}

		const bool bCastResult = CastComponent->TryCastAbility(EAbilitySlot::Stun);
		TestFalse(TEXT("TryCastAbility should fail while the world is paused"), bCastResult);
		TestEqual(TEXT("A paused-world cast attempt should not change the enemy's state"),
			static_cast<uint8>(Enemy->GetEnemyState()), static_cast<uint8>(EEnemyState::Alert));
	}

	// (l) briefing-visible gate, independent of World->IsPaused() (issue #246 PR
	// review, MEDIUM finding 1): the briefing's EKeys::AnyKey dismiss-bind lives on
	// the controller's InputComponent, this cast bind lives on the pawn's, so whether
	// World->IsPaused() still reads true for a same-keypress event depends on UE5's
	// per-frame input-stack processing order between the two - unpinned by this
	// codebase. TryCastAbility must fail while the owning pawn's controller reports a
	// visible briefing card, even when World->IsPaused() itself reads false - exactly
	// what CreateNewMap() test Worlds always report, since SetGamePaused() silently
	// no-ops without a live AGameModeBase (same note as case (k)) - and must succeed
	// again once the briefing is dismissed.
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

		// Mirrors KrowdKontrolLevelBriefingSubsystemTest.cpp's SpawnPossessedController()
		// shape - the local-player setup is what CreateWidget<T>(Controller, Class)
		// requires inside CreateHUDWidgets(), and World->AddController() is what
		// CreateNewMap() worlds otherwise skip (no PostInitializeComponents).
		AKrowdKontrolPlayerController* Controller = World->SpawnActor<AKrowdKontrolPlayerController>();
		if (!TestNotNull(TEXT("Controller should spawn"), Controller))
		{
			return false;
		}
		Controller->Player = NewObject<ULocalPlayer>(GEngine);
		Controller->SetAsLocalPlayerController();
		Controller->Possess(Owner);
		World->AddController(Controller);
		Controller->DispatchBeginPlay();
		if (!TestNotNull(TEXT("BriefingCardWidgetInstance should exist"), ToRawPtr(Controller->BriefingCardWidgetInstance)))
		{
			return false;
		}

		FLevelBriefingRow Row;
		Row.LevelDisplayName = FText::FromString(TEXT("LEVEL 1"));
		Controller->BriefingCardWidgetInstance->ShowBriefing(Row);
		if (!TestFalse(TEXT("World->IsPaused() stays false in CreateNewMap() worlds - see case (k)'s note"), World->IsPaused()))
		{
			return false;
		}

		const bool bCastResultWhileVisible = CastComponent->TryCastAbility(EAbilitySlot::Stun);
		TestFalse(TEXT("TryCastAbility should fail while the briefing card is visible, even though World->IsPaused() reads false"),
			bCastResultWhileVisible);
		TestEqual(TEXT("A briefing-visible cast attempt should not change the enemy's state"),
			static_cast<uint8>(Enemy->GetEnemyState()), static_cast<uint8>(EEnemyState::Alert));

		Controller->BriefingCardWidgetInstance->DismissBriefing();
		const bool bCastResultAfterDismiss = CastComponent->TryCastAbility(EAbilitySlot::Stun);
		TestTrue(TEXT("TryCastAbility should succeed again once the briefing card is dismissed"), bCastResultAfterDismiss);
	}

	// (m) TryCastThrownAbilityAtLocation (issue #257): an Alert enemy inside the
	// landing circle is Controlled by Sleep; an Alert enemy outside the circle (same
	// throw, same clamped landing point) is untouched; an Alert enemy at exactly
	// ThrownCircleLandingRadiusUnits from the landing point is also Controlled, since
	// the radius check (`DistSquared > RadiusSquared`) is inclusive of the boundary
	// itself (PR #280 review, MEDIUM finding 1 - a future `>` -> `>=` slip would
	// silently exclude edge-of-circle enemies with no other test catching it).
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

		AEnemyBaseTestActor* InCircleEnemy = World->SpawnActor<AEnemyBaseTestActor>();
		AEnemyBaseTestActor* OutOfCircleEnemy = World->SpawnActor<AEnemyBaseTestActor>();
		AEnemyBaseTestActor* OnBoundaryEnemy = World->SpawnActor<AEnemyBaseTestActor>();
		if (!TestNotNull(TEXT("In-circle AEnemyBaseTestActor should spawn"), InCircleEnemy)
			|| !TestNotNull(TEXT("Out-of-circle AEnemyBaseTestActor should spawn"), OutOfCircleEnemy)
			|| !TestNotNull(TEXT("On-boundary AEnemyBaseTestActor should spawn"), OnBoundaryEnemy))
		{
			return false;
		}
		InCircleEnemy->TickCheckDetection(FVector::ZeroVector); // Idle -> Alert
		OutOfCircleEnemy->TickCheckDetection(FVector::ZeroVector); // Idle -> Alert
		OnBoundaryEnemy->TickCheckDetection(FVector::ZeroVector); // Idle -> Alert
		const FVector DesiredTargetLocation(500.0f, 0.0f, 0.0f);
		InCircleEnemy->SetActorLocation(DesiredTargetLocation + FVector(100.0f, 0.0f, 0.0f)); // 100 units from landing point, inside the 400-unit radius
		OutOfCircleEnemy->SetActorLocation(DesiredTargetLocation + FVector(700.0f, 0.0f, 0.0f)); // 700 units away, outside the 400-unit radius
		OnBoundaryEnemy->SetActorLocation(DesiredTargetLocation + FVector(CastComponent->ThrownCircleLandingRadiusUnits, 0.0f, 0.0f)); // exactly at the radius

		const int32 AffectedCount = CastComponent->TryCastThrownAbilityAtLocation(EAbilitySlot::Sleep, DesiredTargetLocation);
		TestEqual(TEXT("The in-circle and on-boundary enemies should be affected"), AffectedCount, 2);
		TestEqual(TEXT("The in-circle enemy should be Controlled"),
			static_cast<uint8>(InCircleEnemy->GetEnemyState()), static_cast<uint8>(EEnemyState::Controlled));
		TestEqual(TEXT("The out-of-circle enemy should be left untouched"),
			static_cast<uint8>(OutOfCircleEnemy->GetEnemyState()), static_cast<uint8>(EEnemyState::Alert));
		TestEqual(TEXT("An enemy exactly at the radius boundary should be Controlled (inclusive boundary)"),
			static_cast<uint8>(OnBoundaryEnemy->GetEnemyState()), static_cast<uint8>(EEnemyState::Controlled));
	}

	// (n) Range-tier clamp: a desired target location beyond Sleep's Long throw range
	// clamps to exactly LongThrowRangeUnits from the caster - an enemy spawned exactly
	// at the clamped point (not the raw desired point) is Controlled, proving the
	// clamp itself, not just the AoE math, is exercised.
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

		// Owner defaults to the World origin - the clamped landing point for a desired
		// location straight down +X beyond LongThrowRangeUnits is exactly
		// (LongThrowRangeUnits, 0, 0).
		const FVector ClampedLandingPoint(CastComponent->LongThrowRangeUnits, 0.0f, 0.0f);
		const FVector DesiredTargetLocation = ClampedLandingPoint * 10.0f; // well beyond the Long tier's range

		AEnemyBaseTestActor* Enemy = World->SpawnActor<AEnemyBaseTestActor>();
		if (!TestNotNull(TEXT("AEnemyBaseTestActor should spawn into the test World"), Enemy))
		{
			return false;
		}
		Enemy->SetActorLocation(ClampedLandingPoint);
		Enemy->TickCheckDetection(ClampedLandingPoint); // Idle -> Alert

		const int32 AffectedCount = CastComponent->TryCastThrownAbilityAtLocation(EAbilitySlot::Sleep, DesiredTargetLocation);
		TestEqual(TEXT("The enemy at the clamped landing point should be affected"), AffectedCount, 1);
		TestEqual(TEXT("The enemy at the clamped landing point should be Controlled"),
			static_cast<uint8>(Enemy->GetEnemyState()), static_cast<uint8>(EEnemyState::Controlled));
	}

	// (n2) ComputeClampedThrowLocation exact-boundary case (PR #280 review, MEDIUM
	// finding 2): a desired location exactly at MaxRangeUnits must be returned
	// unclamped (the branch is `Delta.SizeSquared() <= FMath::Square(MaxRangeUnits)`,
	// inclusive), and a location just beyond it must clamp to exactly MaxRangeUnits.
	// Needs no UWorld - ComputeClampedThrowLocation is a static pure function, same
	// no-World shape as IntersectRayWithGroundPlane
	// (KrowdKontrolCursorWorldPositionTest.cpp Case A).
	{
		const FVector OwnerLocation = FVector::ZeroVector;
		const float MaxRangeUnits = 2000.0f;

		const FVector AtBoundary(2000.0f, 0.0f, 0.0f);
		const FVector ClampedAtBoundary = UAbilityCastComponent::ComputeClampedThrowLocation(OwnerLocation, AtBoundary, MaxRangeUnits);
		TestTrue(TEXT("A desired location exactly at MaxRangeUnits should be returned unclamped"),
			ClampedAtBoundary.Equals(AtBoundary, 0.5f));

		const FVector BeyondBoundary(3000.0f, 0.0f, 0.0f);
		const FVector ClampedBeyondBoundary = UAbilityCastComponent::ComputeClampedThrowLocation(OwnerLocation, BeyondBoundary, MaxRangeUnits);
		TestTrue(TEXT("A desired location beyond MaxRangeUnits should clamp to exactly MaxRangeUnits"),
			ClampedBeyondBoundary.Equals(FVector(2000.0f, 0.0f, 0.0f), 0.5f));
	}

	// (m-stun) TryCastThrownAbilityAtLocation via Stun (issue #256): proves the AoE
	// circle math (ability-agnostic, already exercised via Sleep in case (m) above)
	// also holds for Stun - an Alert enemy inside the landing circle is Controlled,
	// one outside is untouched. The exactly-at-radius boundary case is intentionally
	// not re-cloned here (radius math is shared and already covered by case (m)'s
	// OnBoundaryEnemy assertion); this case exists to prove Stun routes into the
	// same AoE sweep, not to re-prove the radius math.
	{
		UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
		if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
		{
			return false;
		}
		APawn* Owner = World->SpawnActor<APawn>();
		UAbilityUnlockComponent* UnlockComponent = NewObject<UAbilityUnlockComponent>(Owner);
		UnlockComponent->RegisterComponent(); // Stun is unlocked by default - no NotifyLevelReached() needed
		UAbilityCooldownComponent* CooldownComponent = NewObject<UAbilityCooldownComponent>(Owner);
		CooldownComponent->RegisterComponent();
		UAbilityCastComponent* CastComponent = NewObject<UAbilityCastComponent>(Owner);
		CastComponent->RegisterComponent();

		AEnemyBaseTestActor* InCircleEnemy = World->SpawnActor<AEnemyBaseTestActor>();
		AEnemyBaseTestActor* OutOfCircleEnemy = World->SpawnActor<AEnemyBaseTestActor>();
		if (!TestNotNull(TEXT("In-circle AEnemyBaseTestActor should spawn"), InCircleEnemy)
			|| !TestNotNull(TEXT("Out-of-circle AEnemyBaseTestActor should spawn"), OutOfCircleEnemy))
		{
			return false;
		}
		InCircleEnemy->TickCheckDetection(FVector::ZeroVector); // Idle -> Alert
		OutOfCircleEnemy->TickCheckDetection(FVector::ZeroVector); // Idle -> Alert
		const FVector DesiredTargetLocation(500.0f, 0.0f, 0.0f);
		// Distances derived from CastComponent->ThrownCircleLandingRadiusUnits itself
		// (radius = 4x placeholder body diameter per AbilityData - not touched by this
		// diff) rather than a hardcoded radius literal, so this stays correct if that
		// default ever changes.
		InCircleEnemy->SetActorLocation(DesiredTargetLocation + FVector(CastComponent->ThrownCircleLandingRadiusUnits * 0.25f, 0.0f, 0.0f)); // inside the radius
		OutOfCircleEnemy->SetActorLocation(DesiredTargetLocation + FVector(CastComponent->ThrownCircleLandingRadiusUnits * 1.75f, 0.0f, 0.0f)); // outside the radius

		const int32 AffectedCount = CastComponent->TryCastThrownAbilityAtLocation(EAbilitySlot::Stun, DesiredTargetLocation);
		TestEqual(TEXT("Only the in-circle enemy should be affected by Stun's AoE"), AffectedCount, 1);
		TestEqual(TEXT("The in-circle enemy should be Controlled by Stun"),
			static_cast<uint8>(InCircleEnemy->GetEnemyState()), static_cast<uint8>(EEnemyState::Controlled));
		TestEqual(TEXT("The out-of-circle enemy should be left untouched"),
			static_cast<uint8>(OutOfCircleEnemy->GetEnemyState()), static_cast<uint8>(EEnemyState::Alert));
	}

	// (n-stun) Range-tier clamp via Stun/Short (issue #256): closes the coverage gap
	// PR #280 deferred - GetThrowRangeUnitsForTier's EAbilityRange::Short branch was
	// previously reachable only from production code, never exercised by a test
	// (every existing thrown-ability test used Sleep/Long). Mirrors case (n) exactly,
	// substituting LongThrowRangeUnits -> ShortThrowRangeUnits and Sleep -> Stun.
	{
		UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
		if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
		{
			return false;
		}
		APawn* Owner = World->SpawnActor<APawn>();
		UAbilityUnlockComponent* UnlockComponent = NewObject<UAbilityUnlockComponent>(Owner);
		UnlockComponent->RegisterComponent(); // Stun is unlocked by default - no NotifyLevelReached() needed
		UAbilityCooldownComponent* CooldownComponent = NewObject<UAbilityCooldownComponent>(Owner);
		CooldownComponent->RegisterComponent();
		UAbilityCastComponent* CastComponent = NewObject<UAbilityCastComponent>(Owner);
		CastComponent->RegisterComponent();

		// Owner defaults to the World origin - the clamped landing point for a desired
		// location straight down +X beyond ShortThrowRangeUnits is exactly
		// (ShortThrowRangeUnits, 0, 0).
		const FVector ClampedLandingPoint(CastComponent->ShortThrowRangeUnits, 0.0f, 0.0f);
		const FVector DesiredTargetLocation = ClampedLandingPoint * 10.0f; // well beyond the Short tier's range

		AEnemyBaseTestActor* Enemy = World->SpawnActor<AEnemyBaseTestActor>();
		if (!TestNotNull(TEXT("AEnemyBaseTestActor should spawn into the test World"), Enemy))
		{
			return false;
		}
		Enemy->SetActorLocation(ClampedLandingPoint);
		Enemy->TickCheckDetection(ClampedLandingPoint); // Idle -> Alert

		const int32 AffectedCount = CastComponent->TryCastThrownAbilityAtLocation(EAbilitySlot::Stun, DesiredTargetLocation);
		TestEqual(TEXT("The enemy at the clamped landing point should be affected"), AffectedCount, 1);
		TestEqual(TEXT("The enemy at the clamped landing point should be Controlled"),
			static_cast<uint8>(Enemy->GetEnemyState()), static_cast<uint8>(EEnemyState::Controlled));
	}

	// (o) Zero enemies in the landing circle still consumes the cooldown - contrast
	// directly with case (c)'s "a whiff must not consume the cooldown" for
	// TryCastAbility; this is the deliberately different, documented contract for
	// TryCastThrownAbilityAtLocation (a thrown bomb commits the moment it's thrown).
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

		const int32 AffectedCount = CastComponent->TryCastThrownAbilityAtLocation(EAbilitySlot::Sleep, FVector(500.0f, 0.0f, 0.0f));
		TestEqual(TEXT("A throw landing on zero enemies should return 0, not -1"), AffectedCount, 0);
		TestTrue(TEXT("A 0-affected throw must still consume the cooldown"),
			CooldownComponent->IsOnCooldown(EAbilitySlot::Sleep));
	}

	// (p) Multi-target: two enemies inside the landing circle both become Controlled
	// and OnAbilityCastApplied fires exactly twice (once per enemy).
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

		UAbilityCastAppliedTestListener* Listener = NewObject<UAbilityCastAppliedTestListener>();
		CastComponent->OnAbilityCastApplied.AddDynamic(Listener, &UAbilityCastAppliedTestListener::HandleAbilityCastApplied);

		AEnemyBaseTestActor* FirstEnemy = World->SpawnActor<AEnemyBaseTestActor>();
		AEnemyBaseTestActor* SecondEnemy = World->SpawnActor<AEnemyBaseTestActor>();
		if (!TestNotNull(TEXT("First AEnemyBaseTestActor should spawn"), FirstEnemy)
			|| !TestNotNull(TEXT("Second AEnemyBaseTestActor should spawn"), SecondEnemy))
		{
			return false;
		}
		FirstEnemy->TickCheckDetection(FVector::ZeroVector); // Idle -> Alert
		SecondEnemy->TickCheckDetection(FVector::ZeroVector); // Idle -> Alert
		const FVector DesiredTargetLocation(500.0f, 0.0f, 0.0f);
		FirstEnemy->SetActorLocation(DesiredTargetLocation + FVector(50.0f, 0.0f, 0.0f));
		SecondEnemy->SetActorLocation(DesiredTargetLocation + FVector(-50.0f, 0.0f, 0.0f));

		const int32 AffectedCount = CastComponent->TryCastThrownAbilityAtLocation(EAbilitySlot::Sleep, DesiredTargetLocation);
		TestEqual(TEXT("Both enemies inside the circle should be affected"), AffectedCount, 2);
		TestEqual(TEXT("The first enemy should be Controlled"),
			static_cast<uint8>(FirstEnemy->GetEnemyState()), static_cast<uint8>(EEnemyState::Controlled));
		TestEqual(TEXT("The second enemy should be Controlled"),
			static_cast<uint8>(SecondEnemy->GetEnemyState()), static_cast<uint8>(EEnemyState::Controlled));
		TestEqual(TEXT("OnAbilityCastApplied should have fired exactly twice"), Listener->CallCount, 2);
	}

	// (p2) AoE sweep over an already-Controlled (different-ability) enemy (PR #280
	// review, MEDIUM finding 3): TryCastThrownAbilityAtLocation's loop calls
	// ReceiveControl unconditionally on every enemy inside the landing circle, so an
	// enemy already Controlled by a different, non-wake-flagged ability (Root) must
	// stay Controlled by Root and must not be double-counted/re-broadcast, since it
	// wasn't Alert/Attack immediately before this call.
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

		AEnemyBaseTestActor* AlreadyControlled = World->SpawnActor<AEnemyBaseTestActor>();
		if (!TestNotNull(TEXT("AEnemyBaseTestActor should spawn into the test World"), AlreadyControlled))
		{
			return false;
		}
		AlreadyControlled->TickCheckDetection(FVector::ZeroVector); // Idle -> Alert
		AlreadyControlled->ReceiveControl(EAbilitySlot::Root); // Alert -> Controlled by Root (not wake-flagged)
		const FVector DesiredTargetLocation(500.0f, 0.0f, 0.0f);
		AlreadyControlled->SetActorLocation(DesiredTargetLocation); // dead centre of the landing circle

		const int32 AffectedCount = CastComponent->TryCastThrownAbilityAtLocation(EAbilitySlot::Sleep, DesiredTargetLocation);
		TestEqual(TEXT("An already-Controlled enemy inside the circle should not be counted as freshly affected"), AffectedCount, 0);
		TestEqual(TEXT("A Root-Controlled enemy swept by Sleep's AoE should remain Controlled by Root (not wake-flagged)"),
			static_cast<uint8>(AlreadyControlled->GetEnemyState()), static_cast<uint8>(EEnemyState::Controlled));
	}

	// (q) Gate failure (locked ability): default UAbilityUnlockComponent state only
	// unlocks Stun, so throwing Sleep must return -1 and change nothing, mirroring
	// case (a)'s TryCastAbility gate-failure contract.
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

		const FVector DesiredTargetLocation(500.0f, 0.0f, 0.0f);
		AEnemyBaseTestActor* Enemy = World->SpawnActor<AEnemyBaseTestActor>();
		if (!TestNotNull(TEXT("AEnemyBaseTestActor should spawn into the test World"), Enemy))
		{
			return false;
		}
		Enemy->SetActorLocation(DesiredTargetLocation);
		Enemy->TickCheckDetection(DesiredTargetLocation); // Idle -> Alert

		const int32 AffectedCount = CastComponent->TryCastThrownAbilityAtLocation(EAbilitySlot::Sleep, DesiredTargetLocation);
		TestEqual(TEXT("TryCastThrownAbilityAtLocation for a locked ability should return -1"), AffectedCount, -1);
		TestEqual(TEXT("A locked-ability throw should not change the enemy's state"),
			static_cast<uint8>(Enemy->GetEnemyState()), static_cast<uint8>(EEnemyState::Alert));
		TestFalse(TEXT("A gate-failed throw must not consume the cooldown"),
			CooldownComponent->IsOnCooldown(EAbilitySlot::Sleep));
	}

	// (r) world-paused gate via TryCastThrownAbilityAtLocation (PR #280 review, HIGH
	// finding 1): ResolvePassedCastGates was factored out of TryCastAbility so
	// TryCastThrownAbilityAtLocation shares the identical gate chain - this mirrors
	// case (k), but only case (q) had exercised the new entry point's gate chain
	// before this fix, and only for the "not unlocked" gate.
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

		AEnemyBaseTestActor* Enemy = World->SpawnActor<AEnemyBaseTestActor>();
		if (!TestNotNull(TEXT("AEnemyBaseTestActor should spawn into the test World"), Enemy))
		{
			return false;
		}
		const FVector DesiredTargetLocation(500.0f, 0.0f, 0.0f);
		Enemy->SetActorLocation(DesiredTargetLocation);
		Enemy->TickCheckDetection(DesiredTargetLocation); // Idle -> Alert

		// See case (k)'s comment: SetPauserPlayerState() is what actually latches
		// World->IsPaused() in a CreateNewMap() test World.
		APlayerState* PauserPlayerState = NewObject<APlayerState>(Owner);
		World->GetWorldSettings()->SetPauserPlayerState(PauserPlayerState);
		if (!TestTrue(TEXT("World should report paused after SetPauserPlayerState()"), World->IsPaused()))
		{
			return false;
		}

		const int32 AffectedCount = CastComponent->TryCastThrownAbilityAtLocation(EAbilitySlot::Sleep, DesiredTargetLocation);
		TestEqual(TEXT("TryCastThrownAbilityAtLocation should refuse while the world is paused"), AffectedCount, -1);
		TestEqual(TEXT("A paused-world throw should not change the enemy's state"),
			static_cast<uint8>(Enemy->GetEnemyState()), static_cast<uint8>(EEnemyState::Alert));
		TestFalse(TEXT("A paused-world throw must not consume the cooldown"),
			CooldownComponent->IsOnCooldown(EAbilitySlot::Sleep));
	}

	// (s) briefing-visible gate via TryCastThrownAbilityAtLocation (PR #280 review,
	// HIGH finding 1), mirroring case (l).
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

		AEnemyBaseTestActor* Enemy = World->SpawnActor<AEnemyBaseTestActor>();
		if (!TestNotNull(TEXT("AEnemyBaseTestActor should spawn into the test World"), Enemy))
		{
			return false;
		}
		const FVector DesiredTargetLocation(500.0f, 0.0f, 0.0f);
		Enemy->SetActorLocation(DesiredTargetLocation);
		Enemy->TickCheckDetection(DesiredTargetLocation); // Idle -> Alert

		// See case (l)'s comment for why this bootstraps a real possessed controller
		// rather than just setting World pause state.
		AKrowdKontrolPlayerController* Controller = World->SpawnActor<AKrowdKontrolPlayerController>();
		if (!TestNotNull(TEXT("Controller should spawn"), Controller))
		{
			return false;
		}
		Controller->Player = NewObject<ULocalPlayer>(GEngine);
		Controller->SetAsLocalPlayerController();
		Controller->Possess(Owner);
		World->AddController(Controller);
		Controller->DispatchBeginPlay();
		if (!TestNotNull(TEXT("BriefingCardWidgetInstance should exist"), ToRawPtr(Controller->BriefingCardWidgetInstance)))
		{
			return false;
		}

		FLevelBriefingRow Row;
		Row.LevelDisplayName = FText::FromString(TEXT("LEVEL 1"));
		Controller->BriefingCardWidgetInstance->ShowBriefing(Row);

		const int32 AffectedCountWhileVisible = CastComponent->TryCastThrownAbilityAtLocation(EAbilitySlot::Sleep, DesiredTargetLocation);
		TestEqual(TEXT("TryCastThrownAbilityAtLocation should refuse while the briefing card is visible"), AffectedCountWhileVisible, -1);
		TestEqual(TEXT("A briefing-visible throw should not change the enemy's state"),
			static_cast<uint8>(Enemy->GetEnemyState()), static_cast<uint8>(EEnemyState::Alert));

		Controller->BriefingCardWidgetInstance->DismissBriefing();
		const int32 AffectedCountAfterDismiss = CastComponent->TryCastThrownAbilityAtLocation(EAbilitySlot::Sleep, DesiredTargetLocation);
		TestEqual(TEXT("TryCastThrownAbilityAtLocation should succeed again once the briefing card is dismissed"), AffectedCountAfterDismiss, 1);
	}

	// (t) lockout gate via TryCastThrownAbilityAtLocation (PR #280 review, HIGH
	// finding 1), mirroring case (h).
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
		UAbilityLockoutComponent* LockoutComponent = NewObject<UAbilityLockoutComponent>(Owner);
		LockoutComponent->RegisterComponent();
		UAbilityCastComponent* CastComponent = NewObject<UAbilityCastComponent>(Owner);
		CastComponent->RegisterComponent();

		AEnemyBaseTestActor* Enemy = World->SpawnActor<AEnemyBaseTestActor>();
		if (!TestNotNull(TEXT("AEnemyBaseTestActor should spawn into the test World"), Enemy))
		{
			return false;
		}
		const FVector DesiredTargetLocation(500.0f, 0.0f, 0.0f);
		Enemy->SetActorLocation(DesiredTargetLocation);
		Enemy->TickCheckDetection(DesiredTargetLocation); // Idle -> Alert

		// Locks Sleep directly - this tests TryCastThrownAbilityAtLocation's gate, not
		// the lockout component's own trigger logic (see case (h)'s equivalent note).
		LockoutComponent->HandleAbilityCastApplied(EAbilitySlot::Sleep, nullptr);
		LockoutComponent->HandlePunishmentTriggered();

		const int32 AffectedCount = CastComponent->TryCastThrownAbilityAtLocation(EAbilitySlot::Sleep, DesiredTargetLocation);
		TestEqual(TEXT("TryCastThrownAbilityAtLocation should refuse a locked-out ability"), AffectedCount, -1);
		TestEqual(TEXT("A locked-out throw should not change the enemy's state"),
			static_cast<uint8>(Enemy->GetEnemyState()), static_cast<uint8>(EEnemyState::Alert));
		TestFalse(TEXT("A locked-out throw must not consume the cooldown"),
			CooldownComponent->IsOnCooldown(EAbilitySlot::Sleep));
	}

	// (u-stun) End-to-end banking (issue #256 acceptance criterion "stunned enemy
	// still banks"): an enemy Controlled via the new cursor-aimed Stun AoE throw
	// (TryCastThrownAbilityAtLocation, the entry point this issue wires up - not the
	// legacy auto-nearest TryCastAbility path) still reaches Banked through a real
	// ARoomActor/ATargetZone physical-overlap chain. Needs a real ARoomActor, not a
	// bare ATargetZone: ATargetZone only broadcasts OnActorBanked (pinned by
	// KrowdKontrolTargetZoneTest.cpp, which never asserts the overlapping actor's own
	// state) - it is ARoomActor::HandleZoneActorBanked that actually calls
	// AEnemyBase::TransitionToBanked(), per KrowdKontrolRoomActorBankingWiringTest.cpp's
	// file comment. This case Controls the enemy through this PR's thrown-AoE entry
	// point instead of that wiring test's direct ReceiveControl() call.
	// AEnemyBaseTestActor (used everywhere else in this file) has no collision
	// component, so this case needs a real production enemy (ARunnerEnemy), the same
	// substitution that wiring test makes for the same reason.
	{
		UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
		if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
		{
			return false;
		}
		// Required for a real physics overlap to fire OnComponentBeginOverlap - see
		// KrowdKontrolTargetZoneTest.cpp's file comment for why both calls are needed.
		World->InitializeActorsForPlay(FURL());
		World->SetBegunPlay(true);

		APawn* Owner = World->SpawnActor<APawn>();
		if (!TestNotNull(TEXT("APawn should spawn into the test World"), Owner))
		{
			return false;
		}
		UAbilityUnlockComponent* UnlockComponent = NewObject<UAbilityUnlockComponent>(Owner);
		UnlockComponent->RegisterComponent(); // Stun is unlocked by default
		UAbilityCooldownComponent* CooldownComponent = NewObject<UAbilityCooldownComponent>(Owner);
		CooldownComponent->RegisterComponent();
		UAbilityCastComponent* CastComponent = NewObject<UAbilityCastComponent>(Owner);
		CastComponent->RegisterComponent();

		// Plain (non-deferred) spawn, same as KrowdKontrolRoomActorTest.cpp - BeginPlay()
		// fires immediately with an empty TargetZones array, so EnsureBankingZonesWired()
		// below is called explicitly after AddTargetZone(), mirroring that method's own
		// documented "safe to call more than once" / self-heal contract.
		ARoomActor* Room = World->SpawnActor<ARoomActor>();
		if (!TestNotNull(TEXT("Room should spawn"), Room))
		{
			return false;
		}
		// RU-NNR matches ARunnerEnemy (RoomActorBankingWiringTest.cpp's own mapping).
		AActor* Marker = Room->AddTargetZone(EEnemyType::RU_NNR);
		if (!TestNotNull(TEXT("Marker should spawn"), Marker))
		{
			return false;
		}
		Room->EnsureBankingZonesWired();

		ATargetZone* Zone = nullptr;
		TArray<AActor*> AttachedActors;
		Marker->GetAttachedActors(AttachedActors);
		for (AActor* Attached : AttachedActors)
		{
			if (ATargetZone* AttachedZone = Cast<ATargetZone>(Attached))
			{
				Zone = AttachedZone;
				break;
			}
		}
		if (!TestNotNull(TEXT("Marker should have a self-healed ATargetZone attached"), Zone))
		{
			return false;
		}

		// Within ShortThrowRangeUnits of Owner (at the World origin - see case (n)'s
		// comment) so the thrown AoE below lands unclamped, directly on the enemy, and
		// well clear of the zone (attached at the Room's own origin location - see
		// AddTargetZone()'s own comment) so no accidental overlap happens before the cast.
		ARunnerEnemy* Enemy = World->SpawnActor<ARunnerEnemy>(FVector(700.0f, 0.0f, 0.0f), FRotator::ZeroRotator);
		if (!TestNotNull(TEXT("ARunnerEnemy should spawn into the test World"), Enemy))
		{
			return false;
		}
		Enemy->TickCheckDetection(Enemy->GetActorLocation()); // Idle -> Alert

		const int32 AffectedCount = CastComponent->TryCastThrownAbilityAtLocation(EAbilitySlot::Stun, Enemy->GetActorLocation());
		TestEqual(TEXT("The Stun AoE throw should affect exactly the one enemy at the landing point"), AffectedCount, 1);
		TestEqual(TEXT("The enemy should be Controlled after the Stun AoE throw"),
			static_cast<uint8>(Enemy->GetEnemyState()), static_cast<uint8>(EEnemyState::Controlled));

		Enemy->SetActorLocation(Zone->GetActorLocation(), /*bSweep=*/true);
		TestEqual(TEXT("A Stun-AoE-controlled enemy overlapping a target zone should reach Banked"),
			static_cast<uint8>(Enemy->GetEnemyState()), static_cast<uint8>(EEnemyState::Banked));
	}

	// (p-root) TryCastLineAbilityTowardLocation via Root (issue #255): in-path vs
	// off-path - an enemy on the Owner->cursor-direction line within LineHitWidthUnits
	// is affected, one at the same along-line distance but well beyond LineHitWidthUnits
	// perpendicular to it is not.
	{
		UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
		if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
		{
			return false;
		}
		APawn* Owner = World->SpawnActor<APawn>();
		UAbilityUnlockComponent* UnlockComponent = NewObject<UAbilityUnlockComponent>(Owner);
		UnlockComponent->RegisterComponent();
		UnlockComponent->NotifyLevelReached(3); // unlocks Root
		UAbilityCooldownComponent* CooldownComponent = NewObject<UAbilityCooldownComponent>(Owner);
		CooldownComponent->RegisterComponent();
		UAbilityCastComponent* CastComponent = NewObject<UAbilityCastComponent>(Owner);
		CastComponent->RegisterComponent();

		AEnemyBaseTestActor* OnLineEnemy = World->SpawnActor<AEnemyBaseTestActor>();
		AEnemyBaseTestActor* OffLineEnemy = World->SpawnActor<AEnemyBaseTestActor>();
		if (!TestNotNull(TEXT("(p-root) On-line AEnemyBaseTestActor should spawn"), OnLineEnemy)
			|| !TestNotNull(TEXT("(p-root) Off-line AEnemyBaseTestActor should spawn"), OffLineEnemy))
		{
			return false;
		}
		OnLineEnemy->TickCheckDetection(FVector::ZeroVector); // Idle -> Alert
		OffLineEnemy->TickCheckDetection(FVector::ZeroVector); // Idle -> Alert
		const FVector CursorLocation(500.0f, 0.0f, 0.0f);
		OnLineEnemy->SetActorLocation(FVector(400.0f, 0.0f, 0.0f)); // on the line, well within LineHitWidthUnits
		OffLineEnemy->SetActorLocation(FVector(400.0f, CastComponent->LineHitWidthUnits * 3.0f, 0.0f)); // same X, far off the line

		const int32 AffectedCount = CastComponent->TryCastLineAbilityTowardLocation(EAbilitySlot::Root, CursorLocation);
		TestEqual(TEXT("(p-root) Only the on-line enemy should be affected"), AffectedCount, 1);
		TestEqual(TEXT("(p-root) The on-line enemy should be Controlled"),
			static_cast<uint8>(OnLineEnemy->GetEnemyState()), static_cast<uint8>(EEnemyState::Controlled));
		TestEqual(TEXT("(p-root) The off-line enemy should be left untouched"),
			static_cast<uint8>(OffLineEnemy->GetEnemyState()), static_cast<uint8>(EEnemyState::Alert));
	}

	// (q-root) Piercing/multi-hit (issue #255): three enemies on the same line at
	// different distances from the Owner, all within LongThrowRangeUnits, are ALL
	// affected - this is the test that actually proves and documents the "all along
	// the line, not just first" design decision (see the plan/PR body's rationale:
	// consistent with every other locked shape hitting everything inside it).
	{
		UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
		if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
		{
			return false;
		}
		APawn* Owner = World->SpawnActor<APawn>();
		UAbilityUnlockComponent* UnlockComponent = NewObject<UAbilityUnlockComponent>(Owner);
		UnlockComponent->RegisterComponent();
		UnlockComponent->NotifyLevelReached(3); // unlocks Root
		UAbilityCooldownComponent* CooldownComponent = NewObject<UAbilityCooldownComponent>(Owner);
		CooldownComponent->RegisterComponent();
		UAbilityCastComponent* CastComponent = NewObject<UAbilityCastComponent>(Owner);
		CastComponent->RegisterComponent();

		AEnemyBaseTestActor* NearEnemy = World->SpawnActor<AEnemyBaseTestActor>();
		AEnemyBaseTestActor* MidEnemy = World->SpawnActor<AEnemyBaseTestActor>();
		AEnemyBaseTestActor* FarEnemy = World->SpawnActor<AEnemyBaseTestActor>();
		if (!TestNotNull(TEXT("(q-root) Near AEnemyBaseTestActor should spawn"), NearEnemy)
			|| !TestNotNull(TEXT("(q-root) Mid AEnemyBaseTestActor should spawn"), MidEnemy)
			|| !TestNotNull(TEXT("(q-root) Far AEnemyBaseTestActor should spawn"), FarEnemy))
		{
			return false;
		}
		NearEnemy->TickCheckDetection(FVector::ZeroVector); // Idle -> Alert
		MidEnemy->TickCheckDetection(FVector::ZeroVector); // Idle -> Alert
		FarEnemy->TickCheckDetection(FVector::ZeroVector); // Idle -> Alert
		NearEnemy->SetActorLocation(FVector(200.0f, 0.0f, 0.0f));
		MidEnemy->SetActorLocation(FVector(600.0f, 0.0f, 0.0f));
		FarEnemy->SetActorLocation(FVector(1000.0f, 0.0f, 0.0f));
		const FVector CursorLocation(1200.0f, 0.0f, 0.0f); // aims +X; all 3 enemies within LongThrowRangeUnits

		const int32 AffectedCount = CastComponent->TryCastLineAbilityTowardLocation(EAbilitySlot::Root, CursorLocation);
		TestEqual(TEXT("(q-root) All three enemies along the line should be affected (piercing)"), AffectedCount, 3);
		TestEqual(TEXT("(q-root) The near enemy should be Controlled"),
			static_cast<uint8>(NearEnemy->GetEnemyState()), static_cast<uint8>(EEnemyState::Controlled));
		TestEqual(TEXT("(q-root) The mid enemy should be Controlled"),
			static_cast<uint8>(MidEnemy->GetEnemyState()), static_cast<uint8>(EEnemyState::Controlled));
		TestEqual(TEXT("(q-root) The far enemy should be Controlled"),
			static_cast<uint8>(FarEnemy->GetEnemyState()), static_cast<uint8>(EEnemyState::Controlled));
	}

	// (r-root) Range clamp at Long tier (issue #255): Root's line always extends to
	// exactly LongThrowRangeUnits from the Owner, never further, even if the cursor is
	// placed well beyond it - mirrors case (n)'s clamp-boundary shape.
	{
		UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
		if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
		{
			return false;
		}
		APawn* Owner = World->SpawnActor<APawn>();
		UAbilityUnlockComponent* UnlockComponent = NewObject<UAbilityUnlockComponent>(Owner);
		UnlockComponent->RegisterComponent();
		UnlockComponent->NotifyLevelReached(3); // unlocks Root
		UAbilityCooldownComponent* CooldownComponent = NewObject<UAbilityCooldownComponent>(Owner);
		CooldownComponent->RegisterComponent();
		UAbilityCastComponent* CastComponent = NewObject<UAbilityCastComponent>(Owner);
		CastComponent->RegisterComponent();

		// Owner defaults to the World origin; cursor placed 10x beyond LongThrowRangeUnits
		// along +X, so the line's endpoint is exactly (LongThrowRangeUnits, 0, 0).
		const FVector LineEndPoint(CastComponent->LongThrowRangeUnits, 0.0f, 0.0f);
		const FVector CursorLocation = LineEndPoint * 10.0f;

		AEnemyBaseTestActor* AtEndpointEnemy = World->SpawnActor<AEnemyBaseTestActor>();
		AEnemyBaseTestActor* BeyondEndpointEnemy = World->SpawnActor<AEnemyBaseTestActor>();
		if (!TestNotNull(TEXT("(r-root) At-endpoint AEnemyBaseTestActor should spawn"), AtEndpointEnemy)
			|| !TestNotNull(TEXT("(r-root) Beyond-endpoint AEnemyBaseTestActor should spawn"), BeyondEndpointEnemy))
		{
			return false;
		}
		AtEndpointEnemy->TickCheckDetection(FVector::ZeroVector); // Idle -> Alert
		BeyondEndpointEnemy->TickCheckDetection(FVector::ZeroVector); // Idle -> Alert
		AtEndpointEnemy->SetActorLocation(LineEndPoint);
		BeyondEndpointEnemy->SetActorLocation(LineEndPoint + FVector(CastComponent->LineHitWidthUnits * 2.0f, 0.0f, 0.0f));

		const int32 AffectedCount = CastComponent->TryCastLineAbilityTowardLocation(EAbilitySlot::Root, CursorLocation);
		TestEqual(TEXT("(r-root) Only the enemy at the clamped endpoint should be affected"), AffectedCount, 1);
		TestEqual(TEXT("(r-root) The at-endpoint enemy should be Controlled"),
			static_cast<uint8>(AtEndpointEnemy->GetEnemyState()), static_cast<uint8>(EEnemyState::Controlled));
		TestEqual(TEXT("(r-root) The beyond-endpoint enemy should be left untouched"),
			static_cast<uint8>(BeyondEndpointEnemy->GetEnemyState()), static_cast<uint8>(EEnemyState::Alert));
	}

	// (s-root) Pure-math ComputeLineEndLocation cases (issue #255) - no UWorld needed,
	// same shape as case (n2)'s ComputeClampedThrowLocation cases. Contrasts directly
	// with ComputeClampedThrowLocation: a Line ability always extends the FULL
	// LineRangeUnits, never clamped down to the cursor's own (shorter) distance.
	{
		const FVector OwnerLocation = FVector::ZeroVector;
		const float LineRangeUnits = 2000.0f;
		const FVector FallbackDirection(1.0f, 0.0f, 0.0f);

		// A cursor well within range still extends the line the FULL LineRangeUnits,
		// NOT clamped down to the cursor's own (shorter) distance.
		const FVector NearCursor(500.0f, 0.0f, 0.0f);
		const FVector EndForNearCursor = UAbilityCastComponent::ComputeLineEndLocation(OwnerLocation, NearCursor, LineRangeUnits, FallbackDirection);
		TestTrue(TEXT("(s-root) A near cursor should still extend the line to the full LineRangeUnits"),
			EndForNearCursor.Equals(FVector(2000.0f, 0.0f, 0.0f), 0.5f));

		// A cursor beyond range: the line still stops at exactly LineRangeUnits, not the
		// cursor's own (longer) distance.
		const FVector FarCursor(9000.0f, 0.0f, 0.0f);
		const FVector EndForFarCursor = UAbilityCastComponent::ComputeLineEndLocation(OwnerLocation, FarCursor, LineRangeUnits, FallbackDirection);
		TestTrue(TEXT("(s-root) A far cursor should clamp the line's end to exactly LineRangeUnits"),
			EndForFarCursor.Equals(FVector(2000.0f, 0.0f, 0.0f), 0.5f));

		// Degenerate case: DesiredTargetLocation == OwnerLocation, direction undefined -
		// falls back to FallbackDirection.
		const FVector DegenerateEnd = UAbilityCastComponent::ComputeLineEndLocation(OwnerLocation, OwnerLocation, LineRangeUnits, FallbackDirection);
		TestTrue(TEXT("(s-root) A degenerate (coincident) cursor should fall back to FallbackDirection"),
			DegenerateEnd.Equals(FVector(2000.0f, 0.0f, 0.0f), 0.5f));

		// Near-degenerate case (code review, PR #282): a cursor a few units from the
		// owner - not exactly coincident, but well inside the same real-gameplay-units
		// dead zone ComputeFacingRotation guards against (PR #279) - must also fall
		// back to FallbackDirection rather than normalizing floating-point noise.
		// Cursor is offset perpendicular to FallbackDirection so a buggy
		// implementation that just normalizes the tiny delta (instead of falling
		// back) would produce a visibly different, wrong end location.
		const FVector NearDegenerateCursor(0.0f, 5.0f, 0.0f);
		const FVector NearDegenerateEnd = UAbilityCastComponent::ComputeLineEndLocation(OwnerLocation, NearDegenerateCursor, LineRangeUnits, FallbackDirection);
		TestTrue(TEXT("(s-root) A cursor inside the dead zone (but not exactly coincident) should also fall back to FallbackDirection"),
			NearDegenerateEnd.Equals(FVector(2000.0f, 0.0f, 0.0f), 0.5f));
	}

	// (t-root) Zero enemies on the line still consumes the cooldown (issue #255) -
	// mirrors case (o)'s identical "a whiff still commits" contract for
	// TryCastThrownAbilityAtLocation.
	{
		UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
		if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
		{
			return false;
		}
		APawn* Owner = World->SpawnActor<APawn>();
		UAbilityUnlockComponent* UnlockComponent = NewObject<UAbilityUnlockComponent>(Owner);
		UnlockComponent->RegisterComponent();
		UnlockComponent->NotifyLevelReached(3); // unlocks Root
		UAbilityCooldownComponent* CooldownComponent = NewObject<UAbilityCooldownComponent>(Owner);
		CooldownComponent->RegisterComponent();
		UAbilityCastComponent* CastComponent = NewObject<UAbilityCastComponent>(Owner);
		CastComponent->RegisterComponent();

		const int32 AffectedCount = CastComponent->TryCastLineAbilityTowardLocation(EAbilitySlot::Root, FVector(500.0f, 0.0f, 0.0f));
		TestEqual(TEXT("(t-root) A line hitting zero enemies should return 0, not -1"), AffectedCount, 0);
		TestTrue(TEXT("(t-root) A 0-affected line cast must still consume the cooldown"),
			CooldownComponent->IsOnCooldown(EAbilitySlot::Root));
	}

	// (u-root) End-to-end banking through the line-cast path (issue #255 acceptance
	// criterion "rooted enemy still banks once herded"): an enemy Controlled via
	// TryCastLineAbilityTowardLocation (this PR's new entry point) still reaches
	// Banked through a real ARoomActor/ATargetZone physical-overlap chain - mirrors
	// case (u-stun)'s shape exactly, substituting the line-cast entry point for the
	// thrown-AoE one. Pre-existing coverage (KrowdKontrolRoomActorBankingWiringTest.cpp)
	// only drove this through the low-level ReceiveControl(EAbilitySlot::Root) call,
	// never through the actual cast component path a player uses.
	{
		UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
		if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
		{
			return false;
		}
		// Required for a real physics overlap to fire OnComponentBeginOverlap - see
		// KrowdKontrolTargetZoneTest.cpp's file comment for why both calls are needed.
		World->InitializeActorsForPlay(FURL());
		World->SetBegunPlay(true);

		APawn* Owner = World->SpawnActor<APawn>();
		if (!TestNotNull(TEXT("APawn should spawn into the test World"), Owner))
		{
			return false;
		}
		UAbilityUnlockComponent* UnlockComponent = NewObject<UAbilityUnlockComponent>(Owner);
		UnlockComponent->RegisterComponent();
		UnlockComponent->NotifyLevelReached(3); // unlocks Root
		UAbilityCooldownComponent* CooldownComponent = NewObject<UAbilityCooldownComponent>(Owner);
		CooldownComponent->RegisterComponent();
		UAbilityCastComponent* CastComponent = NewObject<UAbilityCastComponent>(Owner);
		CastComponent->RegisterComponent();

		// Plain (non-deferred) spawn, same as (u-stun) - BeginPlay() fires immediately
		// with an empty TargetZones array, so EnsureBankingZonesWired() below is called
		// explicitly after AddTargetZone().
		ARoomActor* Room = World->SpawnActor<ARoomActor>();
		if (!TestNotNull(TEXT("Room should spawn"), Room))
		{
			return false;
		}
		// RU-NNR matches ARunnerEnemy (RoomActorBankingWiringTest.cpp's own mapping);
		// type-keyed acceptance means the controlling ability (Root here) doesn't need
		// to match the zone's type, same as (u-stun).
		AActor* Marker = Room->AddTargetZone(EEnemyType::RU_NNR);
		if (!TestNotNull(TEXT("Marker should spawn"), Marker))
		{
			return false;
		}
		Room->EnsureBankingZonesWired();

		ATargetZone* Zone = nullptr;
		TArray<AActor*> AttachedActors;
		Marker->GetAttachedActors(AttachedActors);
		for (AActor* Attached : AttachedActors)
		{
			if (ATargetZone* AttachedZone = Cast<ATargetZone>(Attached))
			{
				Zone = AttachedZone;
				break;
			}
		}
		if (!TestNotNull(TEXT("Marker should have a self-healed ATargetZone attached"), Zone))
		{
			return false;
		}

		// On the Owner (World origin) -> +X line, well within LongThrowRangeUnits, and
		// well clear of the zone (attached at the Room's own origin location) so no
		// accidental overlap happens before the cast.
		ARunnerEnemy* Enemy = World->SpawnActor<ARunnerEnemy>(FVector(700.0f, 0.0f, 0.0f), FRotator::ZeroRotator);
		if (!TestNotNull(TEXT("(u-root) ARunnerEnemy should spawn into the test World"), Enemy))
		{
			return false;
		}
		Enemy->TickCheckDetection(Enemy->GetActorLocation()); // Idle -> Alert

		const int32 AffectedCount = CastComponent->TryCastLineAbilityTowardLocation(EAbilitySlot::Root, FVector(500.0f, 0.0f, 0.0f));
		TestEqual(TEXT("(u-root) The Root line cast should affect exactly the one on-line enemy"), AffectedCount, 1);
		TestEqual(TEXT("(u-root) The enemy should be Controlled after the Root line cast"),
			static_cast<uint8>(Enemy->GetEnemyState()), static_cast<uint8>(EEnemyState::Controlled));

		Enemy->SetActorLocation(Zone->GetActorLocation(), /*bSweep=*/true);
		TestEqual(TEXT("(u-root) A Root-line-controlled enemy overlapping a target zone should reach Banked"),
			static_cast<uint8>(Enemy->GetEnemyState()), static_cast<uint8>(EEnemyState::Banked));
	}

	// (u-snare) TryCastConeAbilityTowardLocation via Snare (issue #254): in-cone/in-range
	// vs behind-the-robot (outside the cone entirely) vs in-cone-direction-but-beyond-range.
	{
		UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
		if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
		{
			return false;
		}
		APawn* Owner = World->SpawnActor<APawn>();
		UAbilityUnlockComponent* UnlockComponent = NewObject<UAbilityUnlockComponent>(Owner);
		UnlockComponent->RegisterComponent();
		UnlockComponent->NotifyLevelReached(5); // unlocks Snare
		UAbilityCooldownComponent* CooldownComponent = NewObject<UAbilityCooldownComponent>(Owner);
		CooldownComponent->RegisterComponent();
		UAbilityCastComponent* CastComponent = NewObject<UAbilityCastComponent>(Owner);
		CastComponent->RegisterComponent();

		AEnemyBaseTestActor* InConeEnemy = World->SpawnActor<AEnemyBaseTestActor>();
		AEnemyBaseTestActor* BehindRobotEnemy = World->SpawnActor<AEnemyBaseTestActor>();
		AEnemyBaseTestActor* BeyondRangeEnemy = World->SpawnActor<AEnemyBaseTestActor>();
		if (!TestNotNull(TEXT("(u-snare) In-cone AEnemyBaseTestActor should spawn"), InConeEnemy)
			|| !TestNotNull(TEXT("(u-snare) Behind-robot AEnemyBaseTestActor should spawn"), BehindRobotEnemy)
			|| !TestNotNull(TEXT("(u-snare) Beyond-range AEnemyBaseTestActor should spawn"), BeyondRangeEnemy))
		{
			return false;
		}
		InConeEnemy->TickCheckDetection(FVector::ZeroVector); // Idle -> Alert
		BehindRobotEnemy->TickCheckDetection(FVector::ZeroVector); // Idle -> Alert
		BeyondRangeEnemy->TickCheckDetection(FVector::ZeroVector); // Idle -> Alert
		const FVector CursorLocation(500.0f, 0.0f, 0.0f); // cone aims +X
		InConeEnemy->SetActorLocation(FVector(400.0f, 0.0f, 0.0f)); // on the cone centreline, well in range
		BehindRobotEnemy->SetActorLocation(FVector(-400.0f, 0.0f, 0.0f)); // dot product -1: outside any sub-360 cone
		BeyondRangeEnemy->SetActorLocation(FVector(CastComponent->GetConeRangeUnits(EAbilitySlot::Snare) * 10.0f, 0.0f, 0.0f)); // in-cone direction, well beyond range

		const int32 AffectedCount = CastComponent->TryCastConeAbilityTowardLocation(EAbilitySlot::Snare, CursorLocation);
		TestEqual(TEXT("(u-snare) Only the in-cone-in-range enemy should be affected"), AffectedCount, 1);
		TestEqual(TEXT("(u-snare) The in-cone enemy should be Controlled"),
			static_cast<uint8>(InConeEnemy->GetEnemyState()), static_cast<uint8>(EEnemyState::Controlled));
		TestEqual(TEXT("(u-snare) The behind-robot enemy should be left untouched"),
			static_cast<uint8>(BehindRobotEnemy->GetEnemyState()), static_cast<uint8>(EEnemyState::Alert));
		TestEqual(TEXT("(u-snare) The beyond-range enemy should be left untouched"),
			static_cast<uint8>(BeyondRangeEnemy->GetEnemyState()), static_cast<uint8>(EEnemyState::Alert));
	}

	// (v-snare) Multi-target (issue #254): two enemies both inside the cone are both
	// affected in a single cast, mirroring (q-root)'s piercing/multi-hit shape.
	{
		UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
		if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
		{
			return false;
		}
		APawn* Owner = World->SpawnActor<APawn>();
		UAbilityUnlockComponent* UnlockComponent = NewObject<UAbilityUnlockComponent>(Owner);
		UnlockComponent->RegisterComponent();
		UnlockComponent->NotifyLevelReached(5); // unlocks Snare
		UAbilityCooldownComponent* CooldownComponent = NewObject<UAbilityCooldownComponent>(Owner);
		CooldownComponent->RegisterComponent();
		UAbilityCastComponent* CastComponent = NewObject<UAbilityCastComponent>(Owner);
		CastComponent->RegisterComponent();

		AEnemyBaseTestActor* CentreEnemy = World->SpawnActor<AEnemyBaseTestActor>();
		AEnemyBaseTestActor* OffCentreEnemy = World->SpawnActor<AEnemyBaseTestActor>();
		if (!TestNotNull(TEXT("(v-snare) Centre AEnemyBaseTestActor should spawn"), CentreEnemy)
			|| !TestNotNull(TEXT("(v-snare) Off-centre AEnemyBaseTestActor should spawn"), OffCentreEnemy))
		{
			return false;
		}
		CentreEnemy->TickCheckDetection(FVector::ZeroVector); // Idle -> Alert
		OffCentreEnemy->TickCheckDetection(FVector::ZeroVector); // Idle -> Alert
		const FVector CursorLocation(500.0f, 0.0f, 0.0f); // cone aims +X
		CentreEnemy->SetActorLocation(FVector(400.0f, 0.0f, 0.0f)); // dead-centre
		OffCentreEnemy->SetActorLocation(FVector(346.4f, 200.0f, 0.0f)); // ~30 degrees off centre, inside the 37.5 degree half-angle

		const int32 AffectedCount = CastComponent->TryCastConeAbilityTowardLocation(EAbilitySlot::Snare, CursorLocation);
		TestEqual(TEXT("(v-snare) Both enemies in the cone should be affected"), AffectedCount, 2);
		TestEqual(TEXT("(v-snare) The centre enemy should be Controlled"),
			static_cast<uint8>(CentreEnemy->GetEnemyState()), static_cast<uint8>(EEnemyState::Controlled));
		TestEqual(TEXT("(v-snare) The off-centre enemy should be Controlled"),
			static_cast<uint8>(OffCentreEnemy->GetEnemyState()), static_cast<uint8>(EEnemyState::Controlled));
	}

	// (w-snare) Range clamp boundary at Medium tier (issue #254): an enemy exactly at
	// MediumThrowRangeUnits (on the cone centreline) is affected, one just beyond it is
	// not - mirrors (r-root)'s clamp-boundary shape.
	{
		UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
		if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
		{
			return false;
		}
		APawn* Owner = World->SpawnActor<APawn>();
		UAbilityUnlockComponent* UnlockComponent = NewObject<UAbilityUnlockComponent>(Owner);
		UnlockComponent->RegisterComponent();
		UnlockComponent->NotifyLevelReached(5); // unlocks Snare
		UAbilityCooldownComponent* CooldownComponent = NewObject<UAbilityCooldownComponent>(Owner);
		CooldownComponent->RegisterComponent();
		UAbilityCastComponent* CastComponent = NewObject<UAbilityCastComponent>(Owner);
		CastComponent->RegisterComponent();

		const float RangeUnits = CastComponent->GetConeRangeUnits(EAbilitySlot::Snare);
		const FVector CursorLocation(500.0f, 0.0f, 0.0f); // cone aims +X

		AEnemyBaseTestActor* AtRangeEnemy = World->SpawnActor<AEnemyBaseTestActor>();
		AEnemyBaseTestActor* BeyondRangeEnemy = World->SpawnActor<AEnemyBaseTestActor>();
		if (!TestNotNull(TEXT("(w-snare) At-range AEnemyBaseTestActor should spawn"), AtRangeEnemy)
			|| !TestNotNull(TEXT("(w-snare) Beyond-range AEnemyBaseTestActor should spawn"), BeyondRangeEnemy))
		{
			return false;
		}
		AtRangeEnemy->TickCheckDetection(FVector::ZeroVector); // Idle -> Alert
		BeyondRangeEnemy->TickCheckDetection(FVector::ZeroVector); // Idle -> Alert
		AtRangeEnemy->SetActorLocation(FVector(RangeUnits, 0.0f, 0.0f));
		BeyondRangeEnemy->SetActorLocation(FVector(RangeUnits + 50.0f, 0.0f, 0.0f));

		const int32 AffectedCount = CastComponent->TryCastConeAbilityTowardLocation(EAbilitySlot::Snare, CursorLocation);
		TestEqual(TEXT("(w-snare) Only the at-range enemy should be affected"), AffectedCount, 1);
		TestEqual(TEXT("(w-snare) The at-range enemy should be Controlled"),
			static_cast<uint8>(AtRangeEnemy->GetEnemyState()), static_cast<uint8>(EEnemyState::Controlled));
		TestEqual(TEXT("(w-snare) The beyond-range enemy should be left untouched"),
			static_cast<uint8>(BeyondRangeEnemy->GetEnemyState()), static_cast<uint8>(EEnemyState::Alert));
	}

	// (x-snare) Pure-math ComputeConeDirection and IsPointInCone cases (issue #254) - no
	// UWorld needed, mirroring (s-root)'s ComputeLineEndLocation pure-math shape.
	{
		const FVector OwnerLocation = FVector::ZeroVector;
		const FVector FallbackDirection(1.0f, 0.0f, 0.0f);

		// ComputeConeDirection: near cursor still yields the normalized direction (not
		// clamped to any range - direction-only math).
		const FVector NearCursor(500.0f, 0.0f, 0.0f);
		const FVector DirectionForNearCursor = UAbilityCastComponent::ComputeConeDirection(OwnerLocation, NearCursor, FallbackDirection);
		TestTrue(TEXT("(x-snare) A near cursor should yield the normalized direction toward it"),
			DirectionForNearCursor.Equals(FVector(1.0f, 0.0f, 0.0f), 0.01f));

		// ComputeConeDirection: far cursor also yields just the normalized direction.
		const FVector FarCursor(9000.0f, 3000.0f, 0.0f);
		const FVector DirectionForFarCursor = UAbilityCastComponent::ComputeConeDirection(OwnerLocation, FarCursor, FallbackDirection);
		TestTrue(TEXT("(x-snare) A far cursor should yield the normalized direction toward it"),
			DirectionForFarCursor.Equals(FarCursor.GetSafeNormal(), 0.01f));

		// ComputeConeDirection: degenerate (coincident) cursor falls back to FallbackDirection.
		const FVector DegenerateDirection = UAbilityCastComponent::ComputeConeDirection(OwnerLocation, OwnerLocation, FallbackDirection);
		TestTrue(TEXT("(x-snare) A degenerate (coincident) cursor should fall back to FallbackDirection"),
			DegenerateDirection.Equals(FVector(1.0f, 0.0f, 0.0f), 0.01f));

		// ComputeConeDirection: near-degenerate cursor (inside the dead zone but not
		// exactly coincident) also falls back to FallbackDirection.
		const FVector NearDegenerateCursor(0.0f, 5.0f, 0.0f);
		const FVector NearDegenerateDirection = UAbilityCastComponent::ComputeConeDirection(OwnerLocation, NearDegenerateCursor, FallbackDirection);
		TestTrue(TEXT("(x-snare) A cursor inside the dead zone (but not exactly coincident) should also fall back to FallbackDirection"),
			NearDegenerateDirection.Equals(FVector(1.0f, 0.0f, 0.0f), 0.01f));

		// IsPointInCone: dead-centre hit.
		const FVector ConeDirection(1.0f, 0.0f, 0.0f);
		constexpr float HalfAngleDegrees = 37.5f; // half of Snare's 75 degree ConeFullAngleDegrees
		constexpr float RangeUnits = 1200.0f;
		TestTrue(TEXT("(x-snare) A point dead-centre in the cone direction should be in-cone"),
			UAbilityCastComponent::IsPointInCone(FVector(400.0f, 0.0f, 0.0f), OwnerLocation, ConeDirection, HalfAngleDegrees, RangeUnits));

		// IsPointInCone: exactly at the half-angle boundary is still in-cone (inclusive >=).
		const float HalfAngleRadians = FMath::DegreesToRadians(HalfAngleDegrees);
		const FVector AtBoundaryPoint(FMath::Cos(HalfAngleRadians) * 400.0f, FMath::Sin(HalfAngleRadians) * 400.0f, 0.0f);
		TestTrue(TEXT("(x-snare) A point exactly at the half-angle boundary should be in-cone"),
			UAbilityCastComponent::IsPointInCone(AtBoundaryPoint, OwnerLocation, ConeDirection, HalfAngleDegrees, RangeUnits));

		// IsPointInCone: just outside the half-angle boundary is not in-cone.
		const float JustOutsideRadians = FMath::DegreesToRadians(HalfAngleDegrees + 5.0f);
		const FVector JustOutsidePoint(FMath::Cos(JustOutsideRadians) * 400.0f, FMath::Sin(JustOutsideRadians) * 400.0f, 0.0f);
		TestFalse(TEXT("(x-snare) A point just outside the half-angle boundary should not be in-cone"),
			UAbilityCastComponent::IsPointInCone(JustOutsidePoint, OwnerLocation, ConeDirection, HalfAngleDegrees, RangeUnits));

		// IsPointInCone: in-angle but beyond range.
		TestFalse(TEXT("(x-snare) A point in-angle but beyond range should not be in-cone"),
			UAbilityCastComponent::IsPointInCone(FVector(RangeUnits + 50.0f, 0.0f, 0.0f), OwnerLocation, ConeDirection, HalfAngleDegrees, RangeUnits));

		// IsPointInCone: a point exactly coincident with ApexLocation (zero-length
		// ToPoint, undefined angle) is treated as outside the cone - documented edge case.
		TestFalse(TEXT("(x-snare) A point exactly at the apex should be treated as outside the cone"),
			UAbilityCastComponent::IsPointInCone(OwnerLocation, OwnerLocation, ConeDirection, HalfAngleDegrees, RangeUnits));
	}

	// (y-snare) A snared enemy still banks (issue #254 acceptance criterion): casting
	// Snare on an in-cone enemy leaves it IsControlled() (via IHerdable, mirroring
	// KrowdKontrolEnemyBaseHerdableTest.cpp's own pattern), and a direct
	// TransitionToBanked() call still reaches Banked - proving banking eligibility is
	// untouched by the new partial-slow mechanism.
	{
		UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
		if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
		{
			return false;
		}
		APawn* Owner = World->SpawnActor<APawn>();
		UAbilityUnlockComponent* UnlockComponent = NewObject<UAbilityUnlockComponent>(Owner);
		UnlockComponent->RegisterComponent();
		UnlockComponent->NotifyLevelReached(5); // unlocks Snare
		UAbilityCooldownComponent* CooldownComponent = NewObject<UAbilityCooldownComponent>(Owner);
		CooldownComponent->RegisterComponent();
		UAbilityCastComponent* CastComponent = NewObject<UAbilityCastComponent>(Owner);
		CastComponent->RegisterComponent();

		AEnemyBaseTestActor* Enemy = World->SpawnActor<AEnemyBaseTestActor>();
		if (!TestNotNull(TEXT("(y-snare) AEnemyBaseTestActor should spawn"), Enemy))
		{
			return false;
		}
		Enemy->TickCheckDetection(FVector::ZeroVector); // Idle -> Alert
		Enemy->SetActorLocation(FVector(400.0f, 0.0f, 0.0f)); // on the cone centreline, well in range

		const int32 AffectedCount = CastComponent->TryCastConeAbilityTowardLocation(EAbilitySlot::Snare, FVector(500.0f, 0.0f, 0.0f));
		TestEqual(TEXT("(y-snare) The Snare cone cast should affect exactly the one in-cone enemy"), AffectedCount, 1);

		IHerdable* Herdable = Cast<IHerdable>(Enemy);
		if (!TestNotNull(TEXT("(y-snare) AEnemyBaseTestActor should be castable to IHerdable"), Herdable))
		{
			return false;
		}
		TestTrue(TEXT("(y-snare) IsControlled should report true once Controlled by Snare"), Herdable->IsControlled());

		Enemy->TransitionToBanked(); // Controlled -> Banked
		TestEqual(TEXT("(y-snare) The enemy should reach Banked via a direct TransitionToBanked call"),
			static_cast<uint8>(Enemy->GetEnemyState()), static_cast<uint8>(EEnemyState::Banked));
	}

	// (z-snare) Zero enemies in the cone still consumes the cooldown (issue #254) -
	// mirrors (t-root)'s identical "a whiff still commits" contract.
	{
		UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
		if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
		{
			return false;
		}
		APawn* Owner = World->SpawnActor<APawn>();
		UAbilityUnlockComponent* UnlockComponent = NewObject<UAbilityUnlockComponent>(Owner);
		UnlockComponent->RegisterComponent();
		UnlockComponent->NotifyLevelReached(5); // unlocks Snare
		UAbilityCooldownComponent* CooldownComponent = NewObject<UAbilityCooldownComponent>(Owner);
		CooldownComponent->RegisterComponent();
		UAbilityCastComponent* CastComponent = NewObject<UAbilityCastComponent>(Owner);
		CastComponent->RegisterComponent();

		const int32 AffectedCount = CastComponent->TryCastConeAbilityTowardLocation(EAbilitySlot::Snare, FVector(500.0f, 0.0f, 0.0f));
		TestEqual(TEXT("(z-snare) A cone hitting zero enemies should return 0, not -1"), AffectedCount, 0);
		TestTrue(TEXT("(z-snare) A 0-affected cone cast must still consume the cooldown"),
			CooldownComponent->IsOnCooldown(EAbilitySlot::Snare));
	}

	// (aa-fear) TryCastSelfCircleAbility via Fear (issue #253): an enemy inside
	// SelfCircleRadiusUnits of the owner is affected, one outside is not, and one
	// exactly on the boundary is affected too since the radius check is inclusive
	// - mirrors (u-snare)'s in-shape/out-of-shape split and case (m)'s boundary
	// convention (PR #280 review, MEDIUM finding 1: a future <= -> < slip must not
	// silently exclude edge-of-circle enemies with nothing to catch it).
	{
		UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
		if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
		{
			return false;
		}
		APawn* Owner = World->SpawnActor<APawn>();
		UAbilityUnlockComponent* UnlockComponent = NewObject<UAbilityUnlockComponent>(Owner);
		UnlockComponent->RegisterComponent();
		UnlockComponent->NotifyLevelReached(4); // unlocks Fear
		UAbilityCooldownComponent* CooldownComponent = NewObject<UAbilityCooldownComponent>(Owner);
		CooldownComponent->RegisterComponent();
		UAbilityCastComponent* CastComponent = NewObject<UAbilityCastComponent>(Owner);
		CastComponent->RegisterComponent();

		AEnemyBaseTestActor* InCircleEnemy = World->SpawnActor<AEnemyBaseTestActor>();
		AEnemyBaseTestActor* OutOfCircleEnemy = World->SpawnActor<AEnemyBaseTestActor>();
		AEnemyBaseTestActor* OnBoundaryEnemy = World->SpawnActor<AEnemyBaseTestActor>();
		if (!TestNotNull(TEXT("(aa-fear) In-circle AEnemyBaseTestActor should spawn"), InCircleEnemy)
			|| !TestNotNull(TEXT("(aa-fear) Out-of-circle AEnemyBaseTestActor should spawn"), OutOfCircleEnemy)
			|| !TestNotNull(TEXT("(aa-fear) On-boundary AEnemyBaseTestActor should spawn"), OnBoundaryEnemy))
		{
			return false;
		}
		InCircleEnemy->TickCheckDetection(FVector::ZeroVector); // Idle -> Alert
		OutOfCircleEnemy->TickCheckDetection(FVector::ZeroVector); // Idle -> Alert
		OnBoundaryEnemy->TickCheckDetection(FVector::ZeroVector); // Idle -> Alert
		InCircleEnemy->SetActorLocation(FVector(CastComponent->SelfCircleRadiusUnits - 50.0f, 0.0f, 0.0f));
		OutOfCircleEnemy->SetActorLocation(FVector(CastComponent->SelfCircleRadiusUnits + 50.0f, 0.0f, 0.0f));
		OnBoundaryEnemy->SetActorLocation(FVector(CastComponent->SelfCircleRadiusUnits, 0.0f, 0.0f));

		const int32 AffectedCount = CastComponent->TryCastSelfCircleAbility(EAbilitySlot::Fear);
		TestEqual(TEXT("(aa-fear) The in-circle and on-boundary enemies should be affected"), AffectedCount, 2);
		TestEqual(TEXT("(aa-fear) The in-circle enemy should be Controlled"),
			static_cast<uint8>(InCircleEnemy->GetEnemyState()), static_cast<uint8>(EEnemyState::Controlled));
		TestEqual(TEXT("(aa-fear) The out-of-circle enemy should be left untouched"),
			static_cast<uint8>(OutOfCircleEnemy->GetEnemyState()), static_cast<uint8>(EEnemyState::Alert));
		TestEqual(TEXT("(aa-fear) The on-boundary enemy should be Controlled"),
			static_cast<uint8>(OnBoundaryEnemy->GetEnemyState()), static_cast<uint8>(EEnemyState::Controlled));
	}

	// (bb-fear) Multi-target (issue #253): two enemies both inside the circle are both
	// affected in a single cast, mirroring (v-snare)'s multi-target shape.
	{
		UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
		if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
		{
			return false;
		}
		APawn* Owner = World->SpawnActor<APawn>();
		UAbilityUnlockComponent* UnlockComponent = NewObject<UAbilityUnlockComponent>(Owner);
		UnlockComponent->RegisterComponent();
		UnlockComponent->NotifyLevelReached(4); // unlocks Fear
		UAbilityCooldownComponent* CooldownComponent = NewObject<UAbilityCooldownComponent>(Owner);
		CooldownComponent->RegisterComponent();
		UAbilityCastComponent* CastComponent = NewObject<UAbilityCastComponent>(Owner);
		CastComponent->RegisterComponent();

		UAbilityCastAppliedTestListener* Listener = NewObject<UAbilityCastAppliedTestListener>();
		CastComponent->OnAbilityCastApplied.AddDynamic(Listener, &UAbilityCastAppliedTestListener::HandleAbilityCastApplied);

		AEnemyBaseTestActor* FirstEnemy = World->SpawnActor<AEnemyBaseTestActor>();
		AEnemyBaseTestActor* SecondEnemy = World->SpawnActor<AEnemyBaseTestActor>();
		if (!TestNotNull(TEXT("(bb-fear) First AEnemyBaseTestActor should spawn"), FirstEnemy)
			|| !TestNotNull(TEXT("(bb-fear) Second AEnemyBaseTestActor should spawn"), SecondEnemy))
		{
			return false;
		}
		FirstEnemy->TickCheckDetection(FVector::ZeroVector); // Idle -> Alert
		SecondEnemy->TickCheckDetection(FVector::ZeroVector); // Idle -> Alert
		FirstEnemy->SetActorLocation(FVector(100.0f, 0.0f, 0.0f));
		SecondEnemy->SetActorLocation(FVector(-100.0f, 0.0f, 0.0f));

		const int32 AffectedCount = CastComponent->TryCastSelfCircleAbility(EAbilitySlot::Fear);
		TestEqual(TEXT("(bb-fear) Both enemies in the circle should be affected"), AffectedCount, 2);
		TestEqual(TEXT("(bb-fear) The first enemy should be Controlled"),
			static_cast<uint8>(FirstEnemy->GetEnemyState()), static_cast<uint8>(EEnemyState::Controlled));
		TestEqual(TEXT("(bb-fear) The second enemy should be Controlled"),
			static_cast<uint8>(SecondEnemy->GetEnemyState()), static_cast<uint8>(EEnemyState::Controlled));
		TestEqual(TEXT("(bb-fear) OnAbilityCastApplied should have fired exactly twice"), Listener->CallCount, 2);
	}

	// (cc-fear) Zero enemies in the circle still consumes the cooldown (issue #253) -
	// mirrors (z-snare)'s identical "a whiff still commits" contract.
	{
		UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
		if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
		{
			return false;
		}
		APawn* Owner = World->SpawnActor<APawn>();
		UAbilityUnlockComponent* UnlockComponent = NewObject<UAbilityUnlockComponent>(Owner);
		UnlockComponent->RegisterComponent();
		UnlockComponent->NotifyLevelReached(4); // unlocks Fear
		UAbilityCooldownComponent* CooldownComponent = NewObject<UAbilityCooldownComponent>(Owner);
		CooldownComponent->RegisterComponent();
		UAbilityCastComponent* CastComponent = NewObject<UAbilityCastComponent>(Owner);
		CastComponent->RegisterComponent();

		const int32 AffectedCount = CastComponent->TryCastSelfCircleAbility(EAbilitySlot::Fear);
		TestEqual(TEXT("(cc-fear) A self-circle hitting zero enemies should return 0, not -1"), AffectedCount, 0);
		TestTrue(TEXT("(cc-fear) A 0-affected self-circle cast must still consume the cooldown"),
			CooldownComponent->IsOnCooldown(EAbilitySlot::Fear));
	}

	// (dd-fear) Gate failure (locked ability) via TryCastSelfCircleAbility (issue
	// #253): default UAbilityUnlockComponent state only unlocks Stun, so casting Fear
	// must return -1 and change nothing, mirroring case (q).
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
		if (!TestNotNull(TEXT("(dd-fear) AEnemyBaseTestActor should spawn into the test World"), Enemy))
		{
			return false;
		}
		Enemy->TickCheckDetection(FVector::ZeroVector); // Idle -> Alert

		const int32 AffectedCount = CastComponent->TryCastSelfCircleAbility(EAbilitySlot::Fear);
		TestEqual(TEXT("(dd-fear) TryCastSelfCircleAbility for a locked ability should return -1"), AffectedCount, -1);
		TestEqual(TEXT("(dd-fear) A locked-ability self-circle cast should not change the enemy's state"),
			static_cast<uint8>(Enemy->GetEnemyState()), static_cast<uint8>(EEnemyState::Alert));
		TestFalse(TEXT("(dd-fear) A gate-failed self-circle cast must not consume the cooldown"),
			CooldownComponent->IsOnCooldown(EAbilitySlot::Fear));
	}

	// (ee-fear) world-paused gate via TryCastSelfCircleAbility (issue #253), mirroring
	// case (r)'s TryCastThrownAbilityAtLocation coverage.
	{
		UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
		if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
		{
			return false;
		}
		APawn* Owner = World->SpawnActor<APawn>();
		UAbilityUnlockComponent* UnlockComponent = NewObject<UAbilityUnlockComponent>(Owner);
		UnlockComponent->RegisterComponent();
		UnlockComponent->NotifyLevelReached(4); // unlocks Fear
		UAbilityCooldownComponent* CooldownComponent = NewObject<UAbilityCooldownComponent>(Owner);
		CooldownComponent->RegisterComponent();
		UAbilityCastComponent* CastComponent = NewObject<UAbilityCastComponent>(Owner);
		CastComponent->RegisterComponent();

		AEnemyBaseTestActor* Enemy = World->SpawnActor<AEnemyBaseTestActor>();
		if (!TestNotNull(TEXT("(ee-fear) AEnemyBaseTestActor should spawn into the test World"), Enemy))
		{
			return false;
		}
		Enemy->TickCheckDetection(FVector::ZeroVector); // Idle -> Alert

		APlayerState* PauserPlayerState = NewObject<APlayerState>(Owner);
		World->GetWorldSettings()->SetPauserPlayerState(PauserPlayerState);
		if (!TestTrue(TEXT("(ee-fear) World should report paused after SetPauserPlayerState()"), World->IsPaused()))
		{
			return false;
		}

		const int32 AffectedCount = CastComponent->TryCastSelfCircleAbility(EAbilitySlot::Fear);
		TestEqual(TEXT("(ee-fear) TryCastSelfCircleAbility should refuse while the world is paused"), AffectedCount, -1);
		TestFalse(TEXT("(ee-fear) A paused-world self-circle cast must not consume the cooldown"),
			CooldownComponent->IsOnCooldown(EAbilitySlot::Fear));
	}

	// (ff-fear) briefing-visible gate via TryCastSelfCircleAbility (issue #253),
	// mirroring case (s).
	{
		UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
		if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
		{
			return false;
		}
		APawn* Owner = World->SpawnActor<APawn>();
		if (!TestNotNull(TEXT("(ff-fear) APawn should spawn into the test World"), Owner))
		{
			return false;
		}
		UAbilityUnlockComponent* UnlockComponent = NewObject<UAbilityUnlockComponent>(Owner);
		UnlockComponent->RegisterComponent();
		UnlockComponent->NotifyLevelReached(4); // unlocks Fear
		UAbilityCooldownComponent* CooldownComponent = NewObject<UAbilityCooldownComponent>(Owner);
		CooldownComponent->RegisterComponent();
		UAbilityCastComponent* CastComponent = NewObject<UAbilityCastComponent>(Owner);
		CastComponent->RegisterComponent();

		AEnemyBaseTestActor* Enemy = World->SpawnActor<AEnemyBaseTestActor>();
		if (!TestNotNull(TEXT("(ff-fear) AEnemyBaseTestActor should spawn into the test World"), Enemy))
		{
			return false;
		}
		Enemy->TickCheckDetection(FVector::ZeroVector); // Idle -> Alert

		AKrowdKontrolPlayerController* Controller = World->SpawnActor<AKrowdKontrolPlayerController>();
		if (!TestNotNull(TEXT("(ff-fear) Controller should spawn"), Controller))
		{
			return false;
		}
		Controller->Player = NewObject<ULocalPlayer>(GEngine);
		Controller->SetAsLocalPlayerController();
		Controller->Possess(Owner);
		World->AddController(Controller);
		Controller->DispatchBeginPlay();
		if (!TestNotNull(TEXT("(ff-fear) BriefingCardWidgetInstance should exist"), ToRawPtr(Controller->BriefingCardWidgetInstance)))
		{
			return false;
		}

		FLevelBriefingRow Row;
		Row.LevelDisplayName = FText::FromString(TEXT("LEVEL 1"));
		Controller->BriefingCardWidgetInstance->ShowBriefing(Row);

		const int32 AffectedCountWhileVisible = CastComponent->TryCastSelfCircleAbility(EAbilitySlot::Fear);
		TestEqual(TEXT("(ff-fear) TryCastSelfCircleAbility should refuse while the briefing card is visible"), AffectedCountWhileVisible, -1);

		Controller->BriefingCardWidgetInstance->DismissBriefing();
		const int32 AffectedCountAfterDismiss = CastComponent->TryCastSelfCircleAbility(EAbilitySlot::Fear);
		TestEqual(TEXT("(ff-fear) TryCastSelfCircleAbility should succeed again once the briefing card is dismissed"), AffectedCountAfterDismiss, 1);
	}

	// (gg-fear) lockout gate via TryCastSelfCircleAbility (issue #253), mirroring case (t).
	{
		UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
		if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
		{
			return false;
		}
		APawn* Owner = World->SpawnActor<APawn>();
		UAbilityUnlockComponent* UnlockComponent = NewObject<UAbilityUnlockComponent>(Owner);
		UnlockComponent->RegisterComponent();
		UnlockComponent->NotifyLevelReached(4); // unlocks Fear
		UAbilityCooldownComponent* CooldownComponent = NewObject<UAbilityCooldownComponent>(Owner);
		CooldownComponent->RegisterComponent();
		UAbilityLockoutComponent* LockoutComponent = NewObject<UAbilityLockoutComponent>(Owner);
		LockoutComponent->RegisterComponent();
		UAbilityCastComponent* CastComponent = NewObject<UAbilityCastComponent>(Owner);
		CastComponent->RegisterComponent();

		AEnemyBaseTestActor* Enemy = World->SpawnActor<AEnemyBaseTestActor>();
		if (!TestNotNull(TEXT("(gg-fear) AEnemyBaseTestActor should spawn into the test World"), Enemy))
		{
			return false;
		}
		Enemy->TickCheckDetection(FVector::ZeroVector); // Idle -> Alert

		// Locks Fear directly - this tests TryCastSelfCircleAbility's gate, not the
		// lockout component's own trigger logic (see case (h)'s equivalent note).
		LockoutComponent->HandleAbilityCastApplied(EAbilitySlot::Fear, nullptr);
		LockoutComponent->HandlePunishmentTriggered();

		const int32 AffectedCount = CastComponent->TryCastSelfCircleAbility(EAbilitySlot::Fear);
		TestEqual(TEXT("(gg-fear) TryCastSelfCircleAbility should refuse a locked-out ability"), AffectedCount, -1);
		TestFalse(TEXT("(gg-fear) A locked-out self-circle cast must not consume the cooldown"),
			CooldownComponent->IsOnCooldown(EAbilitySlot::Fear));
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
