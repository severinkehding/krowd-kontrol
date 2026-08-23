// Adds KrowdKontrol.PIE.SerializedPlacedActorHealth (PRD
// docs/prd-functional-pie-tests.md REQ-3 item 2, issue #239) - the second scenario
// test in the KrowdKontrol.PIE.* group (issue #236) that
// KrowdKontrolPIESessionTest.cpp stood up.
//
// APlaceholderTargetZoneActor::EnsureBeaconHierarchy() (issue #199) self-heals actors
// placed in a level before issue #190's component-hierarchy change, but one of its two
// entry points - AActor::PostInitializeComponents - never runs for actors deserialized
// into an editor world (PlaceholderTargetZoneActor.cpp:94-101's own comment), only for
// a real game-time init such as PIE or a packaged build. Every existing
// KrowdKontrol.Unit.* test runs in a CreateNewMap()/LoadMap() world that never starts
// play, so no gate today actually exercises that path, or proves a placed marker (and
// the ATargetZone that ARoomActor::EnsureBankingZonesWired() self-heals onto it during
// a real ARoomActor::BeginPlay() tick) ends up somewhere other than the world origin.
//
// This test opens each shipped map (L_Level01, L_Level02) in a real PIE session via
// AutomationOpenMap, and for every placed APlaceholderTargetZoneActor asserts: the
// corrected component hierarchy (root = TargetZoneRootComponent; mesh/column siblings
// under it; light atop the column), a non-origin world location, and that its
// self-healed, attached ATargetZone spawned at that same position. No lifecycle method
// (PostInitializeComponents, EnsureBeaconHierarchy, EnsureBankingZonesWired) is ever
// called directly - all healed/spawned state is reached only through the real PIE
// session's own tick flow, exactly as the PRD's "no direct lifecycle calls" rule
// requires.
//
// TWO SEPARATE TESTS, NOT ONE LOOPING OVER BOTH MAPS (deviation from the original
// investigation plan, empirically forced - see implementation.md): calling
// AutomationOpenMap twice inside one ordinary (non-latent) RunTest loop doesn't work,
// because AutomationOpenMap's EditorContext path (UEditorEngine::AutomationLoadMap)
// calls FEditorFileUtils::LoadMap() *synchronously*, immediately, on every call - not
// deferred behind a latent command. Since the loop's second AutomationOpenMap call
// runs before any of the first iteration's queued latent commands (including its
// FStartPIEForAutomationCommand) ever execute, it silently reloads the editor world
// out from under the first map before PIE even starts, so both PIE sessions end up
// copying the *second* map's content - confirmed empirically: the Editor log showed
// "Created PIE world by copying editor world from L_Level02" twice, never L_Level01,
// and every marker assertion failed as a result (a sequencing artifact, not a real
// placed-actor-at-origin regression). One IMPLEMENT_SIMPLE_AUTOMATION_TEST per map -
// each with its own single AutomationOpenMap call - avoids this entirely, matching
// every existing KrowdKontrol.PIE.* test's one-map-per-test shape.
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the other
// KrowdKontrol.* automation tests.

#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "PlaceholderTargetZoneActor.h"
#include "TargetZone.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"

#if WITH_DEV_AUTOMATION_TESTS

DEFINE_LATENT_AUTOMATION_COMMAND_TWO_PARAMETER(FKrowdKontrolAssertPlacedActorHealthCommand, FAutomationTestBase*, Test, FString, MapPath);

bool FKrowdKontrolAssertPlacedActorHealthCommand::Update()
{
	UWorld* PIEWorld = AutomationCommon::GetAnyGameWorld();
	if (!Test->TestNotNull(FString::Printf(TEXT("[%s] A live PIE world should exist"), *MapPath), PIEWorld))
	{
		return true;
	}

	TArray<APlaceholderTargetZoneActor*> Markers;
	for (TActorIterator<APlaceholderTargetZoneActor> It(PIEWorld); It; ++It)
	{
		Markers.Add(*It);
	}

	Test->TestTrue(FString::Printf(TEXT("[%s] should have at least one placed target-zone marker"), *MapPath), Markers.Num() > 0);

	for (APlaceholderTargetZoneActor* Marker : Markers)
	{
		Test->TestEqual(FString::Printf(TEXT("[%s] BeaconMeshComponent should be attached to the actor's root component"), *MapPath),
			Marker->BeaconMeshComponent->GetAttachParent(), Marker->GetRootComponent());
		Test->TestEqual(FString::Printf(TEXT("[%s] BeaconColumnMeshComponent should be attached to the actor's root component"), *MapPath),
			Marker->BeaconColumnMeshComponent->GetAttachParent(), Marker->GetRootComponent());
		Test->TestEqual(FString::Printf(TEXT("[%s] BeaconLightComponent should be attached to BeaconColumnMeshComponent"), *MapPath),
			Marker->BeaconLightComponent->GetAttachParent(), static_cast<USceneComponent*>(Marker->BeaconColumnMeshComponent));

		Test->TestFalse(FString::Printf(TEXT("[%s] Marker should not be left at the world origin (issue #199 self-heal regression)"), *MapPath),
			Marker->GetActorLocation().Equals(FVector::ZeroVector, 1.0f));

		TArray<AActor*> Attached;
		Marker->GetAttachedActors(Attached);
		ATargetZone* BankingZone = nullptr;
		for (AActor* AttachedActor : Attached)
		{
			if (ATargetZone* Candidate = Cast<ATargetZone>(AttachedActor))
			{
				BankingZone = Candidate;
				break;
			}
		}

		if (Test->TestNotNull(FString::Printf(TEXT("[%s] Marker should have a self-healed ATargetZone attached (real BeginPlay tick, issue #211)"), *MapPath), BankingZone))
		{
			Test->TestTrue(FString::Printf(TEXT("[%s] The self-healed ATargetZone should spawn at its marker's placed position, not the world origin"), *MapPath),
				BankingZone->GetActorLocation().Equals(Marker->GetActorLocation(), 1.0f));
		}
	}

	return true;
}

// One IMPLEMENT_SIMPLE_AUTOMATION_TEST per shipped map - append a new pair here for
// future L_Level03-05 maps as they ship (MISSION.md's 5-level Alpha roster). See the
// file header comment for why this can't be a single test looping over a map array.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolPIESerializedPlacedActorHealthLevel01Test,
	"KrowdKontrol.PIE.SerializedPlacedActorHealth.L_Level01",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolPIESerializedPlacedActorHealthLevel01Test::RunTest(const FString& Parameters)
{
	static const FString MapPath = TEXT("/Game/Maps/L_Level01");
	AutomationOpenMap(MapPath);
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForEngineFramesCommand(5));
	ADD_LATENT_AUTOMATION_COMMAND(FKrowdKontrolAssertPlacedActorHealthCommand(this, MapPath));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolPIESerializedPlacedActorHealthLevel02Test,
	"KrowdKontrol.PIE.SerializedPlacedActorHealth.L_Level02",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolPIESerializedPlacedActorHealthLevel02Test::RunTest(const FString& Parameters)
{
	static const FString MapPath = TEXT("/Game/Maps/L_Level02");
	AutomationOpenMap(MapPath);
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForEngineFramesCommand(5));
	ADD_LATENT_AUTOMATION_COMMAND(FKrowdKontrolAssertPlacedActorHealthCommand(this, MapPath));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
