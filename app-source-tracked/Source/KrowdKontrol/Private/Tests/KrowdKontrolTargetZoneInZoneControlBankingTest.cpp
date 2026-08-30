// Regression test for the in-zone control banking gap (2026-08-30 operator
// playtest): ATargetZone banked ONLY via OnComponentBeginOverlap, so an enemy
// ALREADY standing inside the zone when it became Controlled fired no new
// overlap event and silently never banked. Latent since the zone shipped
// (issue #80), unreachable until issue #361's wake-and-control made stunning a
// stationary in-zone robot possible. The fix is two-sided and this test pins
// both: ATargetZone::EvaluateHerdableForBanking() exposes the acceptance rules
// outside overlap events, and AEnemyBase::ReceiveControl() re-evaluates every
// zone the enemy currently overlaps once its Controlled state is fully set up.
//
// Uses the same real-physics-overlap world setup as KrowdKontrolTargetZoneTest
// (InitializeActorsForPlay + SetBegunPlay - see that file's comment for why
// both are required), because the regression is precisely about overlap state
// that exists without a begin-overlap event ever reaching the zone.
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the
// other KrowdKontrol.Unit.* tests.

#include "Misc/AutomationTest.h"
#include "TargetZone.h"
#include "RunnerEnemy.h"
#include "AbilitySlot.h"
#include "TargetZoneBankedTestListener.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolTargetZoneInZoneControlBankingTest,
	"KrowdKontrol.Unit.TargetZoneInZoneControlBanking",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolTargetZoneInZoneControlBankingTest::RunTest(const FString& Parameters)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
	{
		return false;
	}

	// Required for real overlap state - see the file comment above.
	World->InitializeActorsForPlay(FURL());
	World->SetBegunPlay(true);

	ATargetZone* Zone = World->SpawnActor<ATargetZone>();
	if (!TestNotNull(TEXT("ATargetZone should spawn into the test World"), Zone))
	{
		return false;
	}

	UTargetZoneBankedTestListener* Listener = NewObject<UTargetZoneBankedTestListener>();
	Zone->OnActorBanked.AddDynamic(Listener, &UTargetZoneBankedTestListener::HandleActorBanked);

	// Spawn the enemy directly INSIDE the zone. This registers overlap state on
	// spawn but - the regression's precondition - the enemy is not Controlled, so
	// the begin-overlap evaluation correctly declines to bank it.
	ARunnerEnemy* Enemy = World->SpawnActor<ARunnerEnemy>(FVector::ZeroVector, FRotator::ZeroRotator);
	if (!TestNotNull(TEXT("ARunnerEnemy should spawn into the test World"), Enemy))
	{
		return false;
	}
	if (!TestTrue(TEXT("Precondition: the enemy must physically overlap the zone"),
		Enemy->IsOverlappingActor(Zone)))
	{
		return false;
	}
	TestEqual(TEXT("An uncontrolled enemy inside the zone must not bank on spawn overlap"),
		Listener->CallCount, 0);

	// ReceiveControl() is a no-op outside Alert/Attack (EnemyBase.cpp's state
	// guard) and a fresh-spawned enemy is Idle - reach Alert through the same
	// private friend-granted seam the other enemy-state tests use.
	Enemy->AdvanceToAlert();

	// The regression moment: control lands while the enemy is already inside.
	// No new overlap event exists to fire - banking must happen anyway.
	Enemy->ReceiveControl(EAbilitySlot::Stun);

	TestEqual(TEXT("Becoming Controlled while already inside the zone should bank exactly once"),
		Listener->CallCount, 1);
	TestEqual(TEXT("The banked actor should be the in-zone enemy"),
		static_cast<AActor*>(Listener->LastBankedActor), static_cast<AActor*>(Enemy));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
