// Confirms ADualZoneBoss (issue #52, PRD 04 Mid-boss 3): (1) it marks itself split
// and reaches Armed within the first moment of BeginPlay() (well inside the issue's
// 10 second AC window), (2) balanced banking across ZoneA/ZoneB - including the exact
// imbalance-equals-threshold boundary - never triggers Enrage, (3) banking driven
// strictly past EnrageImbalanceThreshold in one zone does trigger Enrage, (4) the
// two zones' banked counts are tracked independently, not combined into one tally,
// (5) ZoneB leading the imbalance enrages too, pinning the abs() check's symmetry,
// and (6) an unwired ZoneA/ZoneB never crashes and never receives banked events -
// BeginPlay()'s null-guard branches for a misconfigured level placement.
//
// Deviation from the investigation artifact: the plan's Mandatory Reading section
// claimed TargetZoneBankedTestListener.cpp already established that
// OnActorBanked.Broadcast(Actor) can be invoked directly with no world setup - it
// does not; that listener is only ever driven through a real physics overlap in
// KrowdKontrolTargetZoneTest.cpp. Broadcasting a dynamic multicast delegate straight
// to an AActor target (ADualZoneBoss's handlers, as opposed to a plain UObject
// listener) silently no-ops unless World->AreActorsInitialized() is true -
// AActor::ProcessEvent gates reflection-dispatched calls on it. This is the same
// underlying "actor not initialized for play" gotcha KrowdKontrolTargetZoneTest.cpp
// already documents for physics overlap events, just reached via a different call
// path (delegate broadcast instead of component overlap). Fixed the same way that
// file does: World->InitializeActorsForPlay(FURL()) up front, before spawning any
// actors. SetBegunPlay(true) is intentionally NOT called here (unlike that file) -
// this test drives BeginPlay explicitly via DispatchBeginPlay() after assigning
// ZoneA/ZoneB, and flipping bBegunPlay would make SpawnActor() auto-invoke BeginPlay
// immediately, before those EditInstanceOnly references are assigned.
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the other
// KrowdKontrol.Unit.* tests.

