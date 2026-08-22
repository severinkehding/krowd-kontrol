// Confirms UAbilityUnlockPromptComponent (issue #220, PRD "Level Progression &
// Teaching Arc" REQ-3's ability-unlock half): consuming
// UAbilityUnlockComponent::OnAbilityUnlocked, each of levels 2-5 shows exactly one
// correctly-worded UOnScreenPromptWidget::ShowPrompt() call naming the unlocked
// ability, its key, and its colour-matched countered enemy type - and never re-fires
// on a repeat NotifyLevelReached call for an already-unlocked level.
//
// Each case uses its own FAutomationEditorCommonUtils::CreateNewMap() World, per this
// module's established per-scenario isolation convention (see
// KrowdKontrolAbilityMatchupNudgeComponentTest.cpp).
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the other
// KrowdKontrol.Unit.* tests.

#include "Misc/AutomationTest.h"
#include "AbilityUnlockPromptComponent.h"
#include "AbilityUnlockComponent.h"
#include "KrowdKontrolPlayerController.h"
#include "OnScreenPromptWidget.h"
#include "FlatCamera3DPrototypePawn.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Engine.h"
#include "GameFramework/Pawn.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolAbilityUnlockPromptComponentTest,
	"KrowdKontrol.Unit.AbilityUnlockPromptComponent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

namespace KrowdKontrolAbilityUnlockPromptComponentTest
{
	// Spawns a controller-backed AKrowdKontrolPlayerController with a real, live
	// OnScreenPromptWidgetInstance - mirrors
	// KrowdKontrolAbilityMatchupNudgeComponentTest.cpp's
	// SpawnControllerWithPromptWidget() verbatim (this codebase duplicates this
	// helper per test file rather than sharing it).
	AKrowdKontrolPlayerController* SpawnControllerWithPromptWidget(UWorld* World)
	{
		AKrowdKontrolPlayerController* Controller = World->SpawnActor<AKrowdKontrolPlayerController>();
		if (!Controller)
		{
			return nullptr;
		}
		Controller->Player = NewObject<ULocalPlayer>(GEngine);
		Controller->SetAsLocalPlayerController();
		World->AddController(Controller);
		Controller->DispatchBeginPlay();
		return Controller;
	}
}

