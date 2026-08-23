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
// shared audit (see plan.md's Alternatives Rejected for why). Pass-1 validation fix
// coverage: (10) a UWaveSpawnerComponent present before OnLevelBegin but whose wave
// lands afterward (mirroring ARootSurgeBoss) still gets counted into TotalEnemyCount
// when OnWaveSpawned fires, and (11) a widget created after OnLevelBegin already
// broadcast (AKrowdKontrolPlayerController::CreateHUDWidgets() racing
// ULevelLifecycleSubsystem::OnWorldBeginPlay(), issue #235's already-documented hazard)
// still catches up to the real enemy count instead of staying stuck at "0/0".
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
#include "WaveSpawnerComponent.h"
#include "LevelLifecycleSubsystem.h"
#include "HUDChromeColours.h"
#include "AbilityData.h"
#include "AbilityUnlockComponent.h"
#include "EnemyTypeIndicatorComponent.h"
#include "RoomActor.h"
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

	// (10) Wave-spawner recount (pass-1 validation fix): a UWaveSpawnerComponent that
	// exists before OnLevelBegin (mirroring ARootSurgeBoss's CreateDefaultSubobject
	// spawner) but spawns its adds afterward must still get counted into
	// TotalEnemyCount when its OnWaveSpawned fires - otherwise the denominator
	// undercounts once those adds are bankable, producing an impossible "banked >
	// total" display.
	UWorld* WaveSpawnerWorld = FAutomationEditorCommonUtils::CreateNewMap();
	if (TestNotNull(TEXT("CreateNewMap should return a valid World for the wave-spawner test"), WaveSpawnerWorld))
	{
		WaveSpawnerWorld->InitializeActorsForPlay(FURL());

		constexpr int32 InitialEnemies = 2;
		for (int32 Index = 0; Index < InitialEnemies; ++Index)
		{
			WaveSpawnerWorld->SpawnActor<AEnemyBaseTestActor>();
		}

		// Spawner exists before OnLevelBegin, same as ARootSurgeBoss's
		// WaveSpawnerComponent (a CreateDefaultSubobject, present from construction) -
		// only its wave actually landing is delayed.
		AActor* SpawnerOwner = WaveSpawnerWorld->SpawnActor<AActor>();
		UWaveSpawnerComponent* Spawner = TestNotNull(TEXT("Spawner owner should spawn into the wave-spawner test World"), SpawnerOwner)
			? NewObject<UWaveSpawnerComponent>(SpawnerOwner)
			: nullptr;
		if (TestNotNull(TEXT("UWaveSpawnerComponent should construct"), Spawner))
		{
			Spawner->RegisterComponent();

			constexpr int32 WaveEnemies = 3;
			FWaveEntry Entry;
			Entry.EnemyClass = AEnemyBaseTestActor::StaticClass();
			Entry.Count = WaveEnemies;
			Entry.DelaySeconds = 0.0f;
			Spawner->Waves = { Entry };

			UQuestTrackerWidget* WaveSpawnerWidget = CreateWidget<UQuestTrackerWidget>(WaveSpawnerWorld, UQuestTrackerWidget::StaticClass());
			if (TestNotNull(TEXT("UQuestTrackerWidget should construct for the wave-spawner test"), WaveSpawnerWidget))
			{
				ULevelLifecycleSubsystem* WaveSpawnerLifecycleSubsystem = WaveSpawnerWorld->GetSubsystem<ULevelLifecycleSubsystem>();
				if (TestNotNull(TEXT("Wave-spawner test World should auto-instantiate ULevelLifecycleSubsystem"), WaveSpawnerLifecycleSubsystem))
				{
					WaveSpawnerLifecycleSubsystem->OnWorldBeginPlay(*WaveSpawnerWorld);
					TestEqual(TEXT("Total enemy count should be the initial sweep before any wave spawns"),
						WaveSpawnerWidget->GetTotalEnemyCount(), InitialEnemies);

					// The wave lands after OnLevelBegin, same as ARootSurgeBoss's
					// accelerated-cadence adds arriving 3-9s into the fight.
					Spawner->StartWaves();
					TestEqual(TEXT("Total enemy count should include the wave's adds after OnWaveSpawned"),
						WaveSpawnerWidget->GetTotalEnemyCount(), InitialEnemies + WaveEnemies);
					TestEqual(TEXT("Display text should reflect the updated total after a wave spawns"),
						WaveSpawnerWidget->GetQuestTrackerDisplayText().ToString(),
						FString::Printf(TEXT("Robots penned: 0/%d"), InitialEnemies + WaveEnemies));
				}
			}
		}
	}

	// (11) Late-subscribe catch-up (pass-1 validation fix): if OnLevelBegin already
	// broadcast before this widget existed to subscribe -
	// AKrowdKontrolPlayerController::CreateHUDWidgets()'s order relative to
	// ULevelLifecycleSubsystem::OnWorldBeginPlay() isn't guaranteed in real gameplay,
	// same documented hazard as UAbilityUnlockLevelSubsystem/OnScreenPromptWidget
	// (issue #235) - the widget must still pick up the current enemy count immediately
	// at construction, not stay stuck at "Robots penned: 0/0" forever since
	// OnLevelBegin never re-fires.
	UWorld* LateSubscribeWorld = FAutomationEditorCommonUtils::CreateNewMap();
	if (TestNotNull(TEXT("CreateNewMap should return a valid World for the late-subscribe test"), LateSubscribeWorld))
	{
		LateSubscribeWorld->InitializeActorsForPlay(FURL());

		constexpr int32 LateSubscribeEnemies = 6;
		for (int32 Index = 0; Index < LateSubscribeEnemies; ++Index)
		{
			LateSubscribeWorld->SpawnActor<AEnemyBaseTestActor>();
		}

		ULevelLifecycleSubsystem* LateSubscribeLifecycleSubsystem = LateSubscribeWorld->GetSubsystem<ULevelLifecycleSubsystem>();
		if (TestNotNull(TEXT("Late-subscribe test World should auto-instantiate ULevelLifecycleSubsystem"), LateSubscribeLifecycleSubsystem))
		{
			// OnLevelBegin fires BEFORE the widget exists - the exact race this fix covers.
			LateSubscribeLifecycleSubsystem->OnWorldBeginPlay(*LateSubscribeWorld);

			UQuestTrackerWidget* LateSubscribeWidget = CreateWidget<UQuestTrackerWidget>(LateSubscribeWorld, UQuestTrackerWidget::StaticClass());
			if (TestNotNull(TEXT("UQuestTrackerWidget should construct for the late-subscribe test"), LateSubscribeWidget))
			{
				TestEqual(TEXT("A late-created widget should still catch up to the already-broadcast enemy total"),
					LateSubscribeWidget->GetTotalEnemyCount(), LateSubscribeEnemies);
				TestEqual(TEXT("A late-created widget's display should not be stuck at 0/0"),
					LateSubscribeWidget->GetQuestTrackerDisplayText().ToString(),
					FString::Printf(TEXT("Robots penned: 0/%d"), LateSubscribeEnemies));
			}
		}
	}

	// (12) Suggested-ability line - colour-matched case (issue #249's AC (a)): a
	// live Sniper (SN_1PR) enemy present, Sleep unlocked -> the colour-matched
	// suggestion, tinted Sleep's real reserved colour. Also covers (12b): banking
	// the only remaining Sniper recomputes the suggestion back to the universal
	// fallback via HandleActorBanked() -> RefreshSuggestedAbilityDisplay(), proving
	// that handler does more than just increment the banked count (pass-2 review
	// coverage).
	UWorld* SuggestionUnlockedWorld = FAutomationEditorCommonUtils::CreateNewMap();
	if (TestNotNull(TEXT("CreateNewMap should return a valid World for the suggestion-unlocked test"), SuggestionUnlockedWorld))
	{
		SuggestionUnlockedWorld->InitializeActorsForPlay(FURL());

		// Zone must exist before the widget is created (matching real gameplay
		// ordering - see this file's header comment) so HandleLevelBegin()'s zone
		// discovery (or BindToLevelLifecycle()'s late-subscribe catch-up, whichever
		// applies) binds HandleActorBanked to it in time for (12b) below.
		ATargetZone* SuggestionZone = SuggestionUnlockedWorld->SpawnActor<ATargetZone>();
		TestNotNull(TEXT("ATargetZone should spawn into the suggestion-unlocked test World"), SuggestionZone);

		AEnemyBaseTestActor* SniperEnemy = SuggestionUnlockedWorld->SpawnActor<AEnemyBaseTestActor>();
		UEnemyTypeIndicatorComponent* TypeIndicator = TestNotNull(TEXT("Sniper test enemy should spawn"), SniperEnemy)
			? NewObject<UEnemyTypeIndicatorComponent>(SniperEnemy)
			: nullptr;
		if (TestNotNull(TEXT("UEnemyTypeIndicatorComponent should construct"), TypeIndicator))
		{
			TypeIndicator->EnemyType = EEnemyType::SN_1PR;
			TypeIndicator->RegisterComponent();

			AActor* UnlockOwner = SuggestionUnlockedWorld->SpawnActor<AActor>();
			UAbilityUnlockComponent* UnlockComponent = TestNotNull(TEXT("Unlock component owner should spawn"), UnlockOwner)
				? NewObject<UAbilityUnlockComponent>(UnlockOwner)
				: nullptr;
			if (TestNotNull(TEXT("UAbilityUnlockComponent should construct"), UnlockComponent))
			{
				UnlockComponent->RegisterComponent();
				UnlockComponent->NotifyLevelReached(2); // unlocks Sleep

				ULevelLifecycleSubsystem* SuggestionLifecycleSubsystem = SuggestionUnlockedWorld->GetSubsystem<ULevelLifecycleSubsystem>();
				if (TestNotNull(TEXT("Suggestion-unlocked test World should auto-instantiate ULevelLifecycleSubsystem"), SuggestionLifecycleSubsystem))
				{
					SuggestionLifecycleSubsystem->OnWorldBeginPlay(*SuggestionUnlockedWorld);

					UQuestTrackerWidget* SuggestionWidget = CreateWidget<UQuestTrackerWidget>(SuggestionUnlockedWorld, UQuestTrackerWidget::StaticClass());
					if (TestNotNull(TEXT("UQuestTrackerWidget should construct for the suggestion-unlocked test"), SuggestionWidget))
					{
						SuggestionWidget->BindAbilityUnlockComponent(UnlockComponent);

						TestEqual(TEXT("Suggested-ability line should show the colour-matched suggestion once Sleep is unlocked"),
							SuggestionWidget->GetSuggestedAbilityDisplayText().ToString(), FString(TEXT("SNIPERS → SLEEP (RMB)")));
						TestEqual(TEXT("Suggested-ability text colour should match Sleep's reserved colour"),
							SuggestionWidget->GetSuggestedAbilityTextColour(), AbilityData::Get(EAbilitySlot::Sleep).Colour);

						// (12b) Banking the only remaining Sniper - real Idle->Alert->
						// Controlled->Banked progression (same sequence
						// KrowdKontrolEnemyBaseTest.cpp's (g) uses), then the zone's real
						// OnActorBanked broadcast, matching ARoomActor::HandleZoneActorBanked's
						// production sequence (TransitionToBanked() + the zone's own
						// broadcast) rather than only firing the delegate in isolation.
						if (SniperEnemy && SuggestionZone)
						{
							SniperEnemy->TickCheckDetection(FVector::ZeroVector); // Idle -> Alert
							SniperEnemy->ReceiveControl(EAbilitySlot::Sleep); // Alert -> Controlled
							SniperEnemy->TransitionToBanked(); // Controlled -> Banked
							SuggestionZone->OnActorBanked.Broadcast(SniperEnemy);

							TestEqual(TEXT("Suggested-ability line should fall back to Stun once the only Sniper is banked"),
								SuggestionWidget->GetSuggestedAbilityDisplayText().ToString(), FString(TEXT("ANY ROBOT → STUN (LMB)")));
						}
					}
				}
			}
		}
	}

	// (13) Suggested-ability line - universal fallback case (issue #249's AC (b)):
	// same Sniper present, but Sleep is NOT unlocked (component's default state -
	// only Stun) -> falls back to "ANY ROBOT" + Stun's key/colour.
	UWorld* SuggestionFallbackWorld = FAutomationEditorCommonUtils::CreateNewMap();
	if (TestNotNull(TEXT("CreateNewMap should return a valid World for the suggestion-fallback test"), SuggestionFallbackWorld))
	{
		SuggestionFallbackWorld->InitializeActorsForPlay(FURL());

		AEnemyBaseTestActor* SniperEnemy = SuggestionFallbackWorld->SpawnActor<AEnemyBaseTestActor>();
		UEnemyTypeIndicatorComponent* TypeIndicator = TestNotNull(TEXT("Sniper test enemy should spawn for the fallback test"), SniperEnemy)
			? NewObject<UEnemyTypeIndicatorComponent>(SniperEnemy)
			: nullptr;
		if (TestNotNull(TEXT("UEnemyTypeIndicatorComponent should construct for the fallback test"), TypeIndicator))
		{
			TypeIndicator->EnemyType = EEnemyType::SN_1PR;
			TypeIndicator->RegisterComponent();

			AActor* UnlockOwner = SuggestionFallbackWorld->SpawnActor<AActor>();
			UAbilityUnlockComponent* UnlockComponent = TestNotNull(TEXT("Unlock component owner should spawn for the fallback test"), UnlockOwner)
				? NewObject<UAbilityUnlockComponent>(UnlockOwner)
				: nullptr;
			if (TestNotNull(TEXT("UAbilityUnlockComponent should construct for the fallback test"), UnlockComponent))
			{
				UnlockComponent->RegisterComponent();
				// Deliberately no NotifyLevelReached() call - only Stun unlocked, the
				// component's real construction-time default.

				ULevelLifecycleSubsystem* FallbackLifecycleSubsystem = SuggestionFallbackWorld->GetSubsystem<ULevelLifecycleSubsystem>();
				if (TestNotNull(TEXT("Suggestion-fallback test World should auto-instantiate ULevelLifecycleSubsystem"), FallbackLifecycleSubsystem))
				{
					FallbackLifecycleSubsystem->OnWorldBeginPlay(*SuggestionFallbackWorld);

					UQuestTrackerWidget* FallbackWidget = CreateWidget<UQuestTrackerWidget>(SuggestionFallbackWorld, UQuestTrackerWidget::StaticClass());
					if (TestNotNull(TEXT("UQuestTrackerWidget should construct for the suggestion-fallback test"), FallbackWidget))
					{
						FallbackWidget->BindAbilityUnlockComponent(UnlockComponent);

						TestEqual(TEXT("Suggested-ability line should fall back to the universal Stun suggestion when Sleep isn't unlocked"),
							FallbackWidget->GetSuggestedAbilityDisplayText().ToString(), FString(TEXT("ANY ROBOT → STUN (LMB)")));
						TestEqual(TEXT("Suggested-ability text colour should match Stun's reserved (white) colour"),
							FallbackWidget->GetSuggestedAbilityTextColour(), AbilityData::Get(EAbilitySlot::Stun).Colour);
					}
				}
			}
		}
	}

	// (14) Suggested-ability line - live update via OnAbilityUnlocked while already
	// bound (issue #249's "event-driven, no polling" claim): Sniper present, widget
	// bound while only Stun is unlocked (fallback showing), then NotifyLevelReached(2)
	// broadcasts OnAbilityUnlocked -> suggestion must flip live to the colour-matched
	// text without any re-bind or re-construction. Cases (12)/(13) above only prove
	// the "seed from current state at bind time" path (BindAbilityUnlockComponent()'s
	// own RefreshSuggestedAbilityDisplay() call) - this case is the only one that
	// exercises HandleAbilityUnlocked() via a real post-bind broadcast.
	UWorld* LiveUpdateWorld = FAutomationEditorCommonUtils::CreateNewMap();
	if (TestNotNull(TEXT("CreateNewMap should return a valid World for the live-update test"), LiveUpdateWorld))
	{
		LiveUpdateWorld->InitializeActorsForPlay(FURL());

		AEnemyBaseTestActor* LiveUpdateSniper = LiveUpdateWorld->SpawnActor<AEnemyBaseTestActor>();
		UEnemyTypeIndicatorComponent* LiveUpdateIndicator = TestNotNull(TEXT("Sniper test enemy should spawn for the live-update test"), LiveUpdateSniper)
			? NewObject<UEnemyTypeIndicatorComponent>(LiveUpdateSniper)
			: nullptr;
		if (TestNotNull(TEXT("UEnemyTypeIndicatorComponent should construct for the live-update test"), LiveUpdateIndicator))
		{
			LiveUpdateIndicator->EnemyType = EEnemyType::SN_1PR;
			LiveUpdateIndicator->RegisterComponent();

			AActor* LiveUpdateUnlockOwner = LiveUpdateWorld->SpawnActor<AActor>();
			UAbilityUnlockComponent* LiveUpdateUnlockComponent = TestNotNull(TEXT("Unlock component owner should spawn for the live-update test"), LiveUpdateUnlockOwner)
				? NewObject<UAbilityUnlockComponent>(LiveUpdateUnlockOwner)
				: nullptr;
			if (TestNotNull(TEXT("UAbilityUnlockComponent should construct for the live-update test"), LiveUpdateUnlockComponent))
			{
				LiveUpdateUnlockComponent->RegisterComponent();
				// Deliberately not unlocked yet - bind happens first this time.

				ULevelLifecycleSubsystem* LiveUpdateLifecycleSubsystem = LiveUpdateWorld->GetSubsystem<ULevelLifecycleSubsystem>();
				if (TestNotNull(TEXT("Live-update test World should auto-instantiate ULevelLifecycleSubsystem"), LiveUpdateLifecycleSubsystem))
				{
					LiveUpdateLifecycleSubsystem->OnWorldBeginPlay(*LiveUpdateWorld);

					UQuestTrackerWidget* LiveUpdateWidget = CreateWidget<UQuestTrackerWidget>(LiveUpdateWorld, UQuestTrackerWidget::StaticClass());
					if (TestNotNull(TEXT("UQuestTrackerWidget should construct for the live-update test"), LiveUpdateWidget))
					{
						LiveUpdateWidget->BindAbilityUnlockComponent(LiveUpdateUnlockComponent);

						TestEqual(TEXT("Suggestion should show the fallback before Sleep unlocks"),
							LiveUpdateWidget->GetSuggestedAbilityDisplayText().ToString(), FString(TEXT("ANY ROBOT → STUN (LMB)")));

						LiveUpdateUnlockComponent->NotifyLevelReached(2); // broadcasts OnAbilityUnlocked live

						TestEqual(TEXT("Suggestion should flip live to the colour-matched text on OnAbilityUnlocked"),
							LiveUpdateWidget->GetSuggestedAbilityDisplayText().ToString(), FString(TEXT("SNIPERS → SLEEP (RMB)")));
					}
				}
			}
		}
	}

	// (15) Suggested-ability line - multi-type tie-break (documented-but-untested
	// behaviour in ComputeSuggestedAbility()'s own comment): Sniper (Sleep-countered)
	// and Trooper (Root-countered) both alive. With only Root unlocked, the scan
	// picks Root (proving it's a real per-candidate IsAbilityUnlocked() scan, not a
	// fixed lowest-enum-value pick); once Sleep is also unlocked, declaration order
	// (Stun, Sleep, Root, Fear, Snare) picks Sleep instead.
	UWorld* TieBreakWorld = FAutomationEditorCommonUtils::CreateNewMap();
	if (TestNotNull(TEXT("CreateNewMap should return a valid World for the tie-break test"), TieBreakWorld))
	{
		TieBreakWorld->InitializeActorsForPlay(FURL());

		AEnemyBaseTestActor* TieBreakSniper = TieBreakWorld->SpawnActor<AEnemyBaseTestActor>();
		UEnemyTypeIndicatorComponent* TieBreakSniperIndicator = TestNotNull(TEXT("Sniper test enemy should spawn for the tie-break test"), TieBreakSniper)
			? NewObject<UEnemyTypeIndicatorComponent>(TieBreakSniper)
			: nullptr;
		if (TestNotNull(TEXT("Sniper UEnemyTypeIndicatorComponent should construct for the tie-break test"), TieBreakSniperIndicator))
		{
			TieBreakSniperIndicator->EnemyType = EEnemyType::SN_1PR;
			TieBreakSniperIndicator->RegisterComponent();
		}

		AEnemyBaseTestActor* TieBreakTrooper = TieBreakWorld->SpawnActor<AEnemyBaseTestActor>();
		UEnemyTypeIndicatorComponent* TieBreakTrooperIndicator = TestNotNull(TEXT("Trooper test enemy should spawn for the tie-break test"), TieBreakTrooper)
			? NewObject<UEnemyTypeIndicatorComponent>(TieBreakTrooper)
			: nullptr;
		if (TestNotNull(TEXT("Trooper UEnemyTypeIndicatorComponent should construct for the tie-break test"), TieBreakTrooperIndicator))
		{
			TieBreakTrooperIndicator->EnemyType = EEnemyType::TR_UPR;
			TieBreakTrooperIndicator->RegisterComponent();
		}

		AActor* TieBreakUnlockOwner = TieBreakWorld->SpawnActor<AActor>();
		UAbilityUnlockComponent* TieBreakUnlockComponent = TestNotNull(TEXT("Unlock component owner should spawn for the tie-break test"), TieBreakUnlockOwner)
			? NewObject<UAbilityUnlockComponent>(TieBreakUnlockOwner)
			: nullptr;
		if (TestNotNull(TEXT("UAbilityUnlockComponent should construct for the tie-break test"), TieBreakUnlockComponent))
		{
			TieBreakUnlockComponent->RegisterComponent();
			TieBreakUnlockComponent->NotifyLevelReached(3); // unlocks Root only, Sleep still locked

			ULevelLifecycleSubsystem* TieBreakLifecycleSubsystem = TieBreakWorld->GetSubsystem<ULevelLifecycleSubsystem>();
			if (TestNotNull(TEXT("Tie-break test World should auto-instantiate ULevelLifecycleSubsystem"), TieBreakLifecycleSubsystem))
			{
				TieBreakLifecycleSubsystem->OnWorldBeginPlay(*TieBreakWorld);

				UQuestTrackerWidget* TieBreakWidget = CreateWidget<UQuestTrackerWidget>(TieBreakWorld, UQuestTrackerWidget::StaticClass());
				if (TestNotNull(TEXT("UQuestTrackerWidget should construct for the tie-break test"), TieBreakWidget))
				{
					TieBreakWidget->BindAbilityUnlockComponent(TieBreakUnlockComponent);

					TestEqual(TEXT("With only Root unlocked, the scan should pick Root over the Sniper's still-locked Sleep"),
						TieBreakWidget->GetSuggestedAbilityDisplayText().ToString(), FString(TEXT("TROOPERS → ROOT (Q)")));

					TieBreakUnlockComponent->NotifyLevelReached(2); // also unlocks Sleep, live via OnAbilityUnlocked

					TestEqual(TEXT("With both Sleep and Root unlocked, declaration order (Stun, Sleep, Root, ...) should pick Sleep first"),
						TieBreakWidget->GetSuggestedAbilityDisplayText().ToString(), FString(TEXT("SNIPERS → SLEEP (RMB)")));
				}
			}
		}
	}

	return true;
}

