// Confirms UMainMenuWidget (issue #324, docs/prd-main-menu.md REQ-3) builds a title
// text block, a Quit button wired to HandleQuitClicked(), and an explicitly-sized
// mastery-display anchor region - and that the anchor is auto-filled at construction
// with the current Crowd Mastery total (issue #328), with SetMasteryDisplayContent()
// remaining available as an external override.
//
// Deliberately does NOT let a real "quit" console command reach the Automation
// Testing Framework's own UnrealEditor-Cmd.exe process: HandleQuitClicked()'s target
// resolves via GetOwningPlayer(), which is null for a CreateWidget<T>(World, ...)
// construction (no owning local player) - UKismetSystemLibrary::QuitGame() then
// safely no-ops (TargetPC resolves null; verified directly against
// Engine/Private/KismetSystemLibrary.cpp). This test proves the wiring (OnClicked is
// bound) and calls HandleQuitClicked() directly to prove that no-op path doesn't
// crash - same "prove the degrade-safely path directly" shape as
// KrowdKontrolPunishmentDebugMenuWidgetTest.cpp's null-bound-component case (f).

#include "Misc/AutomationTest.h"
#include "MainMenuWidget.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/SizeBox.h"
#include "CrowdMasteryTotalSubsystem.h"
#include "Engine/GameInstance.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolMainMenuWidgetTest,
	"KrowdKontrol.Unit.MainMenuWidget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolMainMenuWidgetTest::RunTest(const FString& Parameters)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
	{
		return false;
	}

	UMainMenuWidget* Widget = CreateWidget<UMainMenuWidget>(World, UMainMenuWidget::StaticClass());
	if (!TestNotNull(TEXT("UMainMenuWidget should construct"), Widget))
	{
		return false;
	}

	// (a) Title text is present and reads "KROWD KONTROL".
	if (TestNotNull(TEXT("TitleText should be non-null"), ToRawPtr(Widget->TitleText)))
	{
		TestEqual(TEXT("Title text should read KROWD KONTROL"),
			Widget->GetTitleDisplayText().ToString(), FString(TEXT("KROWD KONTROL")));
	}

	// (b)/(c) Quit button exists, is wired to a real handler, and activating it is
	// safe with no owning player controller (see file header comment).
	if (TestNotNull(TEXT("QuitButton should be non-null"), ToRawPtr(Widget->QuitButton)))
	{
		TestTrue(TEXT("QuitButton::OnClicked should be bound"), Widget->QuitButton->OnClicked.IsBound());
		TestNull(TEXT("Widget should have no owning player in this bare test world"), Widget->GetOwningPlayer());
		Widget->HandleQuitClicked();
	}

	// (d) Mastery-display anchor is reserved (explicit non-zero size) and is filled at
	// construction with the running Crowd Mastery total (issue #328,
	// docs/prd-crowd-mastery-persistence.md REQ-2) - "0" here because this bare
	// CreateNewMap() World has no GameInstance to read a real total from.
	if (TestNotNull(TEXT("MasteryDisplayAnchor should be non-null"), ToRawPtr(Widget->MasteryDisplayAnchor)))
	{
		TestTrue(TEXT("MasteryDisplayAnchor should have an explicit width override"), Widget->MasteryDisplayAnchor->GetWidthOverride() > 0.0f);
		TestTrue(TEXT("MasteryDisplayAnchor should have an explicit height override"), Widget->MasteryDisplayAnchor->GetHeightOverride() > 0.0f);
		TestNotNull(TEXT("MasteryDisplayAnchor should already hold the mastery display text at construction"), Widget->MasteryDisplayAnchor->GetContent());
		TestEqual(TEXT("Mastery display should read CROWD MASTERY: 0 with no GameInstance to read a real total from"),
			Widget->GetMasteryDisplayText().ToString(), FString(TEXT("CROWD MASTERY: 0")));

		// SetMasteryDisplayContent() remains a valid external override path - its
		// documented contract is unchanged by this issue.
		UTextBlock* PlaceholderMasteryContent = NewObject<UTextBlock>(Widget);
		Widget->SetMasteryDisplayContent(PlaceholderMasteryContent);
		TestEqual(TEXT("SetMasteryDisplayContent should override the anchor's content"),
			Widget->MasteryDisplayAnchor->GetContent(), Cast<UWidget>(PlaceholderMasteryContent));

		// SetMasteryDisplayContent(nullptr) is documented as a no-op - must not clear
		// content that's already been set.
		Widget->SetMasteryDisplayContent(nullptr);
		TestEqual(TEXT("SetMasteryDisplayContent(nullptr) should not clear existing content"),
			Widget->MasteryDisplayAnchor->GetContent(), Cast<UWidget>(PlaceholderMasteryContent));
	}

	// (d2) Injecting a real subsystem via the friend-accessible CachedMasteryTotalSubsystem
	// seam and calling RefreshMasteryDisplayText() again proves the display actually reads
	// through UCrowdMasteryTotalSubsystem::GetAccumulatedTotal(), not just the 0-default.
	UMainMenuWidget* MasteryWidget = CreateWidget<UMainMenuWidget>(World, UMainMenuWidget::StaticClass());
	if (TestNotNull(TEXT("MasteryWidget should construct"), MasteryWidget))
	{
		UGameInstance* GameInstanceOuter = NewObject<UGameInstance>();
		UCrowdMasteryTotalSubsystem* InjectedSubsystem = NewObject<UCrowdMasteryTotalSubsystem>(GameInstanceOuter);
		InjectedSubsystem->DepositRunMastery(42);
		MasteryWidget->CachedMasteryTotalSubsystem = InjectedSubsystem;
		MasteryWidget->RefreshMasteryDisplayText();
		TestEqual(TEXT("Mastery display should reflect the injected subsystem's real total"),
			MasteryWidget->GetMasteryDisplayText().ToString(), FString(TEXT("CROWD MASTERY: 42")));
	}

	// (e) Initialize() guard - must not rebuild the tree when NativeOnInitialized()
	// (invoked synchronously by CreateWidget()) already built it.
	UMainMenuWidget* GuardWidget = CreateWidget<UMainMenuWidget>(World, UMainMenuWidget::StaticClass());
	if (TestNotNull(TEXT("GuardWidget should construct"), GuardWidget))
	{
		UTextBlock* FirstTitleText = GuardWidget->TitleText;
		GuardWidget->Initialize();
		TestEqual(TEXT("Initialize() must not rebuild the tree when already built"),
			ToRawPtr(GuardWidget->TitleText), FirstTitleText);
	}

	// (f) Unbuilt tree (bare NewObject(), neither NativeOnInitialized() nor Initialize()
	// called) should degrade to empty display rather than crash.
	UMainMenuWidget* UnbuiltWidget = NewObject<UMainMenuWidget>();
	if (TestNotNull(TEXT("UnbuiltWidget should construct"), UnbuiltWidget))
	{
		TestTrue(TEXT("GetTitleDisplayText should degrade to empty text before the tree is built"),
			UnbuiltWidget->GetTitleDisplayText().IsEmpty());

		// (f) continued - SetMasteryDisplayContent() on an unbuilt widget (MasteryDisplayAnchor
		// still null) must warn-and-no-op rather than crash - the other documented no-op
		// condition, alongside the null-Content case already covered in block (d).
		// Register the expected warning with a count (suite convention -
		// LevelClearTimeSubsystemTest et al.) so the run doesn't park in
		// succeededWithWarnings and the fired-exactly-once contract is asserted
		// (PR #333 review).
		AddExpectedError(TEXT("MasteryDisplayAnchor is null"), EAutomationExpectedErrorFlags::Contains, 1);
		UTextBlock* UnusedContent = NewObject<UTextBlock>(UnbuiltWidget);
		UnbuiltWidget->SetMasteryDisplayContent(UnusedContent);
		TestTrue(TEXT("SetMasteryDisplayContent on an unbuilt widget should not crash"), true);
	}

	// (g) NativeOnInitialized() invoked directly, bypassing Initialize() - WidgetTree is
	// still null at this point, exercising EnsureWidgetTreeBuilt()'s null-WidgetTree guard
	// (issue #66 precedent, ported from UAbilityCooldownTrayWidget). This bare NewObject()
	// has no World, so PopulateLevelSelectButtons() (issue #325) also hits its
	// no-ULevelSequenceSubsystem warning path here - registered below per suite convention
	// (see block (f)'s identical AddExpectedError rationale).
	AddExpectedError(TEXT("no ULevelSequenceSubsystem available"), EAutomationExpectedErrorFlags::Contains, 1);
	UMainMenuWidget* BypassWidget = NewObject<UMainMenuWidget>();
	if (TestNotNull(TEXT("BypassWidget should construct"), BypassWidget))
	{
		BypassWidget->NativeOnInitialized();
		TestNotNull(TEXT("TitleText should be built after direct NativeOnInitialized()"), ToRawPtr(BypassWidget->TitleText));
		TestNotNull(TEXT("QuitButton should be built after direct NativeOnInitialized()"), ToRawPtr(BypassWidget->QuitButton));
		TestNotNull(TEXT("MasteryDisplayAnchor should be built after direct NativeOnInitialized()"), ToRawPtr(BypassWidget->MasteryDisplayAnchor));
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
