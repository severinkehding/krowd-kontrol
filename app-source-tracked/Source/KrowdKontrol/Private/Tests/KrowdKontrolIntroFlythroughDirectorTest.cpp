// Pins AIntroFlythroughDirector's camera-flight math and travel seam
// (intro-scene direction, 2026-09-02): eased path position, focus-point
// facing, the fade trigger time, and the end-of-flight travel request. The
// real OpenLevel() is IsGameWorld()-gated (issue #172 lineage), so in this
// CreateNewMap() world only the bTravelRequested seam flips - the exact
// pattern the player controller's restart tests already rely on.
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the
// other KrowdKontrol.Unit.* tests.

#include "Misc/AutomationTest.h"
#include "IntroFlythroughDirector.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolIntroFlythroughDirectorTest,
	"KrowdKontrol.Unit.IntroFlythroughDirector",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolIntroFlythroughDirectorTest::RunTest(const FString& Parameters)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
	{
		return false;
	}

	AIntroFlythroughDirector* Director = World->SpawnActor<AIntroFlythroughDirector>();
	if (!TestNotNull(TEXT("AIntroFlythroughDirector should spawn into the test World"), Director))
	{
		return false;
	}
	Director->Waypoints = { FVector(0.f, 0.f, 500.f), FVector(1000.f, 0.f, 500.f) };
	Director->FocusPoint = FVector(5000.f, 0.f, 500.f);
	Director->DurationSeconds = 7.0f;
	Director->FadeStartSeconds = 6.1f;

	// (a) half time: smoothstep(0.5) == 0.5 - exactly halfway along the path,
	// facing the focus point (dead ahead on +X here), fade not yet started.
	Director->Tick(3.5f);
	TestEqual(TEXT("At half duration the camera should sit halfway along the path"),
		Director->GetActorLocation(), FVector(500.f, 0.f, 500.f));
	TestEqual(TEXT("The camera should face the focus point"),
		Director->GetActorRotation().Yaw, 0.0, 0.1);
	TestFalse(TEXT("The fade must not start before FadeStartSeconds"), Director->bFadeStarted);
	TestFalse(TEXT("No travel request before the flight ends"), Director->bTravelRequested);

	// (b) crossing FadeStartSeconds arms the fade exactly once.
	Director->Tick(2.7f); // elapsed 6.2
	TestTrue(TEXT("The fade should have started after FadeStartSeconds"), Director->bFadeStarted);
	TestFalse(TEXT("Still no travel request before DurationSeconds"), Director->bTravelRequested);

	// (c) the end of the flight parks at the final waypoint and requests travel
	// (the real OpenLevel is IsGameWorld()-gated and stays un-run here).
	Director->Tick(1.0f); // elapsed 7.2
	TestEqual(TEXT("The flight should end exactly at the final waypoint"),
		Director->GetActorLocation(), FVector(1000.f, 0.f, 500.f));
	TestTrue(TEXT("Reaching DurationSeconds should request the level travel"), Director->bTravelRequested);

	// (d) further ticks are inert - the director never re-travels.
	Director->Tick(5.0f);
	TestEqual(TEXT("Post-travel ticks must not move the camera"),
		Director->GetActorLocation(), FVector(1000.f, 0.f, 500.f));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
