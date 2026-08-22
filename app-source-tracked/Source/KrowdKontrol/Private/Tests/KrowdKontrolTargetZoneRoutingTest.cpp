// Confirms ATargetZone's routing metadata fields (bRequiresRouting,
// RoutingObstacleActor - issue #83) default correctly, round-trip after being set,
// and do not change OnActorBanked's existing firing conditions (issue #80). This is
// level-authoring metadata only in this issue's scope - no AI/pathfinding logic reads
// either field - so the banking-regression checks below are a subset of
// KrowdKontrolTargetZoneTest.cpp's own scenarios (a)/(b), not a full re-test.
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the other
// KrowdKontrol.Unit.* tests.

#include "Misc/AutomationTest.h"
#include "TargetZone.h"
#include "TargetZoneTestActor.h"
#include "TargetZoneBankedTestListener.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolTargetZoneRoutingTest,
	"KrowdKontrol.Unit.TargetZoneRouting",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolTargetZoneRoutingTest::RunTest(const FString& Parameters)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
	{
		return false;
	}
	World->InitializeActorsForPlay(FURL());
	World->SetBegunPlay(true);

	ATargetZone* Zone = World->SpawnActor<ATargetZone>();
	if (!TestNotNull(TEXT("ATargetZone should spawn into the test World"), Zone))
	{
		return false;
	}

	// (a) Defaults.
	TestFalse(TEXT("bRequiresRouting should default to false"), Zone->bRequiresRouting);
	TestNull(TEXT("RoutingObstacleActor should default to nullptr"), Zone->RoutingObstacleActor.Get());

	// (b) Round-trip.
	AActor* Obstacle = World->SpawnActor<AActor>(FVector(500.f, 500.f, 0.f), FRotator::ZeroRotator);
	if (!TestNotNull(TEXT("Obstacle actor should spawn into the test World"), Obstacle))
	{
		return false;
	}
	Zone->bRequiresRouting = true;
	Zone->RoutingObstacleActor = Obstacle;
	TestTrue(TEXT("bRequiresRouting should read back as true after being set"), Zone->bRequiresRouting);
	TestEqual(TEXT("RoutingObstacleActor should read back as the obstacle actor after being set"),
		Zone->RoutingObstacleActor.Get(), Obstacle);

	// (c) Banking non-regression: same firing conditions as KrowdKontrolTargetZoneTest.cpp
	// with routing fields now set.
	Zone->ZoneColourTag = FName(TEXT("Purple"));
	UTargetZoneBankedTestListener* Listener = NewObject<UTargetZoneBankedTestListener>();
	Zone->OnActorBanked.AddDynamic(Listener, &UTargetZoneBankedTestListener::HandleActorBanked);

	ATargetZoneTestActor* MatchedActor = World->SpawnActor<ATargetZoneTestActor>(FVector(1000.f, 0.f, 0.f), FRotator::ZeroRotator);
	if (!TestNotNull(TEXT("Matched actor should spawn into the test World"), MatchedActor))
	{
		return false;
	}
	MatchedActor->SetControlled(true);
	MatchedActor->SetHerdColourTag(FName(TEXT("Purple")));
	MatchedActor->SetActorLocation(FVector::ZeroVector, /*bSweep=*/true);
	TestEqual(TEXT("OnActorBanked should still fire once for a controlled, colour-matched actor with routing fields set"),
		Listener->CallCount, 1);
	TestEqual(TEXT("OnActorBanked should still report the correct actor with routing fields set"),
		Listener->LastBankedActor.Get(), static_cast<AActor*>(MatchedActor));

	ATargetZoneTestActor* MismatchedActor = World->SpawnActor<ATargetZoneTestActor>(FVector(1000.f, 0.f, 0.f), FRotator::ZeroRotator);
	if (!TestNotNull(TEXT("Mismatched actor should spawn into the test World"), MismatchedActor))
	{
		return false;
	}
	// Type-keyed acceptance (operator ruling 2026-08-22): colour no longer gates,
	// so the rejection case is a typed zone vs an actor with no matching type.
	Zone->bAcceptAnyEnemyType = false;
	Zone->ZoneEnemyType = EEnemyType::RU_NNR;
	MismatchedActor->SetControlled(true);
	MismatchedActor->SetHerdColourTag(FName(TEXT("Teal")));
	MismatchedActor->SetActorLocation(FVector::ZeroVector, /*bSweep=*/true);
	TestEqual(TEXT("A typed zone should still reject a type-less controlled actor with routing fields set"),
		Listener->CallCount, 1);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
