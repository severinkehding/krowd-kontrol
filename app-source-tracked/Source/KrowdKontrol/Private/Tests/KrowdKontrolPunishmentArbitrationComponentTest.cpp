// Confirms UPunishmentArbitrationComponent (issue #180, PRD "Punishment System (Punishments
// 1 & 2 + arbitration)" REQ-4, resolving closed issue #24) enforces at most one active
// punishment at a time, priority Overcrowd > ability-lock > speed-reduction: Overcrowd
// preempts both lower-priority punishments outright and immediately reverts their effects;
// ability-lock preempts and immediately reverts speed-reduction on their shared
// contact-damage trigger; a pawn with no UAbilityLockoutComponent at all (mirrors
// Paper2DPrototypePawn) still activates speed-reduction normally, unaffected by arbitration.
//
// UOvercrowdDetectionComponent needs a real spawned APawn owner in a real UWorld for its
// own enemy-proximity scan to work, so this test uses
// FAutomationEditorCommonUtils::CreateNewMap() (same as
// KrowdKontrolOvercrowdAudioSubsystemTest.cpp) rather than the bare-NewObject approach
// KrowdKontrolAbilityLockoutComponentTest.cpp uses for everything else here.
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the other
// KrowdKontrol.Unit.* tests.

#include "Misc/AutomationTest.h"
#include "PunishmentArbitrationComponent.h"
#include "AbilityLockoutComponent.h"
#include "SpeedReductionPunishmentComponent.h"
#include "OvercrowdDetectionComponent.h"
#include "EnemyBaseTestActor.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolPunishmentArbitrationComponentTest,
	"KrowdKontrol.Unit.PunishmentArbitrationComponent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolPunishmentArbitrationComponentTest::RunTest(const FString& Parameters)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
	{
		return false;
	}

	APawn* PlayerPawn = World->SpawnActor<APawn>();
	if (!TestNotNull(TEXT("APawn should spawn into the test World"), PlayerPawn))
	{
		return false;
	}
	UOvercrowdDetectionComponent* OvercrowdComponent = NewObject<UOvercrowdDetectionComponent>(PlayerPawn);
	OvercrowdComponent->RegisterComponent();

	AActor* SpeedOwner = World->SpawnActor<AActor>();
	UFloatingPawnMovement* Movement = NewObject<UFloatingPawnMovement>(SpeedOwner);
	Movement->RegisterComponent();
	Movement->MaxSpeed = 1200.0f;
	USpeedReductionPunishmentComponent* SpeedReduction = NewObject<USpeedReductionPunishmentComponent>(SpeedOwner);
	SpeedReduction->RegisterComponent();
	SpeedReduction->MovementComponent = Movement;

	UAbilityLockoutComponent* Lockout = NewObject<UAbilityLockoutComponent>();
	if (!TestNotNull(TEXT("UAbilityLockoutComponent should construct"), Lockout))
	{
		return false;
	}

	UPunishmentArbitrationComponent* Arbitration = NewObject<UPunishmentArbitrationComponent>();
	if (!TestNotNull(TEXT("UPunishmentArbitrationComponent should construct"), Arbitration))
	{
		return false;
	}
	Arbitration->OvercrowdComponent = OvercrowdComponent;
	Arbitration->AbilityLockoutComponent = Lockout;
	Arbitration->SpeedReductionComponent = SpeedReduction;
	OvercrowdComponent->OnPanicOverloadStateChanged.AddDynamic(Arbitration, &UPunishmentArbitrationComponent::HandlePanicOverloadStateChanged);

	// (a) Nothing active, AbilityLockoutComponent present: a trigger locks Stun (the
	// no-cast-yet fallback) and speed-reduction never activates.
	Arbitration->HandlePunishmentTriggered();
	TestTrue(TEXT("Ability-lock should activate (lock Stun) on a plain trigger"), Lockout->IsAbilityLocked(EAbilitySlot::Stun));
	TestFalse(TEXT("Speed-reduction should never activate while AbilityLockoutComponent is present"), SpeedReduction->IsSpeedReductionTimerActive());
	TestEqual(TEXT("MaxSpeed should be untouched"), Movement->MaxSpeed, 1200.0f);

	// (b) "speed-reduction trigger dropped while ability-lock active": a further trigger
	// while ability-lock is still active must still never activate speed-reduction.
	Arbitration->HandlePunishmentTriggered();
	TestFalse(TEXT("A further trigger while ability-lock is active must not activate speed-reduction"), SpeedReduction->IsSpeedReductionTimerActive());

	// (c) "ability-lock preempts speed-reduction": force speed-reduction active directly
	// (bypassing arbitration, simulating a pawn state where it started running before
	// ability-lock existed to preempt it), then a real arbitrated trigger must end it
	// immediately and activate ability-lock instead.
	Lockout->EndAllLockouts();
	SpeedReduction->HandlePunishmentTriggered();
	TestTrue(TEXT("Speed-reduction should be active after the direct force"), SpeedReduction->IsSpeedReductionTimerActive());
	TestEqual(TEXT("MaxSpeed should be reduced after the direct force"), Movement->MaxSpeed, 600.0f);

	Arbitration->HandlePunishmentTriggered();
	TestFalse(TEXT("Ability-lock preempting should end the active speed-reduction immediately"), SpeedReduction->IsSpeedReductionTimerActive());
	TestEqual(TEXT("MaxSpeed should be restored immediately on preemption"), Movement->MaxSpeed, 1200.0f);
	TestTrue(TEXT("Ability-lock should now be active"), Lockout->IsAbilityLocked(EAbilitySlot::Stun));

	// (d) Drive Overcrowd to real Active state (same recipe as
	// KrowdKontrolOvercrowdAudioSubsystemTest.cpp) while ability-lock is active.
	for (int32 Index = 0; Index < OvercrowdComponent->OvercrowdCrowdThreshold; ++Index)
	{
		AEnemyBaseTestActor* Enemy = World->SpawnActor<AEnemyBaseTestActor>();
		if (!TestNotNull(TEXT("AEnemyBaseTestActor should spawn"), Enemy))
		{
			return false;
		}
		Enemy->TickCheckDetection(FVector::ZeroVector); // Idle -> Alert
	}
	OvercrowdComponent->AdvancePanicOverloadState(OvercrowdComponent->OvercrowdUncontrolledDurationSeconds + 10.0f);
	TestEqual(TEXT("Overcrowd should now be Active"),
		static_cast<uint8>(OvercrowdComponent->GetPanicOverloadState()), static_cast<uint8>(EPanicOverloadState::Active));

	// (e) "Overcrowd preempts ability-lock": the Active transition above must have already
	// force-ended the active ability-lock.
	TestFalse(TEXT("Overcrowd activating should end the active ability-lock immediately"), Lockout->IsAbilityLocked(EAbilitySlot::Stun));

	// (f) "ability-lock trigger dropped while Overcrowd active" / "speed-reduction trigger
	// dropped while Overcrowd active": a further trigger while Overcrowd is Active must be
	// a total no-op.
	Arbitration->HandlePunishmentTriggered();
	TestFalse(TEXT("A trigger while Overcrowd is Active must not activate ability-lock"), Lockout->IsAbilityLocked(EAbilitySlot::Stun));
	TestFalse(TEXT("A trigger while Overcrowd is Active must not activate speed-reduction"), SpeedReduction->IsSpeedReductionTimerActive());

	// (g) "Overcrowd preempts speed-reduction": force speed-reduction active directly again
	// (still while Overcrowd is Active - forcing bypasses arbitration entirely, simulating
	// speed-reduction having started before Overcrowd), confirm a fresh Active broadcast
	// ends it immediately.
	SpeedReduction->HandlePunishmentTriggered();
	TestTrue(TEXT("Speed-reduction should be active after the direct force"), SpeedReduction->IsSpeedReductionTimerActive());
	OvercrowdComponent->OnPanicOverloadStateChanged.Broadcast(EPanicOverloadState::Active);
	TestFalse(TEXT("A repeated Overcrowd-Active broadcast should end the active speed-reduction immediately"), SpeedReduction->IsSpeedReductionTimerActive());
	TestEqual(TEXT("MaxSpeed should be restored"), Movement->MaxSpeed, 1200.0f);

	// (h) No AbilityLockoutComponent wired at all (mirrors Paper2DPrototypePawn) - Overcrowd
	// no longer Active, a trigger should activate speed-reduction normally.
	{
		AActor* FallbackOwner = World->SpawnActor<AActor>();
		UFloatingPawnMovement* FallbackMovement = NewObject<UFloatingPawnMovement>(FallbackOwner);
		FallbackMovement->RegisterComponent();
		FallbackMovement->MaxSpeed = 1000.0f;
		USpeedReductionPunishmentComponent* FallbackSpeedReduction = NewObject<USpeedReductionPunishmentComponent>(FallbackOwner);
		FallbackSpeedReduction->RegisterComponent();
		FallbackSpeedReduction->MovementComponent = FallbackMovement;

		UPunishmentArbitrationComponent* FallbackArbitration = NewObject<UPunishmentArbitrationComponent>();
		FallbackArbitration->SpeedReductionComponent = FallbackSpeedReduction;
		// AbilityLockoutComponent and OvercrowdComponent both left nullptr deliberately.

		FallbackArbitration->HandlePunishmentTriggered();
		TestTrue(TEXT("With no AbilityLockoutComponent wired, speed-reduction should activate normally"),
			FallbackSpeedReduction->IsSpeedReductionTimerActive());
		TestEqual(TEXT("MaxSpeed should be reduced"), FallbackMovement->MaxSpeed, 500.0f);
	}

	// (i) BeginPlay()'s own FindComponentByClass+AddDynamic wiring - not the test's
	// manual AddDynamic used for every prior case above - is what actually connects a
	// real in-game pawn's arbitration to Overcrowd. Proves that path specifically.
	{
		APawn* WiredPawn = World->SpawnActor<APawn>();
		UOvercrowdDetectionComponent* WiredOvercrowd = NewObject<UOvercrowdDetectionComponent>(WiredPawn);
		WiredOvercrowd->RegisterComponent();

		UAbilityLockoutComponent* WiredLockout = NewObject<UAbilityLockoutComponent>(WiredPawn);
		WiredLockout->RegisterComponent();

		UPunishmentArbitrationComponent* WiredArbitration = NewObject<UPunishmentArbitrationComponent>(WiredPawn);
		WiredArbitration->AbilityLockoutComponent = WiredLockout;
		WiredArbitration->RegisterComponent();

		WiredPawn->DispatchBeginPlay(); // exercises the real BeginPlay() lookup+bind

		WiredArbitration->HandlePunishmentTriggered();
		TestTrue(TEXT("Ability-lock should activate via the pawn's real trigger path"), WiredLockout->IsAbilityLocked(EAbilitySlot::Stun));

		for (int32 Index = 0; Index < WiredOvercrowd->OvercrowdCrowdThreshold; ++Index)
		{
			AEnemyBaseTestActor* Enemy = World->SpawnActor<AEnemyBaseTestActor>();
			if (!TestNotNull(TEXT("AEnemyBaseTestActor should spawn"), Enemy))
			{
				return false;
			}
			Enemy->TickCheckDetection(FVector::ZeroVector);
		}
		WiredOvercrowd->AdvancePanicOverloadState(WiredOvercrowd->OvercrowdUncontrolledDurationSeconds + 10.0f);

		TestFalse(TEXT("BeginPlay()'s own FindComponentByClass+AddDynamic binding should have force-ended ability-lock when Overcrowd went Active"),
			WiredLockout->IsAbilityLocked(EAbilitySlot::Stun));
	}

	// (j) Overcrowd recovering to Inactive via the real recovery path (landing control on
	// a converged enemy, PRD 08 REQ-2) does not resurrect a punishment that was preempted
	// while Active, and a fresh trigger afterward resumes normal (non-Overcrowd)
	// arbitration - issue #180 AC: a dropped/ended lower-priority activation is never
	// queued, recovery alone must never bring it back.
	{
		APawn* RecoveryPawn = World->SpawnActor<APawn>();
		UOvercrowdDetectionComponent* RecoveryOvercrowd = NewObject<UOvercrowdDetectionComponent>(RecoveryPawn);
		RecoveryOvercrowd->RegisterComponent();

		UAbilityLockoutComponent* RecoveryLockout = NewObject<UAbilityLockoutComponent>();
		if (!TestNotNull(TEXT("UAbilityLockoutComponent should construct"), RecoveryLockout))
		{
			return false;
		}

		UPunishmentArbitrationComponent* RecoveryArbitration = NewObject<UPunishmentArbitrationComponent>();
		if (!TestNotNull(TEXT("UPunishmentArbitrationComponent should construct"), RecoveryArbitration))
		{
			return false;
		}
		RecoveryArbitration->OvercrowdComponent = RecoveryOvercrowd;
		RecoveryArbitration->AbilityLockoutComponent = RecoveryLockout;
		RecoveryOvercrowd->OnPanicOverloadStateChanged.AddDynamic(RecoveryArbitration, &UPunishmentArbitrationComponent::HandlePanicOverloadStateChanged);

		RecoveryArbitration->HandlePunishmentTriggered();
		TestTrue(TEXT("Ability-lock should activate before Overcrowd goes Active"), RecoveryLockout->IsAbilityLocked(EAbilitySlot::Stun));

		TArray<AEnemyBaseTestActor*> RecoveryEnemies;
		for (int32 Index = 0; Index < RecoveryOvercrowd->OvercrowdCrowdThreshold; ++Index)
		{
			AEnemyBaseTestActor* Enemy = World->SpawnActor<AEnemyBaseTestActor>();
			if (!TestNotNull(TEXT("AEnemyBaseTestActor should spawn"), Enemy))
			{
				return false;
			}
			Enemy->TickCheckDetection(FVector::ZeroVector); // Idle -> Alert
			RecoveryEnemies.Add(Enemy);
		}
		RecoveryOvercrowd->AdvancePanicOverloadState(RecoveryOvercrowd->OvercrowdUncontrolledDurationSeconds + 10.0f);
		TestFalse(TEXT("Overcrowd activating should end the active ability-lock"), RecoveryLockout->IsAbilityLocked(EAbilitySlot::Stun));

		// Land control on one converged enemy - the real, only recovery path - then drive
		// AdvancePanicOverloadState again so CurrentState actually flips back to Inactive.
		RecoveryEnemies[0]->ReceiveControl(EAbilitySlot::Stun);
		RecoveryOvercrowd->AdvancePanicOverloadState(1.0f);
		TestEqual(TEXT("Overcrowd should be back to Inactive after landing control on a converged enemy"),
			static_cast<uint8>(RecoveryOvercrowd->GetPanicOverloadState()), static_cast<uint8>(EPanicOverloadState::Inactive));
		TestFalse(TEXT("Recovery to Inactive alone must not reactivate ability-lock"), RecoveryLockout->IsAbilityLocked(EAbilitySlot::Stun));

		RecoveryArbitration->HandlePunishmentTriggered();
		TestTrue(TEXT("A fresh trigger after recovery should resume normal arbitration (ability-lock)"), RecoveryLockout->IsAbilityLocked(EAbilitySlot::Stun));
	}

	// (k) kk.Punishment.LockoutEnabled=0 (issue #181) lets speed-reduction activate instead
	// of being preempted-into-nothing on a pawn with both components wired: arbitration must
	// treat a CVar-disabled AbilityLockoutComponent the same as an unwired one and fall
	// through to SpeedReductionComponent, not still call EndSpeedReduction() followed by a
	// now-gated no-op. This is the arbitration-level regression the component-level CVar
	// tests in KrowdKontrolAbilityLockoutComponentTest.cpp/
	// KrowdKontrolSpeedReductionPunishmentComponentTest.cpp can't catch, since those call
	// HandlePunishmentTriggered() directly on a bare component, bypassing arbitration.
	{
		IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("kk.Punishment.LockoutEnabled"));
		if (!TestNotNull(TEXT("kk.Punishment.LockoutEnabled CVar should be registered"), CVar))
		{
			return false;
		}

		AActor* CVarSpeedOwner = World->SpawnActor<AActor>();
		UFloatingPawnMovement* CVarMovement = NewObject<UFloatingPawnMovement>(CVarSpeedOwner);
		CVarMovement->RegisterComponent();
		CVarMovement->MaxSpeed = 1200.0f;
		USpeedReductionPunishmentComponent* CVarSpeedReduction = NewObject<USpeedReductionPunishmentComponent>(CVarSpeedOwner);
		CVarSpeedReduction->RegisterComponent();
		CVarSpeedReduction->MovementComponent = CVarMovement;

		UAbilityLockoutComponent* CVarLockout = NewObject<UAbilityLockoutComponent>();
		if (!TestNotNull(TEXT("UAbilityLockoutComponent should construct"), CVarLockout))
		{
			return false;
		}

		UPunishmentArbitrationComponent* CVarArbitration = NewObject<UPunishmentArbitrationComponent>();
		if (!TestNotNull(TEXT("UPunishmentArbitrationComponent should construct"), CVarArbitration))
		{
			return false;
		}
		CVarArbitration->AbilityLockoutComponent = CVarLockout;
		CVarArbitration->SpeedReductionComponent = CVarSpeedReduction;

		CVar->Set(0);
		CVarArbitration->HandlePunishmentTriggered();
		TestFalse(TEXT("Ability-lock should not activate while kk.Punishment.LockoutEnabled is 0"),
			CVarLockout->IsAbilityLocked(EAbilitySlot::Stun));
		TestTrue(TEXT("Speed-reduction should activate instead of being cancelled-into-nothing while LockoutEnabled is 0"),
			CVarSpeedReduction->IsSpeedReductionTimerActive());
		TestEqual(TEXT("MaxSpeed should be reduced by the fallback speed-reduction"), CVarMovement->MaxSpeed, 600.0f);

		CVar->Set(1);
		CVarArbitration->HandlePunishmentTriggered();
		TestTrue(TEXT("Restoring the CVar to 1 should let ability-lock preempt speed-reduction normally again"),
			CVarLockout->IsAbilityLocked(EAbilitySlot::Stun));
		TestFalse(TEXT("Ability-lock preempting should end the active speed-reduction immediately, same as case (c)"),
			CVarSpeedReduction->IsSpeedReductionTimerActive());
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
