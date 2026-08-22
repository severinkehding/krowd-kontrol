// Confirms UQuestTrackerWidget (issue #247, PRD "Mission Briefing & Live Quest
// Tracker" REQ-2): (1) the display text before OnLevelBegin fires reads
// "Robots penned: 0/0" (no fabricated placeholder), (2) OnLevelBegin captures the
// live AEnemyBase total as Y, (3) firing ATargetZone::OnActorBanked N times
// increments the displayed count to N, matching the issue's explicit "fires
// OnActorBanked N times, asserts displayed count reaches N" acceptance criterion,
// (4) the panel is anchored to the top-right corner, (5) its pixel footprint stays
// within the issue's ~15%-of-screen-width envelope at both a 1280x720 minimum and a
// 3840x2160 maximum target resolution (same reasoning
// KrowdKontrolEnergyMeterWidgetTest.cpp section 9b documents), and (6) its chrome
// colours come from HUDChromeColours, mirroring KrowdKontrolEnergyMeterWidgetTest.cpp's
// own in-file chrome check rather than KrowdKontrolReservedGameplayColoursTest.cpp's
// shared audit (see plan.md's Alternatives Rejected for why).
//
// World->InitializeActorsForPlay(FURL()) is called up front, before spawning any
// actor, purely defensively: KrowdKontrolDualZoneBossTest.cpp's own file comment
// documents a previously-wrong plan assumption that OnActorBanked.Broadcast()
// never needs this - true for a UObject target (Broadcast dispatches via
// UObject::ProcessEvent, not gated on AreActorsInitialized()), but calling it
// anyway costs nothing and removes any doubt.
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the other
// KrowdKontrol.Unit.* tests.