bool FKrowdKontrolAbilityUnlockPromptComponentTest::RunTest(const FString& Parameters)
{
	using namespace KrowdKontrolAbilityUnlockPromptComponentTest;

	// (a) Levels 2-5 each show exactly one correctly-worded prompt.
	// (b) No re-fire on a second visit to the same level within the run.
	{
		UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
		if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
		{
			return false;
		}

		AKrowdKontrolPlayerController* Controller = SpawnControllerWithPromptWidget(World);
		if (!TestNotNull(TEXT("Controller should spawn"), Controller) ||
			!TestNotNull(TEXT("Controller should own a live OnScreenPromptWidgetInstance"), ToRawPtr(Controller->OnScreenPromptWidgetInstance)))
		{
			return false;
		}

		APawn* Owner = World->SpawnActor<APawn>();
		if (!TestNotNull(TEXT("APawn should spawn into the test World"), Owner))
		{
			return false;
		}
		UAbilityUnlockComponent* UnlockComponent = NewObject<UAbilityUnlockComponent>(Owner);
		UnlockComponent->RegisterComponent();
		UAbilityUnlockPromptComponent* PromptComponent = NewObject<UAbilityUnlockPromptComponent>(Owner);
		PromptComponent->RegisterComponent();
		UnlockComponent->OnAbilityUnlocked.AddDynamic(PromptComponent, &UAbilityUnlockPromptComponent::HandleAbilityUnlocked);

		struct FLevelExpectation
		{
			int32 Level;
			const TCHAR* ExpectedText;
		};
		const FLevelExpectation Expectations[] = {
			{ 2, TEXT("SLEEP — PRESS 2 — STRONG VS SNIPERS") },
			{ 3, TEXT("ROOT — PRESS 3 — STRONG VS TROOPERS") },
			{ 4, TEXT("FEAR — PRESS 4 — STRONG VS BOMBERS") },
			{ 5, TEXT("SNARE — PRESS 5 — STRONG VS RUNNERS") },
		};

		for (const FLevelExpectation& Expectation : Expectations)
		{
			UnlockComponent->NotifyLevelReached(Expectation.Level);
			TestTrue(*FString::Printf(TEXT("Prompt should be visible after NotifyLevelReached(%d)"), Expectation.Level),
				Controller->OnScreenPromptWidgetInstance->IsPromptVisible());
			TestEqual(*FString::Printf(TEXT("Prompt text should match the expected wording for level %d"), Expectation.Level),
				Controller->OnScreenPromptWidgetInstance->GetPromptDisplayText().ToString(),
				FString(Expectation.ExpectedText));

			Controller->OnScreenPromptWidgetInstance->AdvanceDismissTimer(UOnScreenPromptWidget::MaxPromptDurationSeconds);
			TestFalse(*FString::Printf(TEXT("Prompt should be dismissed before checking level %d"), Expectation.Level),
				Controller->OnScreenPromptWidgetInstance->IsPromptVisible());
		}

		// (b) A repeat NotifyLevelReached for an already-unlocked level must not re-fire.
		// Note: this passes because UAbilityUnlockComponent::UnlockAbility() early-returns
		// for an already-unlocked slot before OnAbilityUnlocked ever broadcasts a second
		// time, so HandleAbilityUnlocked is never invoked here - this case is a smoke-check
		// of that integration, not a standalone proof. The actual proof that
		// OnAbilityUnlocked fires at most once per ability lives in
		// KrowdKontrolAbilityUnlockSequenceTest.cpp case (f), which tests
		// UAbilityUnlockComponent directly.
		UnlockComponent->NotifyLevelReached(2);
		TestFalse(TEXT("Prompt must not re-fire on a repeat NotifyLevelReached for an already-unlocked level"),
			Controller->OnScreenPromptWidgetInstance->IsPromptVisible());
	}

	// (c) Missing-widget defensive case: no AKrowdKontrolPlayerController spawned at
	// all, so GetWorld()->GetFirstPlayerController() returns null. Must not crash and
	// must warn exactly once.
	{
		AddExpectedError(TEXT("no OnScreenPromptWidget available"), EAutomationExpectedErrorFlags::Contains, 1, false);

		UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
		if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
		{
			return false;
		}

		APawn* Owner = World->SpawnActor<APawn>();
		UAbilityUnlockPromptComponent* PromptComponent = NewObject<UAbilityUnlockPromptComponent>(Owner);
		PromptComponent->RegisterComponent();

		PromptComponent->HandleAbilityUnlocked(EAbilitySlot::Sleep);
		TestTrue(TEXT("HandleAbilityUnlocked must not crash when no OnScreenPromptWidget can be resolved"), true);
	}

	// (d) Real-pawn wiring: AFlatCamera3DPrototypePawn's constructor binds
	// AbilityUnlockComponent->OnAbilityUnlocked to its own AbilityUnlockPromptComponent
	// - a copy-paste slip there would compile cleanly and case (a)/(b)/(c) above would
	// still pass, since none of them go through the real pawn.
	{
		UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
		if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
		{
			return false;
		}

		AKrowdKontrolPlayerController* Controller = SpawnControllerWithPromptWidget(World);
		if (!TestNotNull(TEXT("Controller should spawn"), Controller) ||
			!TestNotNull(TEXT("Controller should own a live OnScreenPromptWidgetInstance"), ToRawPtr(Controller->OnScreenPromptWidgetInstance)))
		{
			return false;
		}

		AFlatCamera3DPrototypePawn* WiringPawn = World->SpawnActor<AFlatCamera3DPrototypePawn>();
		if (!TestNotNull(TEXT("AFlatCamera3DPrototypePawn should spawn into the test World"), WiringPawn))
		{
			return false;
		}
		if (!TestNotNull(TEXT("The real pawn's AbilityUnlockPromptComponent should be constructed"),
			ToRawPtr(WiringPawn->AbilityUnlockPromptComponent)))
		{
			return false;
		}

		WiringPawn->AbilityUnlockComponent->NotifyLevelReached(2);
		TestTrue(TEXT("The pawn's real constructor-time AddDynamic binding must reach AbilityUnlockPromptComponent"),
			Controller->OnScreenPromptWidgetInstance->IsPromptVisible());
		TestEqual(TEXT("Prompt text should match the expected Sleep wording via the real pawn"),
			Controller->OnScreenPromptWidgetInstance->GetPromptDisplayText().ToString(),
			FString(TEXT("SLEEP — PRESS 2 — STRONG VS SNIPERS")));
	}

	// (e) Deferred-widget race (issue #235 E2E fix): on a real level-arrival flow,
	// UAbilityUnlockLevelSubsystem::HandleLevelBegin() can call NotifyLevelReached() -
	// and so broadcast OnAbilityUnlocked - before any OnScreenPromptWidget is
	// resolvable (AKrowdKontrolPlayerController::CreateHUDWidgets() hasn't run yet).
	// Since OnAbilityUnlocked only ever broadcasts once per ability, a prompt dropped
	// at that moment would otherwise never show. Must be buffered and shown once
	// FlushPendingPrompts() runs - AKrowdKontrolPlayerController::WireWidgetsToPawn()
	// calls it for real, mirrored here directly since WireWidgetsToPawn is private.
	{
		AddExpectedError(TEXT("no OnScreenPromptWidget available"), EAutomationExpectedErrorFlags::Contains, 1, false);

		UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
		if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
		{
			return false;
		}

		APawn* Owner = World->SpawnActor<APawn>();
		UAbilityUnlockComponent* UnlockComponent = NewObject<UAbilityUnlockComponent>(Owner);
		UnlockComponent->RegisterComponent();
		UAbilityUnlockPromptComponent* PromptComponent = NewObject<UAbilityUnlockPromptComponent>(Owner);
		PromptComponent->RegisterComponent();
		UnlockComponent->OnAbilityUnlocked.AddDynamic(PromptComponent, &UAbilityUnlockPromptComponent::HandleAbilityUnlocked);

		// No controller (so no widget) exists yet - this broadcast must be buffered,
		// not dropped.
		UnlockComponent->NotifyLevelReached(2);

		AKrowdKontrolPlayerController* Controller = SpawnControllerWithPromptWidget(World);
		if (!TestNotNull(TEXT("Controller should spawn"), Controller) ||
			!TestNotNull(TEXT("Controller should own a live OnScreenPromptWidgetInstance"), ToRawPtr(Controller->OnScreenPromptWidgetInstance)))
		{
			return false;
		}

		TestFalse(TEXT("Prompt should still be unresolved before FlushPendingPrompts()"),
			Controller->OnScreenPromptWidgetInstance->IsPromptVisible());

		PromptComponent->FlushPendingPrompts();
		TestTrue(TEXT("FlushPendingPrompts should show the prompt that arrived before any widget existed"),
			Controller->OnScreenPromptWidgetInstance->IsPromptVisible());
		TestEqual(TEXT("Flushed prompt text should match the expected Sleep wording"),
			Controller->OnScreenPromptWidgetInstance->GetPromptDisplayText().ToString(),
			FString(TEXT("SLEEP — PRESS 2 — STRONG VS SNIPERS")));
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
