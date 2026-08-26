// Confirms UMainMenuWidget (issue #324, docs/prd-main-menu.md REQ-3) builds a title
// text block, a Quit button wired to HandleQuitClicked(), and an empty, explicitly-
// sized mastery-display anchor region - and that SetMasteryDisplayContent() fills
// that anchor without any change to the surrounding layout.
//
// Deliberately does NOT let a real "quit" console command reach the Automation
// Testing Framework's own UnrealEditor-Cmd.exe process: HandleQuitClicked()'s target
// resolves via GetOwningPlayer(), which is null for a CreateWidget<T>(World, ...)
// construction (no owning local player) - UKismetSystemLibrary::QuitGame() then
// safely no-ops (TargetPC resolves null; verified directly against
// Engine/Private/KismetSystemLibrary.cpp - see plan's "Verified From Engine Source"
// section). This test proves the wiring (OnClicked is bound) and calls
// HandleQuitClicked() directly to prove that no-op path doesn't crash - same
// "prove the degrade-safely path directly" shape as
// KrowdKontrolPunishmentDebugMenuWidgetTest.cpp's null-bound-component case (f).

#include "Misc/AutomationTest.h"
#include "MainMenuWidget.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/SizeBox.h"

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

	// (a) Title text is present and non-empty.
	if (TestNotNull(TEXT("TitleText should be non-null"), ToRawPtr(Widget->TitleText)))
	{
		TestFalse(TEXT("Title text should not be empty"), Widget->GetTitleDisplayText().IsEmpty());
	}

	// (b)/(c) Quit button exists, is wired to a real handler, and activating it is
	// safe with no owning player controller (see file header comment).
	if (TestNotNull(TEXT("QuitButton should be non-null"), ToRawPtr(Widget->QuitButton)))
	{
		TestTrue(TEXT("QuitButton::OnClicked should be bound"), Widget->QuitButton->OnClicked.IsBound());
		TestNull(TEXT("Widget should have no owning player in this bare test world"), Widget->GetOwningPlayer());
		Widget->HandleQuitClicked();
	}

	// (d) Mastery-display anchor is reserved (explicit non-zero size) and starts empty;
	// SetMasteryDisplayContent() fills it.
	if (TestNotNull(TEXT("MasteryDisplayAnchor should be non-null"), ToRawPtr(Widget->MasteryDisplayAnchor)))
	{
		TestNull(TEXT("MasteryDisplayAnchor should start with no content"), Widget->MasteryDisplayAnchor->GetContent());
		TestTrue(TEXT("MasteryDisplayAnchor should have an explicit width override"), Widget->MasteryDisplayAnchor->GetWidthOverride() > 0.0f);
		TestTrue(TEXT("MasteryDisplayAnchor should have an explicit height override"), Widget->MasteryDisplayAnchor->GetHeightOverride() > 0.0f);

		UTextBlock* PlaceholderMasteryContent = NewObject<UTextBlock>(Widget);
		Widget->SetMasteryDisplayContent(PlaceholderMasteryContent);
		TestEqual(TEXT("SetMasteryDisplayContent should fill the anchor"),
			Widget->MasteryDisplayAnchor->GetContent(), Cast<UWidget>(PlaceholderMasteryContent));
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
