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
				const float FearBaseDurationSeconds = AbilityData::Get(EAbilitySlot::Fear).BaseDurationSeconds;
				WorldBomber->TickControlledDuration(FearBaseDurationSeconds / 2.0f);
				TestTrue(TEXT("(g3) FillMeshComponent's relative X should move negative (left) as FillFraction drains below 1.0"),
					WorldIndicator->FillMeshComponent->GetRelativeLocation().X < RelativeXAtFullFraction);
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
