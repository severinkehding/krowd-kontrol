// Pins AScenicRotatorActor's eased Start->End sweep (intro-scene direction,
// 2026-09-02): endpoint exactness in Once mode, the smoothstep midpoint, and
// PingPong's reversal - transform arithmetic only, matching the actor's
// gameplay-inert contract.
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the
// other KrowdKontrol.Unit.* tests.

#include "Misc/AutomationTest.h"
#include "ScenicRotatorActor.h"
#include "Components/StaticMeshComponent.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolScenicRotatorActorTest,
	"KrowdKontrol.Unit.ScenicRotatorActor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolScenicRotatorActorTest::RunTest(const FString& Parameters)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
	{
		return false;
	}

	AScenicRotatorActor* Rotator = World->SpawnActor<AScenicRotatorActor>();
	if (!TestNotNull(TEXT("AScenicRotatorActor should spawn into the test World"), Rotator))
	{
		return false;
	}
	// The operator's dish brief: yaw -70 -> +100, pitch 61 -> 26.
	Rotator->StartRotation = FRotator(61.f, -70.f, 0.f);
	Rotator->EndRotation = FRotator(26.f, 100.f, 0.f);
	Rotator->DurationSeconds = 7.0f;
	Rotator->PlayMode = EScenicRotatePlayMode::Once;

	// (a) t=0 sits exactly at the start pose.
	Rotator->ApplyProgress(0.0f);
	TestEqual(TEXT("At t=0 the mesh should hold StartRotation (yaw)"),
		Rotator->MeshComponent->GetRelativeRotation().Yaw, -70.0, 0.1);

	// (b) half time: smoothstep(0.5) == 0.5, so exactly the midpoint pose.
	Rotator->Tick(3.5f);
	TestEqual(TEXT("At half duration the sweep should sit at the yaw midpoint"),
		Rotator->MeshComponent->GetRelativeRotation().Yaw, 15.0, 0.5);
	TestEqual(TEXT("At half duration the sweep should sit at the pitch midpoint"),
		Rotator->MeshComponent->GetRelativeRotation().Pitch, 43.5, 0.5);

	// (c) past the end, Once mode holds the end pose exactly.
	Rotator->Tick(10.0f);
	TestEqual(TEXT("Past the duration, Once mode should hold EndRotation (yaw)"),
		Rotator->MeshComponent->GetRelativeRotation().Yaw, 100.0, 0.1);
	TestEqual(TEXT("Past the duration, Once mode should hold EndRotation (pitch)"),
		Rotator->MeshComponent->GetRelativeRotation().Pitch, 26.0, 0.1);

	// (d) PingPong reverses: 1.5 durations in = coming back through the midpoint.
	AScenicRotatorActor* Scanner = World->SpawnActor<AScenicRotatorActor>();
	Scanner->StartRotation = FRotator(0.f, 0.f, 0.f);
	Scanner->EndRotation = FRotator(0.f, 90.f, 0.f);
	Scanner->DurationSeconds = 2.0f;
	Scanner->PlayMode = EScenicRotatePlayMode::PingPong;
	Scanner->Tick(3.0f);
	TestEqual(TEXT("PingPong at 1.5 durations should be back at the midpoint"),
		Scanner->MeshComponent->GetRelativeRotation().Yaw, 45.0, 0.5);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
