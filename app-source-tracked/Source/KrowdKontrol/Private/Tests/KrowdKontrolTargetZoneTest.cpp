// Confirms ATargetZone (issue #80) fires OnActorBanked exactly when an overlapping
// IHerdable actor is both controlled and colour-matched, via a real physical
// UBoxComponent overlap - not a bypassed test hook. Uses ATargetZoneTestActor's own
// collision volume swept into the zone with SetActorLocation(..., bSweep=true).
//
// Deviation from the investigation artifact: the artifact assumed sweep-driven
// overlap needs "no PIE tick and no world BeginPlay()", matching this module's other
// tests (which need no collision at all). That does not hold for a real physics
// overlap. Two gates in Engine/Private/Components/PrimitiveComponent.cpp block a
// BeginOverlap event for an actor spawned into CreateNewMap()'s bare editor world,
// both confirmed empirically (a manual World->OverlapMultiByChannel() query found the
// correct physics hit throughout, proving collision itself was never the problem):
//   1. ShouldIgnoreOverlapResult() discards a hit against an actor whose
//      IsActorInitialized() is false - fixed by World->InitializeActorsForPlay().
//   2. UPrimitiveComponent::BeginComponentOverlap() only broadcasts
//      OnComponentBeginOverlap if World->HasBegunPlay() is true - and
//      UWorld::BeginPlay() alone does not set that; it only flips via
//      GetAuthGameMode()->StartPlay(), and CreateNewMap()'s world has no GameMode.
//      Fixed with World->SetBegunPlay(true) directly.
// Both calls are made once, up front, before any actor is spawned, so every actor
// spawned afterward is auto-initialized and auto-begins-play via the engine's normal
// AActor::PostActorConstruction() flow - no per-actor workaround needed. Neither is a
// broader change to this module's test conventions; both are narrow, minimal calls
// needed specifically because this is the first test in this module driving a real
// physics overlap. See implementation.md.
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the other
// KrowdKontrol.Unit.* tests.