#include "Misc/AutomationTest.h"
#include "QuestTrackerWidget.h"
#include "EnemyBaseTestActor.h"
#include "TargetZone.h"
#include "LevelLifecycleSubsystem.h"
#include "HUDChromeColours.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolQuestTrackerWidgetTest,
	"KrowdKontrol.Unit.QuestTrackerWidget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolQuestTrackerWidgetTest::RunTest(const FString& Parameters)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
	{
		return false;
	}
	World->InitializeActorsForPlay(FURL());

	// Enemies + zone must exist BEFORE OnLevelBegin fires, matching real gameplay
	// ordering (ARoomActor::BeginPlay()'s EnsureBankingZonesWired() spawns
	// ATargetZone during the world's actor-BeginPlay pass, which the engine
	// guarantees completes before ULevelLifecycleSubsystem::OnWorldBeginPlay()
	// broadcasts OnLevelBegin).
	constexpr int32 TotalEnemies = 5;
	for (int32 Index = 0; Index < TotalEnemies; ++Index)
	{
		if (!TestNotNull(TEXT("AEnemyBaseTestActor should spawn into the test World"), World->SpawnActor<AEnemyBaseTestActor>()))
		{
			return false;
		}
	}

	ATargetZone* Zone = World->SpawnActor<ATargetZone>();
	if (!TestNotNull(TEXT("ATargetZone should spawn into the test World"), Zone))
	{
		return false;
	}

	UQuestTrackerWidget* Widget = CreateWidget<UQuestTrackerWidget>(World, UQuestTrackerWidget::StaticClass());
	if (!TestNotNull(TEXT("UQuestTrackerWidget should construct"), Widget))
	{
		return false;
	}

	// (1) Honest zero display before OnLevelBegin - no fabricated placeholder.
	TestEqual(TEXT("Display before OnLevelBegin should read 0/0"),
		Widget->GetQuestTrackerDisplayText().ToString(), FString(TEXT("Robots penned: 0/0")));

	ULevelLifecycleSubsystem* LifecycleSubsystem = World->GetSubsystem<ULevelLifecycleSubsystem>();
	if (!TestNotNull(TEXT("UWorld should auto-instantiate ULevelLifecycleSubsystem"), LifecycleSubsystem))
	{
		return false;
	}
	LifecycleSubsystem->OnWorldBeginPlay(*World);

	// (2) OnLevelBegin captures the live enemy total.
	TestEqual(TEXT("Total enemy count should be captured on OnLevelBegin"), Widget->GetTotalEnemyCount(), TotalEnemies);
	TestEqual(TEXT("Banked count should start at 0 after OnLevelBegin"), Widget->GetBankedCount(), 0);
	TestEqual(TEXT("Display text should reflect 0/TotalEnemies after OnLevelBegin"),
		Widget->GetQuestTrackerDisplayText().ToString(), FString(TEXT("Robots penned: 0/5")));

	// (3) N broadcasts, one per distinct banked actor -> displayed count reaches N.
	// The issue's explicit AC. Distinct actors, not one actor broadcast N times -
	// real gameplay banks N different enemies, and HandleActorBanked() dedups by
	// actor identity (see (3b) below), so reusing one actor here would only ever
	// reach 1.
	constexpr int32 BankFireCount = 3;
	AActor* BankedActorsForTest[BankFireCount];
	for (int32 Index = 0; Index < BankFireCount; ++Index)
	{
		BankedActorsForTest[Index] = World->SpawnActor<AActor>();
		if (!TestNotNull(TEXT("A dummy AActor should spawn into the test World"), BankedActorsForTest[Index]))
		{
			return false;
		}
		Zone->OnActorBanked.Broadcast(BankedActorsForTest[Index]);
	}

	TestEqual(TEXT("Banked count should reach N after N OnActorBanked broadcasts"), Widget->GetBankedCount(), BankFireCount);
	TestEqual(TEXT("Display text should reflect N/TotalEnemies after N broadcasts"),
		Widget->GetQuestTrackerDisplayText().ToString(), FString(TEXT("Robots penned: 3/5")));

	// (3b) Per-actor dedup - ATargetZone::OnActorBanked fires once per overlapping
	// component, not once per actor (TargetZone.cpp's own "KNOWN GAP" comment); a
	// repeat broadcast for an already-banked actor must not double-count it.
	Zone->OnActorBanked.Broadcast(BankedActorsForTest[0]);
	TestEqual(TEXT("A repeat broadcast for an already-banked actor should not increment the count"),
		Widget->GetBankedCount(), BankFireCount);

	// (4) Corner anchoring.
	UCanvasPanel* RootCanvas = Cast<UCanvasPanel>(Widget->WidgetTree->RootWidget);
	if (TestNotNull(TEXT("Widget root should be a UCanvasPanel"), RootCanvas))
	{
		TestEqual(TEXT("Root canvas should have exactly one child"), RootCanvas->GetChildrenCount(), 1);
		if (RootCanvas->GetChildrenCount() == 1)
		{
			UCanvasPanelSlot* TrackerSlot = Cast<UCanvasPanelSlot>(RootCanvas->GetChildAt(0)->Slot);
			if (TestNotNull(TEXT("Tracker child's slot should be a UCanvasPanelSlot"), TrackerSlot))
			{
				TestTrue(TEXT("Tracker should be anchored to the top-right corner"),
					TrackerSlot->GetAnchors() == FAnchors(1.0f, 0.0f, 1.0f, 0.0f));
				TestTrue(TEXT("Tracker should be aligned to its own top-right corner"),
					TrackerSlot->GetAlignment() == FVector2D(1.0f, 0.0f));
			}
		}
	}

	// (5) Resolution-safety envelope - issue #247's explicit ~15%-of-screen-width
	// ceiling, checked at this project's documented min/max target resolutions
	// (same pair KrowdKontrolEnergyMeterWidgetTest.cpp section 9b uses). The low
	// (smallest) resolution is the binding case - both are checked explicitly rather
	// than relying on that argument alone.
	const float TrackerFootprintWidthPx = UQuestTrackerWidget::TrackerMarginPx + UQuestTrackerWidget::TrackerWidthPx;
	const float MaxWidthFraction = 0.15f;
	const FVector2D TargetResolutions[] = { FVector2D(1280.0f, 720.0f), FVector2D(3840.0f, 2160.0f) };
	for (const FVector2D& TargetResolution : TargetResolutions)
	{
		TestTrue(*FString::Printf(TEXT("Tracker footprint width should stay within %.0f%% of screen width at %dx%d"),
			MaxWidthFraction * 100.0f, (int32)TargetResolution.X, (int32)TargetResolution.Y),
			TrackerFootprintWidthPx <= TargetResolution.X * MaxWidthFraction);
	}

	// (6) Chrome-colour compliance (Hard Invariant 3).
	TestEqual(TEXT("Chrome background should come from HUDChromeColours::GetBackground()"),
		Widget->ChromeBorder->GetBrushColor(), HUDChromeColours::GetBackground());
	TestEqual(TEXT("Text colour should come from HUDChromeColours::GetText()"),
		Widget->BankedCountText->GetColorAndOpacity().GetSpecifiedColor(), HUDChromeColours::GetText());

	// (7) Initialize() guard - must not rebuild the tree when NativeOnInitialized()
	// already ran. Mirrors KrowdKontrolEnergyMeterWidgetTest.cpp's case (11).
	UQuestTrackerWidget* GuardWidget = NewObject<UQuestTrackerWidget>();
	if (TestNotNull(TEXT("UQuestTrackerWidget should construct for guard test"), GuardWidget))
	{
		GuardWidget->NativeOnInitialized();
		if (TestNotNull(TEXT("BankedCountText should be populated after NativeOnInitialized()"), ToRawPtr(GuardWidget->BankedCountText)))
		{
			UTextBlock* FirstBankedCountText = GuardWidget->BankedCountText;
			GuardWidget->Initialize();
			TestEqual(TEXT("Initialize() must not rebuild the tree when already built"),
				ToRawPtr(GuardWidget->BankedCountText), FirstBankedCountText);
		}
	}

	// (8) Unbuilt-tree safety - a widget whose tree was never built (bare
	// NewObject(), neither NativeOnInitialized() nor Initialize() called) should
	// degrade safely. Mirrors KrowdKontrolEnergyMeterWidgetTest.cpp's case (12).
	UQuestTrackerWidget* UnbuiltWidget = NewObject<UQuestTrackerWidget>();
	if (TestNotNull(TEXT("UQuestTrackerWidget should construct for unbuilt-tree test"), UnbuiltWidget))
	{
		TestTrue(TEXT("Unbuilt widget should report empty display text"),
			UnbuiltWidget->GetQuestTrackerDisplayText().IsEmpty());
		TestEqual(TEXT("Unbuilt widget should report 0 banked count"), UnbuiltWidget->GetBankedCount(), 0);
		TestEqual(TEXT("Unbuilt widget should report 0 total enemy count"), UnbuiltWidget->GetTotalEnemyCount(), 0);
	}

	// (9) Multi-zone binding - HandleLevelBegin() must bind to every live
	// ATargetZone, not just the first one an iterator finds. Realistic given
	// existing dual-zone level content (KrowdKontrolDualZoneBossTest.cpp).
	UWorld* MultiZoneWorld = FAutomationEditorCommonUtils::CreateNewMap();
	if (TestNotNull(TEXT("CreateNewMap should return a valid World for the multi-zone test"), MultiZoneWorld))
	{
		MultiZoneWorld->InitializeActorsForPlay(FURL());

		ATargetZone* FirstZone = MultiZoneWorld->SpawnActor<ATargetZone>();
		ATargetZone* SecondZone = MultiZoneWorld->SpawnActor<ATargetZone>();
		UQuestTrackerWidget* MultiZoneWidget = CreateWidget<UQuestTrackerWidget>(MultiZoneWorld, UQuestTrackerWidget::StaticClass());
		if (TestNotNull(TEXT("First ATargetZone should spawn into the multi-zone test World"), FirstZone)
			&& TestNotNull(TEXT("Second ATargetZone should spawn into the multi-zone test World"), SecondZone)
			&& TestNotNull(TEXT("UQuestTrackerWidget should construct for the multi-zone test"), MultiZoneWidget))
		{
			ULevelLifecycleSubsystem* MultiZoneLifecycleSubsystem = MultiZoneWorld->GetSubsystem<ULevelLifecycleSubsystem>();
			if (TestNotNull(TEXT("Multi-zone test World should auto-instantiate ULevelLifecycleSubsystem"), MultiZoneLifecycleSubsystem))
			{
				MultiZoneLifecycleSubsystem->OnWorldBeginPlay(*MultiZoneWorld);

				AActor* FirstDummyActor = MultiZoneWorld->SpawnActor<AActor>();
				AActor* SecondDummyActor = MultiZoneWorld->SpawnActor<AActor>();
				if (TestNotNull(TEXT("First dummy AActor should spawn into the multi-zone test World"), FirstDummyActor)
					&& TestNotNull(TEXT("Second dummy AActor should spawn into the multi-zone test World"), SecondDummyActor))
				{
					FirstZone->OnActorBanked.Broadcast(FirstDummyActor);
					SecondZone->OnActorBanked.Broadcast(SecondDummyActor);
					TestEqual(TEXT("Banked count should sum broadcasts from both zones"), MultiZoneWidget->GetBankedCount(), 2);
				}
			}
		}
	}

	return true;
}

#endif
