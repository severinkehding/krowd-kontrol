// Confirms UEnemyTypeIndicatorComponent (issue #77, PRD 13 REQ-7) gives all 4 core
// enemy types (RU-NNR, TR-UPR, B0-0MR, SN-1PR) a distinct non-colour marker, even
// though only B0-0MR (ABomberEnemy) and SN-1PR (ASniperEnemy) have concrete gameplay
// actors today - RU-NNR/TR-UPR are proven via a component spawned directly onto the
// existing AEnemyBaseTestActor scaffold, per this issue's own scope boundary (no new
// gameplay actor classes for enemies-and-ai-PRD territory).
//
// Mirrors KrowdKontrolBomberEnemyTest.cpp/KrowdKontrolSniperEnemyTest.cpp for the
// No-World constructor-state shape, KrowdKontrolStationPowerUpComponentTest.cpp for
// the World-backed dynamic-component-registration shape, and
// KrowdKontrolReservedGameplayColoursTest.cpp for the pairwise-distinctness loop and
// reserved-colour non-collision audit shape.
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the other
// KrowdKontrol.Unit.* tests.

#include "Misc/AutomationTest.h"
#include "EnemyTypeIndicatorComponent.h"
#include "EnemyType.h"
#include "BomberEnemy.h"
#include "SniperEnemy.h"
#include "EnemyBaseTestActor.h"
#include "ReservedGameplayColours.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"
#include "Components/TextRenderComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolEnemyTypeIndicatorComponentTest,
	"KrowdKontrol.Unit.EnemyTypeIndicatorComponent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolEnemyTypeIndicatorComponentTest::RunTest(const FString& Parameters)
{
	// (a) No-World: ABomberEnemy/ASniperEnemy each construct with a correctly
	// pre-configured EnemyTypeIndicatorComponent, its EnemyType/GetMarkerText()
	// matching the enum's own accessor rather than a hardcoded string literal.
	ABomberEnemy* Bomber = NewObject<ABomberEnemy>();
	if (!TestNotNull(TEXT("ABomberEnemy should construct"), Bomber))
	{
		return false;
	}
	UEnemyTypeIndicatorComponent* BomberIndicator = Bomber->EnemyTypeIndicatorComponent;
	if (!TestNotNull(TEXT("ABomberEnemy should have an EnemyTypeIndicatorComponent"), BomberIndicator))
	{
		return false;
	}
	TestEqual(TEXT("Bomber's EnemyTypeIndicatorComponent is B0_0MR"),
		static_cast<uint8>(BomberIndicator->EnemyType), static_cast<uint8>(EEnemyType::B0_0MR));
	TestEqual(TEXT("Bomber's marker text matches EEnemyType::B0_0MR's own DisplayName"),
		BomberIndicator->GetMarkerText().ToString(),
		StaticEnum<EEnemyType>()->GetDisplayNameTextByValue(static_cast<int64>(EEnemyType::B0_0MR)).ToString());

	ASniperEnemy* Sniper = NewObject<ASniperEnemy>();
	if (!TestNotNull(TEXT("ASniperEnemy should construct"), Sniper))
	{
		return false;
	}
	UEnemyTypeIndicatorComponent* SniperIndicator = Sniper->EnemyTypeIndicatorComponent;
	if (!TestNotNull(TEXT("ASniperEnemy should have an EnemyTypeIndicatorComponent"), SniperIndicator))
	{
		return false;
	}
	TestEqual(TEXT("Sniper's EnemyTypeIndicatorComponent is SN_1PR"),
		static_cast<uint8>(SniperIndicator->EnemyType), static_cast<uint8>(EEnemyType::SN_1PR));
	TestEqual(TEXT("Sniper's marker text matches EEnemyType::SN_1PR's own DisplayName"),
		SniperIndicator->GetMarkerText().ToString(),
		StaticEnum<EEnemyType>()->GetDisplayNameTextByValue(static_cast<int64>(EEnemyType::SN_1PR)).ToString());

	// (b) World-backed: spawn real actors for all 4 core types (2 concrete gameplay
	// actors, 2 AEnemyBaseTestActor stand-ins for RU-NNR/TR-UPR, which have no
	// concrete gameplay actor class yet - explicitly out of scope, see the plan's
	// NOT Building section), then drive InitializeMarkerVisual() directly rather than
	// relying on BeginPlay timing in a test-created World.
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
	{
		return false;
	}

	ABomberEnemy* WorldBomber = World->SpawnActor<ABomberEnemy>();
	ASniperEnemy* WorldSniper = World->SpawnActor<ASniperEnemy>();
	AEnemyBaseTestActor* RunnerStandIn = World->SpawnActor<AEnemyBaseTestActor>();
	AEnemyBaseTestActor* TrapperStandIn = World->SpawnActor<AEnemyBaseTestActor>();
	if (!TestNotNull(TEXT("ABomberEnemy should spawn into the test World"), WorldBomber) ||
		!TestNotNull(TEXT("ASniperEnemy should spawn into the test World"), WorldSniper) ||
		!TestNotNull(TEXT("RU-NNR stand-in actor should spawn into the test World"), RunnerStandIn) ||
		!TestNotNull(TEXT("TR-UPR stand-in actor should spawn into the test World"), TrapperStandIn))
	{
		return false;
	}

	UEnemyTypeIndicatorComponent* RunnerIndicator = NewObject<UEnemyTypeIndicatorComponent>(RunnerStandIn);
	RunnerIndicator->EnemyType = EEnemyType::RU_NNR;
	RunnerIndicator->RegisterComponent();

	UEnemyTypeIndicatorComponent* TrapperIndicator = NewObject<UEnemyTypeIndicatorComponent>(TrapperStandIn);
	TrapperIndicator->EnemyType = EEnemyType::TR_UPR;
	TrapperIndicator->RegisterComponent();

	UEnemyTypeIndicatorComponent* AllIndicators[4] =
	{
		WorldBomber->EnemyTypeIndicatorComponent,
		WorldSniper->EnemyTypeIndicatorComponent,
		RunnerIndicator,
		TrapperIndicator,
	};
	AActor* AllOwners[4] = { WorldBomber, WorldSniper, RunnerStandIn, TrapperStandIn };

	for (int32 Index = 0; Index < 4; ++Index)
	{
		if (!TestNotNull(*FString::Printf(TEXT("AllIndicators[%d] should be non-null"), Index), AllIndicators[Index]))
		{
			return false;
		}
		AllIndicators[Index]->InitializeMarkerVisual();
	}

	// (c) InitializeMarkerVisual() actually creates and attaches a MarkerTextComponent
	// for all 4 types.
	for (int32 Index = 0; Index < 4; ++Index)
	{
		UTextRenderComponent* Marker = AllIndicators[Index]->MarkerTextComponent;
		if (TestNotNull(*FString::Printf(TEXT("AllIndicators[%d]->MarkerTextComponent should be non-null after InitializeMarkerVisual"), Index), Marker))
		{
			TestTrue(*FString::Printf(TEXT("AllIndicators[%d]'s MarkerTextComponent should be attached to its owner's root"), Index),
				Marker->GetAttachParent() == AllOwners[Index]->GetRootComponent());
		}
	}

	// (c2) Regression guard for issue #134: MarkerTextComponent must be rotated to
	// face this project's fixed top-down camera rigs, not left at
	// UTextRenderComponent's engine-default orientation - the default renders
	// mirror-flipped under both FlatCamera3DPrototypePawn's -80deg and
	// Paper2DPrototypePawn's -90deg boom pitch (both view the actor from the
	// component's local +X side; see EnemyTypeIndicatorComponent.cpp's own comment
	// at the SetRelativeRotation call for the full derivation).
	for (int32 Index = 0; Index < 4; ++Index)
	{
		UTextRenderComponent* Marker = AllIndicators[Index]->MarkerTextComponent;
		if (TestNotNull(*FString::Printf(TEXT("AllIndicators[%d]->MarkerTextComponent should be non-null"), Index), Marker))
		{
			TestTrue(*FString::Printf(TEXT("AllIndicators[%d]'s MarkerTextComponent should be Yaw-flipped 180 degrees to face the top-down camera rigs"), Index),
				Marker->GetRelativeRotation().Equals(FRotator(0.0f, 180.0f, 0.0f), 0.01f));
		}
	}

	// (d) AC #1: each of the 4 core enemy types has mutually distinct marker text.
	TArray<FString> AllMarkerTexts;
	for (UEnemyTypeIndicatorComponent* Indicator : AllIndicators)
	{
		AllMarkerTexts.Add(Indicator->GetMarkerText().ToString());
	}
	for (int32 i = 0; i < AllMarkerTexts.Num(); ++i)
	{
		for (int32 j = i + 1; j < AllMarkerTexts.Num(); ++j)
		{
			TestNotEqual(*FString::Printf(TEXT("Marker text %d and %d should be mutually distinct"), i, j),
				AllMarkerTexts[i], AllMarkerTexts[j]);
		}
	}

	// (e) Hard Invariant 3 regression guard: the marker's own colour must never
	// collide with a reserved gameplay colour.
	const TArray<FLinearColor> AllReserved = ReservedGameplayColours::GetAll();
	for (int32 Index = 0; Index < 4; ++Index)
	{
		const FLinearColor MarkerColorLinear(AllIndicators[Index]->MarkerTextComponent->TextRenderColor);
		TestFalse(*FString::Printf(TEXT("AllIndicators[%d]'s marker colour should not collide with a reserved gameplay colour"), Index),
			AllReserved.ContainsByPredicate(
				[MarkerColorLinear](const FLinearColor& Reserved) { return Reserved.Equals(MarkerColorLinear, 0.01f); }));
	}

	// (f) Idempotency regression: a second InitializeMarkerVisual() call must not
	// create a duplicate MarkerTextComponent.
	UTextRenderComponent* MarkerBeforeSecondCall = WorldBomber->EnemyTypeIndicatorComponent->MarkerTextComponent;
	WorldBomber->EnemyTypeIndicatorComponent->InitializeMarkerVisual();
	TestTrue(TEXT("A second InitializeMarkerVisual() call should not create a duplicate MarkerTextComponent"),
		WorldBomber->EnemyTypeIndicatorComponent->MarkerTextComponent == MarkerBeforeSecondCall);

	// (g) Regression: an owner with no RootComponent must warn, not crash, and must
	// never create MarkerTextComponent. A plain AActor (not AEnemyBaseTestActor,
	// which now always carries a root - see (b) above) is used here so this path is
	// deliberately, not incidentally, exercised. The early-return path never sets
	// bHasInitializedMarkerVisual, so a second call retries and warns again rather
	// than silently no-op'ing.
	AActor* RootlessOwner = World->SpawnActor<AActor>();
	if (!TestNotNull(TEXT("Rootless owner actor should spawn into the test World"), RootlessOwner))
	{
		return false;
	}
	TestNull(TEXT("Sanity: plain AActor should have no RootComponent by default"), RootlessOwner->GetRootComponent());

	UEnemyTypeIndicatorComponent* RootlessIndicator = NewObject<UEnemyTypeIndicatorComponent>(RootlessOwner);
	RootlessIndicator->EnemyType = EEnemyType::RU_NNR;
	RootlessIndicator->RegisterComponent();

	AddExpectedError(TEXT("found no Owner root component"), EAutomationExpectedErrorFlags::Contains, 2, false);
	RootlessIndicator->InitializeMarkerVisual();
	TestNull(TEXT("MarkerTextComponent should stay null when the owner has no RootComponent"),
		RootlessIndicator->MarkerTextComponent);

	RootlessIndicator->InitializeMarkerVisual();
	TestNull(TEXT("A second InitializeMarkerVisual() call on a rootless owner should still leave MarkerTextComponent null"),
		RootlessIndicator->MarkerTextComponent);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