#include "Misc/AutomationTest.h"
#include "TargetZone.h"
#include "TargetZoneTestActor.h"
#include "NonHerdableTestActor.h"
#include "TargetZoneBankedTestListener.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolTargetZoneTest,
	"KrowdKontrol.Unit.TargetZone",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolTargetZoneTest::RunTest(const FString& Parameters)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
	{
		return false;
	}

	// Required for real overlap events - see the file comment above.
	World->InitializeActorsForPlay(FURL());
	World->SetBegunPlay(true);

	ATargetZone* Zone = World->SpawnActor<ATargetZone>();
	if (!TestNotNull(TEXT("ATargetZone should spawn into the test World"), Zone))
	{
		return false;
	}
	Zone->ZoneColourTag = FName(TEXT("Purple"));

	UTargetZoneBankedTestListener* Listener = NewObject<UTargetZoneBankedTestListener>();
	Zone->OnActorBanked.AddDynamic(Listener, &UTargetZoneBankedTestListener::HandleActorBanked);

	// (a) matched + controlled fires exactly once with the correct actor.
	ATargetZoneTestActor* MatchedActor = World->SpawnActor<ATargetZoneTestActor>(FVector(1000.f, 0.f, 0.f), FRotator::ZeroRotator);
	if (!TestNotNull(TEXT("Matched actor should spawn into the test World"), MatchedActor))
	{
		return false;
	}
	MatchedActor->SetControlled(true);
	MatchedActor->SetHerdColourTag(FName(TEXT("Purple")));
	MatchedActor->SetActorLocation(FVector::ZeroVector, /*bSweep=*/true);
	TestEqual(TEXT("OnActorBanked should fire once for a controlled, colour-matched actor"),
		Listener->CallCount, 1);
	TestEqual(TEXT("OnActorBanked should report the correct actor"),
		Listener->LastBankedActor.Get(), static_cast<AActor*>(MatchedActor));

	// (b) colour mismatch never fires.
	ATargetZoneTestActor* MismatchedActor = World->SpawnActor<ATargetZoneTestActor>(FVector(1000.f, 0.f, 0.f), FRotator::ZeroRotator);
	if (!TestNotNull(TEXT("Mismatched actor should spawn into the test World"), MismatchedActor))
	{
		return false;
	}
	MismatchedActor->SetControlled(true);
	MismatchedActor->SetHerdColourTag(FName(TEXT("Teal")));
	MismatchedActor->SetActorLocation(FVector::ZeroVector, /*bSweep=*/true);
	TestEqual(TEXT("OnActorBanked should not fire on a colour mismatch"),
		Listener->CallCount, 1);

	// (c) uncontrolled never fires.
	ATargetZoneTestActor* UncontrolledActor = World->SpawnActor<ATargetZoneTestActor>(FVector(1000.f, 0.f, 0.f), FRotator::ZeroRotator);
	if (!TestNotNull(TEXT("Uncontrolled actor should spawn into the test World"), UncontrolledActor))
	{
		return false;
	}
	UncontrolledActor->SetHerdColourTag(FName(TEXT("Purple")));
	UncontrolledActor->SetActorLocation(FVector::ZeroVector, /*bSweep=*/true);
	TestEqual(TEXT("OnActorBanked should not fire for an uncontrolled actor"),
		Listener->CallCount, 1);

	// (d) a non-IHerdable actor overlapping the zone never fires the delegate. This is
	// the highest-traffic real-level case (room geometry, doors, props all outnumber
	// IHerdable actors) since ZoneCollisionComponent uses OverlapAllDynamic and will
	// physically overlap with any dynamic actor, not just IHerdable ones.
	ANonHerdableTestActor* PlainActor = World->SpawnActor<ANonHerdableTestActor>(FVector(1000.f, 0.f, 0.f), FRotator::ZeroRotator);
	if (!TestNotNull(TEXT("Non-herdable actor should spawn into the test World"), PlainActor))
	{
		return false;
	}
	PlainActor->SetActorLocation(FVector::ZeroVector, /*bSweep=*/true);
	TestEqual(TEXT("OnActorBanked should not fire for a non-IHerdable actor overlap"),
		Listener->CallCount, 1);

	// (e) an unconfigured zone (default ZoneColourTag == NAME_None) matches an
	// unconfigured actor (default HerdColourTag == NAME_None) by design - pins the
	// documented default-match behavior rather than leaving it to a comment alone.
	// Also confirms cross-instance isolation: this second zone's broadcast must not
	// reach the first zone's Listener (constructor-time delegate binds are per-instance).
	ATargetZone* UnconfiguredZone = World->SpawnActor<ATargetZone>(FVector(2000.f, 0.f, 0.f), FRotator::ZeroRotator);
	if (!TestNotNull(TEXT("Unconfigured zone should spawn into the test World"), UnconfiguredZone))
	{
		return false;
	}

	UTargetZoneBankedTestListener* UnconfiguredListener = NewObject<UTargetZoneBankedTestListener>();
	UnconfiguredZone->OnActorBanked.AddDynamic(UnconfiguredListener, &UTargetZoneBankedTestListener::HandleActorBanked);

	ATargetZoneTestActor* DefaultColourActor = World->SpawnActor<ATargetZoneTestActor>(FVector(3000.f, 0.f, 0.f), FRotator::ZeroRotator);
	if (!TestNotNull(TEXT("Default-colour actor should spawn into the test World"), DefaultColourActor))
	{
		return false;
	}
	DefaultColourActor->SetControlled(true);
	// HerdColourTag left at its NAME_None default - deliberately not calling SetHerdColourTag().
	DefaultColourActor->SetActorLocation(FVector(2000.f, 0.f, 0.f), /*bSweep=*/true);
	TestEqual(TEXT("An unconfigured zone should bank an unconfigured, controlled actor (documented default-match behavior)"),
		UnconfiguredListener->CallCount, 1);
	TestEqual(TEXT("A second zone's broadcast should not reach the first zone's listener (cross-instance isolation)"),
		Listener->CallCount, 1);

	// (f) re-entry: an already-banked actor that leaves and re-enters the zone fires
	// OnActorBanked again. Pins current (undefined-by-issue-scope) behavior so a future
	// change to it is a deliberate decision, not a silent regression - see
	// NotifyEnemyBanked() integration note in TargetZone.h/.cpp for the out-of-scope
	// consumer that will eventually need this contract pinned.
	MatchedActor->SetActorLocation(FVector(1000.f, 0.f, 0.f), /*bSweep=*/true);
	MatchedActor->SetActorLocation(FVector::ZeroVector, /*bSweep=*/true);
	TestEqual(TEXT("OnActorBanked should fire again when an already-banked actor re-enters the zone"),
		Listener->CallCount, 2);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
