// Confirms UControlledDurationIndicatorComponent (issue #225, PRD
// docs/prd-enemy-effect-indicator.md REQ-1 and the testable half of REQ-3): a
// world-space depleting bar appears the instant an AEnemyBase-derived enemy enters
// Controlled, filled in the controlling ability's colour, drains to 0 as
// TickControlledDuration advances, and disappears immediately on reversion to Alert
// or on Banked - including from a bare NewObject<>() actor with no UWorld at all
// (the dominant existing test shape across this module).
//
// Mirrors KrowdKontrolEnemyBaseTest.cpp's no-World, direct-friend-call idiom for
// cases (a)-(f); mirrors KrowdKontrolEnemyTypeIndicatorComponentTest.cpp's
// World-backed CreateNewMap() shape for case (g), which needs a registered mesh
// component to read a real RelativeLocation from.
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the other
// KrowdKontrol.Unit.* tests.

#include "Misc/AutomationTest.h"
#include "EnemyBase.h"
#include "EnemyBaseTestActor.h"
#include "ControlledDurationIndicatorComponent.h"
#include "AbilityData.h"
#include "BomberEnemy.h"
#include "TrooperEnemy.h"
#include "EnemyTypeIndicatorComponent.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"
#include "Components/TextRenderComponent.h"
#include "Components/StaticMeshComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolControlledDurationIndicatorComponentTest,
	"KrowdKontrol.Unit.ControlledDurationIndicatorComponent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolControlledDurationIndicatorComponentTest::RunTest(const FString& Parameters)
{
	const FVector ZeroDistanceLocation(0.0f, 0.0f, 0.0f);

	// (a) Fresh actor: the indicator component exists (proves the base constructor's
	// CreateDefaultSubobject) and starts hidden.
	AEnemyBaseTestActor* FreshEnemy = NewObject<AEnemyBaseTestActor>();
	if (!TestNotNull(TEXT("AEnemyBaseTestActor should construct"), FreshEnemy))
	{
		return false;
	}
	UControlledDurationIndicatorComponent* FreshIndicator = FreshEnemy->GetControlledDurationIndicatorComponent();
	if (!TestNotNull(TEXT("AEnemyBase should own a ControlledDurationIndicatorComponent"), FreshIndicator))
	{
		return false;
	}
	TestFalse(TEXT("bIsVisible should default to false"), FreshIndicator->bIsVisible);

	// (b) Entering Controlled shows the indicator immediately (no world tick in
	// between), at fill fraction 1.0, in the controlling ability's colour.
	AEnemyBaseTestActor* Enemy = NewObject<AEnemyBaseTestActor>();
	UControlledDurationIndicatorComponent* Indicator = Enemy->GetControlledDurationIndicatorComponent();
	if (!TestNotNull(TEXT("Enemy should own a ControlledDurationIndicatorComponent"), Indicator))
	{
		return false;
	}
	Enemy->TickCheckDetection(ZeroDistanceLocation); // Idle -> Alert
	Enemy->ReceiveControl(EAbilitySlot::Sleep); // Alert -> Controlled
	TestTrue(TEXT("bIsVisible should be true immediately after ReceiveControl"), Indicator->bIsVisible);
	TestEqual(TEXT("FillFraction should be 1.0 immediately after ReceiveControl"), Indicator->FillFraction, 1.0f);
	TestTrue(TEXT("CurrentColour should match AbilityData::Get(Sleep).Colour"),
		Indicator->CurrentColour.Equals(AbilityData::Get(EAbilitySlot::Sleep).Colour));
	TestFalse(TEXT("bIsColourMatchBonused should be false for a non-overridden application"), Indicator->bIsColourMatchBonused);

	// (c) Repeated TickControlledDuration calls strictly decrease FillFraction while
	// staying visible.
	const float SleepBaseDurationSeconds = AbilityData::Get(EAbilitySlot::Sleep).BaseDurationSeconds;
	const float DurationStepSeconds = SleepBaseDurationSeconds / 4.0f;
	float PreviousFraction = 1.0f;
	for (int32 Step = 0; Step < 3; ++Step)
	{
		Enemy->TickControlledDuration(DurationStepSeconds);
		TestTrue(TEXT("FillFraction should strictly decrease as duration advances"), Indicator->FillFraction < PreviousFraction);
		TestTrue(TEXT("bIsVisible should remain true while still Controlled"), Indicator->bIsVisible);
		PreviousFraction = Indicator->FillFraction;
	}

	// (d) Expire path: keep ticking until the duration is exhausted and the enemy
	// reverts to Alert - the indicator must hide immediately on that same tick.
	Enemy->TickControlledDuration(SleepBaseDurationSeconds);
	TestEqual(TEXT("Enemy should have reverted to Alert once the duration is exhausted"),
		static_cast<uint8>(Enemy->GetEnemyState()), static_cast<uint8>(EEnemyState::Alert));
	TestFalse(TEXT("bIsVisible should be false immediately after Controlled -> Alert reversion"), Indicator->bIsVisible);

	// (e) Bank path: a fresh Controlled enemy hides the indicator immediately on
	// TransitionToBanked().
	AEnemyBaseTestActor* BankedEnemy = NewObject<AEnemyBaseTestActor>();
	UControlledDurationIndicatorComponent* BankedIndicator = BankedEnemy->GetControlledDurationIndicatorComponent();
	BankedEnemy->TickCheckDetection(ZeroDistanceLocation); // Idle -> Alert
	BankedEnemy->ReceiveControl(EAbilitySlot::Stun); // Alert -> Controlled
	if (TestNotNull(TEXT("BankedEnemy should own a ControlledDurationIndicatorComponent"), BankedIndicator))
	{
		TestTrue(TEXT("bIsVisible should be true after ReceiveControl, before banking"), BankedIndicator->bIsVisible);
	}
	BankedEnemy->TransitionToBanked(); // Controlled -> Banked
	if (BankedIndicator)
	{
		TestFalse(TEXT("bIsVisible should be false immediately after TransitionToBanked"), BankedIndicator->bIsVisible);
	}

	// (f) Regression guard for the RegisterComponent()-requires-World gotcha: the
	// entire (b)-(e) sequence must complete without crashing on an actor with
	// explicitly no World (the default for every NewObject<>() call with no Outer,
	// matching every case above), and reflected state must behave identically.
	AEnemyBaseTestActor* NoWorldEnemy = NewObject<AEnemyBaseTestActor>();
	UControlledDurationIndicatorComponent* NoWorldIndicator = NoWorldEnemy->GetControlledDurationIndicatorComponent();
	if (!TestNotNull(TEXT("NoWorldEnemy should own a ControlledDurationIndicatorComponent"), NoWorldIndicator))
	{
		return false;
	}
	TestNull(TEXT("Sanity: NoWorldEnemy should have no World"), NoWorldEnemy->GetWorld());
	NoWorldEnemy->TickCheckDetection(ZeroDistanceLocation); // Idle -> Alert
	NoWorldEnemy->ReceiveControl(EAbilitySlot::Root); // Alert -> Controlled
	TestTrue(TEXT("(f) bIsVisible should be true after ReceiveControl even with no World"), NoWorldIndicator->bIsVisible);
	TestEqual(TEXT("(f) FillFraction should be 1.0 after ReceiveControl even with no World"), NoWorldIndicator->FillFraction, 1.0f);
	TestFalse(TEXT("(f) bIsColourMatchBonused should be false for AEnemyBaseTestActor, which never overrides duration"), NoWorldIndicator->bIsColourMatchBonused);
	const float RootBaseDurationSeconds = AbilityData::Get(EAbilitySlot::Root).BaseDurationSeconds;
	NoWorldEnemy->TickControlledDuration(RootBaseDurationSeconds); // exhaust -> Alert
	TestEqual(TEXT("(f) NoWorldEnemy should have reverted to Alert once the duration is exhausted"),
		static_cast<uint8>(NoWorldEnemy->GetEnemyState()), static_cast<uint8>(EEnemyState::Alert));
	TestFalse(TEXT("(f) bIsVisible should be false after Controlled -> Alert reversion even with no World"), NoWorldIndicator->bIsVisible);

	AEnemyBaseTestActor* NoWorldBankedEnemy = NewObject<AEnemyBaseTestActor>();
	UControlledDurationIndicatorComponent* NoWorldBankedIndicator = NoWorldBankedEnemy->GetControlledDurationIndicatorComponent();
	NoWorldBankedEnemy->TickCheckDetection(ZeroDistanceLocation); // Idle -> Alert
	NoWorldBankedEnemy->ReceiveControl(EAbilitySlot::Snare); // Alert -> Controlled
	NoWorldBankedEnemy->TransitionToBanked(); // Controlled -> Banked
	if (NoWorldBankedIndicator)
	{
		TestFalse(TEXT("(f) bIsVisible should be false after TransitionToBanked even with no World"), NoWorldBankedIndicator->bIsVisible);
	}

	// (g) REQ-3 sibling-offset guard: the bar's world-space offset must differ from
	// UEnemyTypeIndicatorComponent's, so the two never render stacked at the same
	// height. Needs a World-backed spawn so FillMeshComponent/MarkerTextComponent are
	// actually registered before reading their RelativeLocation.
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	if (TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
	{
		ABomberEnemy* WorldBomber = World->SpawnActor<ABomberEnemy>();
		if (TestNotNull(TEXT("ABomberEnemy should spawn into the test World"), WorldBomber))
		{
			WorldBomber->TickCheckDetection(ZeroDistanceLocation); // Idle -> Alert
			WorldBomber->ReceiveControl(EAbilitySlot::Fear); // Alert -> Controlled

			// SpawnActor() in this test World is never driven through
			// World->InitializeActorsForPlay/BeginPlay (see KrowdKontrolEnemyBaseTest.cpp
			// case (t)'s comment on the same limitation), so EnemyTypeIndicatorComponent's
			// own BeginPlay()-triggered InitializeMarkerVisual() never runs here - drive it
			// directly, same as KrowdKontrolEnemyTypeIndicatorComponentTest.cpp case (b)
			// does for its own World-backed spawns.
			if (WorldBomber->EnemyTypeIndicatorComponent)
			{
				WorldBomber->EnemyTypeIndicatorComponent->InitializeMarkerVisual();
			}

			UControlledDurationIndicatorComponent* WorldIndicator = WorldBomber->GetControlledDurationIndicatorComponent();
			if (TestNotNull(TEXT("WorldBomber should own a ControlledDurationIndicatorComponent"), WorldIndicator)
				&& TestNotNull(TEXT("WorldIndicator->FillMeshComponent should be created in a World-backed context"), WorldIndicator->FillMeshComponent.Get())
				&& TestNotNull(TEXT("WorldBomber should own an EnemyTypeIndicatorComponent"), WorldBomber->EnemyTypeIndicatorComponent.Get())
				&& TestNotNull(TEXT("WorldBomber's EnemyTypeIndicatorComponent should have a MarkerTextComponent"), WorldBomber->EnemyTypeIndicatorComponent->MarkerTextComponent.Get()))
			{
				const float DurationBarZ = WorldIndicator->FillMeshComponent->GetRelativeLocation().Z;
				const float TypeMarkerZ = WorldBomber->EnemyTypeIndicatorComponent->MarkerTextComponent->GetRelativeLocation().Z;
				TestNotEqual(TEXT("(g) The duration bar's world-space Z offset should differ from the type marker's (REQ-3: siblings, not stacked)"),
					DurationBarZ, TypeMarkerZ);

				// (g2) Idempotent-init guard: a second InitializeIndicatorVisual() call (the
				// shape Show() takes on a later ReceiveControl() in the same PIE session)
				// must not re-create FillMeshComponent.
				UStaticMeshComponent* FillMeshBeforeReinit = WorldIndicator->FillMeshComponent.Get();
				WorldIndicator->InitializeIndicatorVisual();
				TestEqual(TEXT("(g2) A second InitializeIndicatorVisual() call must not replace the existing FillMeshComponent"),
					WorldIndicator->FillMeshComponent.Get(), FillMeshBeforeReinit);

				// (g3) Left-anchored drain math: as FillFraction drops below 1.0, the mesh's
				// relative X location must move negative (left) so its left edge stays fixed
				// rather than shrinking symmetrically about the enemy's centre.
				const float RelativeXAtFullFraction = WorldIndicator->FillMeshComponent->GetRelativeLocation().X;
				// issue #65: B0-0MR's GetControlledDurationOverrideSeconds now returns a 7s
				// colour-match bonus for Fear, not the 5s AbilityData::Get(Fear).BaseDurationSeconds
				// baseline - read the actually-applied duration off the enemy itself, same fix
				// KrowdKontrolBomberEnemyTest.cpp case (t) already applies.
				const float FearDurationSeconds = WorldBomber->GetTotalControlledSeconds();
				WorldBomber->TickControlledDuration(FearDurationSeconds / 2.0f);
				TestTrue(TEXT("(g3) FillMeshComponent's relative X should move negative (left) as FillFraction drains below 1.0"),
					WorldIndicator->FillMeshComponent->GetRelativeLocation().X < RelativeXAtFullFraction);

				// (g4) issue #316 readability guard: ApplyBodyChainColourTint() tints the
				// enemy's root mesh via its own BodyChainColourMaterialInstance, a completely
				// separate component and material instance from the Controlled-duration bar's
				// FillMeshComponent/FillMaterialInstance - so the bar's rendered colour can
				// never be affected by the body tint regardless of chain colour. This
				// automated check stands in for the MCP-viewport-capture verification the
				// issue calls for, which has no implementable path: no MCP primitive exists to
				// drive a live PIE enemy into Controlled state (see this project's established
				// holdout limitations).
				WorldBomber->ApplyBodyChainColourTint();
				if (TestNotNull(TEXT("(g4) WorldBomber's root mesh should be tinted after ApplyBodyChainColourTint()"), WorldBomber->BodyChainColourMaterialInstance.Get()))
				{
					TestNotEqual(TEXT("(g4) The body tint's material instance must be a distinct object from the Controlled-duration bar's own material instance"),
						WorldBomber->BodyChainColourMaterialInstance.Get(), WorldIndicator->FillMaterialInstance.Get());
					TestNotEqual(TEXT("(g4) The body tint is applied to the root mesh, never to the Controlled-duration bar's own mesh component"),
						static_cast<UActorComponent*>(Cast<UMeshComponent>(WorldBomber->GetRootComponent())), static_cast<UActorComponent*>(WorldIndicator->FillMeshComponent.Get()));
					TestTrue(TEXT("(g4) The Controlled-duration bar's colour must remain the controlling ability's colour, unaffected by the body tint"),
						WorldIndicator->CurrentColour.Equals(AbilityData::Get(EAbilitySlot::Fear).Colour));
				}

				// (i) issue #357: a colour-match-bonused Controlled application (B0-0MR + Fear,
				// issue #65) must both set bIsColourMatchBonused and render a visibly thicker bar
				// than a non-bonused application in the same World - the fix's whole point is a
				// perceivable difference, not just a reflected flag nobody renders.
				ATrooperEnemy* WorldTrooperNonBonused = World->SpawnActor<ATrooperEnemy>();
				if (TestNotNull(TEXT("(i) ATrooperEnemy should spawn into the test World"), WorldTrooperNonBonused))
				{
					WorldTrooperNonBonused->TickCheckDetection(ZeroDistanceLocation); // Idle -> Alert
					WorldTrooperNonBonused->ReceiveControl(EAbilitySlot::Stun); // never bonused (PR #303 AC: Stun grants no bonus against TR-UPR)
					UControlledDurationIndicatorComponent* NonBonusedIndicator = WorldTrooperNonBonused->GetControlledDurationIndicatorComponent();
					if (TestNotNull(TEXT("(i) WorldTrooperNonBonused should own a ControlledDurationIndicatorComponent"), NonBonusedIndicator)
						&& TestNotNull(TEXT("(i) NonBonusedIndicator->FillMeshComponent should be created in a World-backed context"), NonBonusedIndicator->FillMeshComponent.Get()))
					{
						TestFalse(TEXT("(i) bIsColourMatchBonused should be false for Stun on ATrooperEnemy"), NonBonusedIndicator->bIsColourMatchBonused);
						TestTrue(TEXT("(i) bIsColourMatchBonused should be true for the earlier colour-matched Fear application on WorldBomber"), WorldIndicator->bIsColourMatchBonused);
						TestTrue(TEXT("(i) The bonused bar's depth (Y scale) should be visibly larger than the non-bonused bar's, at the same FillFraction convention"),
							WorldIndicator->FillMeshComponent->GetRelativeScale3D().Y > NonBonusedIndicator->FillMeshComponent->GetRelativeScale3D().Y);
					}

					// (j) Same-instance transition guard: bIsColourMatchBonused must not stay
					// "stuck" true once a later, non-bonused application replaces a bonused one
					// on the same component - Show() assigns the flag unconditionally today, but
					// nothing else in this file would catch a future change making that
					// assignment conditional/sticky, which would silently keep rendering a
					// misleading colour-match signal (the exact perception failure issue #357
					// was filed over).
					WorldBomber->TickControlledDuration(WorldBomber->GetRemainingControlledSeconds()); // exhaust Fear -> Alert
					WorldBomber->TickCheckDetection(ZeroDistanceLocation); // no-op: already Alert
					WorldBomber->ReceiveControl(EAbilitySlot::Stun); // Bomber has no override for Stun -> not bonused
					TestFalse(TEXT("(j) bIsColourMatchBonused should flip back to false when a later application on the same component is not bonused"),
						WorldIndicator->bIsColourMatchBonused);

					// (k) The ambiguous case PR #389's pass-2 escalation flagged: a per-enemy
					// duration OVERRIDE that is NOT a colour match must apply its duration but
					// must NOT set the bonus label. No production class has such an override
					// today (every override happens to be the countered ability), so the
					// scenario is constructed by desyncing the type indicator: this Trooper's
					// Root override (8s, issue #65) still fires on ability slot alone, but the
					// matchup authority (Root counters TR-UPR) no longer agrees with the
					// indicator's stated type - exactly the "override without match" shape the
					// old OverrideSeconds >= 0 derivation mislabeled.
					ATrooperEnemy* WorldTrooperDesynced = World->SpawnActor<ATrooperEnemy>();
					if (TestNotNull(TEXT("(k) desynced ATrooperEnemy should spawn into the test World"), WorldTrooperDesynced))
					{
						WorldTrooperDesynced->FindComponentByClass<UEnemyTypeIndicatorComponent>()->EnemyType = EEnemyType::SN_1PR;
						WorldTrooperDesynced->TickCheckDetection(ZeroDistanceLocation); // Idle -> Alert
						WorldTrooperDesynced->ReceiveControl(EAbilitySlot::Root);
						TestEqual(TEXT("(k) the per-enemy override duration must still apply (8s, issue #65) regardless of the matchup"),
							WorldTrooperDesynced->GetTotalControlledSeconds(), 8.0f);
						UControlledDurationIndicatorComponent* DesyncedIndicator = WorldTrooperDesynced->GetControlledDurationIndicatorComponent();
						if (TestNotNull(TEXT("(k) desynced Trooper should own a ControlledDurationIndicatorComponent"), DesyncedIndicator))
						{
							TestFalse(TEXT("(k) an override WITHOUT a matchup agreement must not be labelled colour-match-bonused (PR #389 HIGH finding)"),
								DesyncedIndicator->bIsColourMatchBonused);
						}
					}
				}
			}
		}
	}

	// (h) Early-wake path (issue #257): being hit by a different ability while
	// Controlled by Sleep (bWakesEarlyOnOtherAbilityHit) must hide the indicator on
	// the same call that reverts the enemy to Alert - this is a distinct call site
	// from TickControlledDuration's natural-expiry Hide() (d) and TransitionToBanked's
	// (e), and was previously untested.
	AEnemyBaseTestActor* WakeEarlyEnemy = NewObject<AEnemyBaseTestActor>();
	UControlledDurationIndicatorComponent* WakeEarlyIndicator = WakeEarlyEnemy->GetControlledDurationIndicatorComponent();
	WakeEarlyEnemy->TickCheckDetection(ZeroDistanceLocation); // Idle -> Alert
	WakeEarlyEnemy->ReceiveControl(EAbilitySlot::Sleep); // Alert -> Controlled by Sleep
	if (TestNotNull(TEXT("WakeEarlyEnemy should own a ControlledDurationIndicatorComponent"), WakeEarlyIndicator))
	{
		TestTrue(TEXT("(h) bIsVisible should be true after the initial Sleep cast"), WakeEarlyIndicator->bIsVisible);
	}
	WakeEarlyEnemy->ReceiveControl(EAbilitySlot::Root); // different ability while still Controlled by Sleep -> early wake
	TestEqual(TEXT("(h) WakeEarlyEnemy should have reverted to Alert on the early wake"),
		static_cast<uint8>(WakeEarlyEnemy->GetEnemyState()), static_cast<uint8>(EEnemyState::Alert));
	if (WakeEarlyIndicator)
	{
		TestFalse(TEXT("(h) bIsVisible should be false immediately after the early-wake Controlled -> Alert reversion"), WakeEarlyIndicator->bIsVisible);
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