#include "Misc/AutomationTest.h"
#include "DualZoneBoss.h"
#include "TargetZone.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolDualZoneBossTest,
	"KrowdKontrol.Unit.DualZoneBoss",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolDualZoneBossTest::RunTest(const FString& Parameters)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
	{
		return false;
	}

	// Required for ADualZoneBoss's own dynamic-delegate-bound handlers to actually
	// fire on Broadcast() - see the file comment above.
	World->InitializeActorsForPlay(FURL());

	ATargetZone* ZoneA = World->SpawnActor<ATargetZone>();
	ATargetZone* ZoneB = World->SpawnActor<ATargetZone>();
	if (!TestNotNull(TEXT("ZoneA should spawn into the test World"), ZoneA)
		|| !TestNotNull(TEXT("ZoneB should spawn into the test World"), ZoneB))
	{
		return false;
	}

	ADualZoneBoss* Boss = World->SpawnActor<ADualZoneBoss>();
	if (!TestNotNull(TEXT("ADualZoneBoss should spawn into the test World"), Boss))
	{
		return false;
	}

	Boss->ZoneA = ZoneA;
	Boss->ZoneB = ZoneB;
	Boss->EnrageImbalanceThreshold = 3;

	Boss->DispatchBeginPlay();

	// (a) Split + Armed within the first moment of BeginPlay() - pins AC #1 and AC #3.
	TestTrue(TEXT("ADualZoneBoss should mark itself split by BeginPlay()"), Boss->IsSplit());
	TestEqual(TEXT("ADualZoneBoss should be Armed immediately after BeginPlay()"),
		static_cast<uint8>(Boss->GetBossState()), static_cast<uint8>(EBossState::Armed));

	AActor* DummyActor = World->SpawnActor<AActor>();
	if (!TestNotNull(TEXT("A dummy AActor should spawn into the test World"), DummyActor))
	{
		return false;
	}

	// (b) Balanced banking - including the exact-threshold boundary - never enrages.
	ZoneA->OnActorBanked.Broadcast(DummyActor); // A=1, B=0
	ZoneB->OnActorBanked.Broadcast(DummyActor); // A=1, B=1
	ZoneA->OnActorBanked.Broadcast(DummyActor); // A=2, B=1
	ZoneB->OnActorBanked.Broadcast(DummyActor); // A=2, B=2
	TestFalse(TEXT("Boss should not be enraged while zones stay balanced"), Boss->IsEnraged());

	ZoneA->OnActorBanked.Broadcast(DummyActor); // A=3, B=2
	ZoneA->OnActorBanked.Broadcast(DummyActor); // A=4, B=2
	TestEqual(TEXT("BankedCountA should track only ZoneA's broadcasts"), Boss->GetBankedCountA(), 4);
	TestEqual(TEXT("BankedCountB should track only ZoneB's broadcasts"), Boss->GetBankedCountB(), 2);
	// Imbalance is exactly 2 here - still not enraged.
	TestFalse(TEXT("Boss should not be enraged while imbalance is below the threshold"), Boss->IsEnraged());

	ZoneA->OnActorBanked.Broadcast(DummyActor); // A=5, B=2 -> imbalance exactly at threshold (3)
	TestFalse(TEXT("Boss should not be enraged when imbalance exactly equals the threshold"), Boss->IsEnraged());

	// (c) Pushing the imbalance strictly past the threshold triggers Enrage.
	ZoneA->OnActorBanked.Broadcast(DummyActor); // A=6, B=2 -> imbalance 4, past threshold
	TestTrue(TEXT("Boss should be enraged once imbalance strictly exceeds the threshold"), Boss->IsEnraged());

	TestEqual(TEXT("BankedCountA should equal the total ZoneA broadcasts"), Boss->GetBankedCountA(), 6);
	TestEqual(TEXT("BankedCountB should equal the total ZoneB broadcasts"), Boss->GetBankedCountB(), 2);

	// (d) ZoneB leading the imbalance also enrages - pins symmetry of the
	// FMath::Abs(BankedCountA - BankedCountB) check, not just the ZoneA-leading
	// direction exercised above.
	ADualZoneBoss* BBoss = World->SpawnActor<ADualZoneBoss>();
	if (!TestNotNull(TEXT("A second ADualZoneBoss should spawn into the test World"), BBoss))
	{
		return false;
	}
	// Deliberately reuses the same ZoneA/ZoneB spawned above rather than spawning a
	// fresh pair - this section only needs a second, independent ADualZoneBoss
	// listening on OnActorBanked broadcasts to pin the abs() check's symmetry, not a
	// second pair of zones, and Boss's own imbalance state above is already past its
	// assertions by this point so BBoss's independent BankedCountA/BankedCountB
	// tally doesn't interact with it.
	BBoss->ZoneA = ZoneA;
	BBoss->ZoneB = ZoneB;
	BBoss->EnrageImbalanceThreshold = 3;
	BBoss->DispatchBeginPlay();
	for (int32 i = 0; i < 4; ++i)
	{
		ZoneB->OnActorBanked.Broadcast(DummyActor);
	}
	TestTrue(TEXT("Boss should be enraged when ZoneB leads past the threshold"), BBoss->IsEnraged());

	// (e) BeginPlay()'s null-guard branches for an unwired ZoneA/ZoneB never crash and
	// never fire handlers - the exact path that protects against a level designer
	// forgetting to wire one of these EditInstanceOnly references.
	ADualZoneBoss* UnwiredBoss = World->SpawnActor<ADualZoneBoss>();
	if (!TestNotNull(TEXT("A third ADualZoneBoss should spawn into the test World"), UnwiredBoss))
	{
		return false;
	}
	// ZoneA/ZoneB intentionally left unassigned.
	UnwiredBoss->DispatchBeginPlay();
	TestEqual(TEXT("An unwired boss's BankedCountA should stay 0 with no ZoneA bound"),
		UnwiredBoss->GetBankedCountA(), 0);
	TestEqual(TEXT("An unwired boss's BankedCountB should stay 0 with no ZoneB bound"),
		UnwiredBoss->GetBankedCountB(), 0);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