// Confirms the room-state line (issue #248, PRD "Mission Briefing & Live Quest
// Tracker" REQ-2): with one owned enemy remaining it reads "Room 1 — 1 robot left"
// (singular), with two it reads "... 2 robots left" (plural); once the last owned
// enemy banks, ARoomActor::OnRoomClearedStateChanged fires and the line flips live
// to "DOOR OPEN" with no re-bind/re-construction - mirroring case (14) above's
// live-update-while-bound shape. Needs its own World with
// World->SetBegunPlay(true) - KrowdKontrolRoomActorDoorGatingTest.cpp's file
// comment documents this is required for AEnemyBase::OnEnemyBanked to actually
// reach ARoomActor's bound handler; the shared World at the top of this file's
// RunTest() never calls SetBegunPlay(true), so a fresh World is needed here, same
// as cases (14)/(15) above already do for their own reasons.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolQuestTrackerWidgetRoomStateTest,
	"KrowdKontrol.Unit.QuestTrackerWidgetRoomState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolQuestTrackerWidgetRoomStateTest::RunTest(const FString& Parameters)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
	{
		return false;
	}
	World->InitializeActorsForPlay(FURL());
	World->SetBegunPlay(true);

	ARoomActor* Room = World->SpawnActor<ARoomActor>();
	if (!TestNotNull(TEXT("ARoomActor should spawn into the test World"), Room))
	{
		return false;
	}

	AEnemyBaseTestActor* EnemyOne = World->SpawnActor<AEnemyBaseTestActor>();
	AEnemyBaseTestActor* EnemyTwo = World->SpawnActor<AEnemyBaseTestActor>();
	if (!TestNotNull(TEXT("First test enemy should spawn"), EnemyOne) ||
		!TestNotNull(TEXT("Second test enemy should spawn"), EnemyTwo))
	{
		return false;
	}
	Room->AddOwnedEnemy(EnemyOne);
	Room->AddOwnedEnemy(EnemyTwo);

	ULevelLifecycleSubsystem* LifecycleSubsystem = World->GetSubsystem<ULevelLifecycleSubsystem>();
	if (!TestNotNull(TEXT("World should auto-instantiate ULevelLifecycleSubsystem"), LifecycleSubsystem))
	{
		return false;
	}
	LifecycleSubsystem->OnWorldBeginPlay(*World);

	UQuestTrackerWidget* Widget = CreateWidget<UQuestTrackerWidget>(World, UQuestTrackerWidget::StaticClass());
	if (!TestNotNull(TEXT("UQuestTrackerWidget should construct"), Widget))
	{
		return false;
	}

	TestEqual(TEXT("Room-state line should show Room 1 with 2 robots left (plural)"),
		Widget->GetRoomStateDisplayText().ToString(), FString(TEXT("Room 1 — 2 robots left")));

	const FVector ZeroDistanceLocation(0.0f, 0.0f, 0.0f);
	EnemyOne->TickCheckDetection(ZeroDistanceLocation); // Idle -> Alert
	EnemyOne->ReceiveControl(EAbilitySlot::Stun);        // Alert -> Controlled
	EnemyOne->TransitionToBanked();                      // Controlled -> Banked

	TestEqual(TEXT("Room-state line should update live to singular '1 robot left'"),
		Widget->GetRoomStateDisplayText().ToString(), FString(TEXT("Room 1 — 1 robot left")));

	EnemyTwo->TickCheckDetection(ZeroDistanceLocation);
	EnemyTwo->ReceiveControl(EAbilitySlot::Stun);
	EnemyTwo->TransitionToBanked();

	TestEqual(TEXT("Room-state line should flip live to DOOR OPEN once the last owned enemy banks"),
		Widget->GetRoomStateDisplayText().ToString(), FString(TEXT("DOOR OPEN")));

	return true;
}

#endif
