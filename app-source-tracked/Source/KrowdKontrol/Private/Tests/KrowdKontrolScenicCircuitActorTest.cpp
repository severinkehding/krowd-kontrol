// Pins AScenicCircuitActor's cosmetic path-following (operator art pass,
// 2026-08-30): constant-speed advance along a closed waypoint loop, loop
// wrap-around, start-offset staggering, travel-direction facing with the
// per-mesh yaw correction, and the no-movement degenerate case. The actor is
// deliberately gameplay-inert, so this is transform-arithmetic coverage only -
// there is no state machine or interaction surface to exercise.
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the
// other KrowdKontrol.Unit.* tests.

#include "Misc/AutomationTest.h"
#include "ScenicCircuitActor.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolScenicCircuitActorTest,
	"KrowdKontrol.Unit.ScenicCircuitActor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolScenicCircuitActorTest::RunTest(const FString& Parameters)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
	{
		return false;
	}

	AScenicCircuitActor* Mover = World->SpawnActor<AScenicCircuitActor>();
	if (!TestNotNull(TEXT("AScenicCircuitActor should spawn into the test World"), Mover))
	{
		return false;
	}

	// 400-unit square loop on the ground plane.
	Mover->Waypoints = {
		FVector(0.f, 0.f, 0.f), FVector(400.f, 0.f, 0.f),
		FVector(400.f, 400.f, 0.f), FVector(0.f, 400.f, 0.f)
	};
	Mover->SpeedUnitsPerSecond = 100.0f;
	Mover->BobAmplitudeUnits = 0.0f;

	// (a) mid-first-segment: 1s at 100u/s from the loop start.
	Mover->DistanceAlongPath = 0.0f;
	Mover->Tick(1.0f);
	TestEqual(TEXT("After 1s the mover should sit 100 units along the first segment"),
		Mover->GetActorLocation(), FVector(100.f, 0.f, 0.f));
	TestEqual(TEXT("Facing should follow the +X travel direction"),
		Mover->GetActorRotation().Yaw, 0.0, 0.1);

	// (b) segment crossover: 4s more lands 100 units into the second (+Y) segment.
	Mover->Tick(4.0f);
	TestEqual(TEXT("After 5s total the mover should sit 100 units up the second segment"),
		Mover->GetActorLocation(), FVector(400.f, 100.f, 0.f));
	TestEqual(TEXT("Facing should follow the +Y travel direction"),
		Mover->GetActorRotation().Yaw, 90.0, 0.1);

	// (c) loop wrap: a full 1600-unit lap returns to the same spot.
	Mover->Tick(16.0f);
	TestEqual(TEXT("A full lap later the mover should be back at the same point"),
		Mover->GetActorLocation(), FVector(400.f, 100.f, 0.f));

	// (d) yaw offset for meshes whose nose is not +X. 45 not 90: travel yaw is 90
	// here and 90+90=180 sits exactly on FRotator's normalisation seam, where the
	// engine may legally report -180.
	Mover->YawOffsetDegrees = 45.0f;
	Mover->Tick(0.01f);
	TestEqual(TEXT("YawOffsetDegrees should add to the travel-direction yaw"),
		Mover->GetActorRotation().Yaw, 135.0, 0.5);

	// (e) start-offset staggering positions a fresh mover along the loop.
	AScenicCircuitActor* Staggered = World->SpawnActor<AScenicCircuitActor>();
	Staggered->Waypoints = Mover->Waypoints;
	Staggered->StartOffsetAlongPath = 400.0f;
	Staggered->DistanceAlongPath = Staggered->StartOffsetAlongPath;
	Staggered->ApplyPathPosition();
	TestEqual(TEXT("StartOffsetAlongPath should place the mover that far around the loop"),
		Staggered->GetActorLocation(), FVector(400.f, 0.f, 0.f));

	// (f) degenerate: fewer than 2 waypoints must not move or divide by zero.
	AScenicCircuitActor* Parked = World->SpawnActor<AScenicCircuitActor>(FVector(7.f, 8.f, 9.f), FRotator::ZeroRotator);
	Parked->Waypoints = { FVector(1.f, 2.f, 3.f) };
	Parked->Tick(1.0f);
	TestEqual(TEXT("A mover with fewer than 2 waypoints must stay parked where placed"),
		Parked->GetActorLocation(), FVector(7.f, 8.f, 9.f));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
