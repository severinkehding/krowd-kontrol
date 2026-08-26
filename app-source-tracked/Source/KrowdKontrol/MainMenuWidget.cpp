#include "MainMenuWidget.h"
#include "HUDChromeColours.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/PanelSlot.h"
#include "Components/VerticalBox.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/SizeBox.h"
#include "Kismet/KismetSystemLibrary.h"

void UMainMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	EnsureWidgetTreeBuilt();
}

bool UMainMenuWidget::Initialize()
{
	const bool bNewlyInitialized = Super::Initialize();
	if (bNewlyInitialized)
	{
		EnsureWidgetTreeBuilt();
	}
	return bNewlyInitialized;
}

void UMainMenuWidget::EnsureWidgetTreeBuilt()
{
	// Whichever of NativeOnInitialized()/Initialize() fires first builds the tree; the
	// other is then a no-op, regardless of engine call order between the two.
	if (!TitleText)
	{
		// UUserWidget::WidgetTree is normally lazily created inside Initialize() (before
		// it conditionally calls NativeOnInitialized()) - but NativeOnInitialized() can
		// also be invoked directly, bypassing Initialize() entirely. WidgetTree would
		// still be null in that case, and WidgetTree->ConstructWidget<T>() on a null
		// WidgetTree crashes - same fix as UAbilityCooldownTrayWidget::EnsureWidgetTreeBuilt()
		// (issue #66).
		if (!WidgetTree)
		{
			WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"), RF_Transient);
		}
		BuildWidgetTree();
	}
}

void UMainMenuWidget::BuildWidgetTree()
{
	// Chrome comes from HUDChromeColours (issue #93), shared with every other HUD
	// widget - stays inside PRD 11 REQ-2's neutral base palette and outside MISSION.md
	// Hard Invariant 3's reserved gameplay colours.
	RootBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("MainMenuRootBorder"));
	RootBorder->SetBrushColor(HUDChromeColours::GetBackground());
	WidgetTree->RootWidget = RootBorder;

	UVerticalBox* Layout = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MainMenuLayout"));
	UPanelSlot* LayoutSlot = RootBorder->SetContent(Layout);
	checkf(LayoutSlot, TEXT("UMainMenuWidget: SetContent(Layout) returned null"));

	const FSlateColor TextColor(HUDChromeColours::GetText());

	TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("MainMenuTitleText"));
	TitleText->SetColorAndOpacity(TextColor);
	TitleText->SetText(NSLOCTEXT("MainMenuWidget", "Title", "KROWD KONTROL"));
	Layout->AddChildToVerticalBox(TitleText);

	// Reserved, empty region for the Crowd Mastery display PRD - fixed-size so it
	// occupies real layout space today even though nothing has called
	// SetMasteryDisplayContent() yet (an empty USizeBox with no override collapses to
	// zero size, which would reserve nothing).
	MasteryDisplayAnchor = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("MasteryDisplayAnchor"));
	MasteryDisplayAnchor->SetWidthOverride(MasteryDisplayAnchorWidthPx);
	MasteryDisplayAnchor->SetHeightOverride(MasteryDisplayAnchorHeightPx);
	Layout->AddChildToVerticalBox(MasteryDisplayAnchor);

	QuitButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("MainMenuQuitButton"));
	QuitButton->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleQuitClicked);

	QuitButtonLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("MainMenuQuitButtonLabel"));
	QuitButtonLabel->SetColorAndOpacity(TextColor);
	QuitButtonLabel->SetText(NSLOCTEXT("MainMenuWidget", "Quit", "Quit"));
	QuitButton->SetContent(QuitButtonLabel); // UButton is a UContentWidget, not a UPanelWidget - SetContent(), not AddChild().

	Layout->AddChildToVerticalBox(QuitButton);
}

void UMainMenuWidget::SetMasteryDisplayContent(UWidget* Content)
{
	if (MasteryDisplayAnchor && Content)
	{
		MasteryDisplayAnchor->SetContent(Content);
	}
}

FText UMainMenuWidget::GetTitleDisplayText() const
{
	return TitleText ? TitleText->GetText() : FText::GetEmpty();
}

void UMainMenuWidget::HandleQuitClicked()
{
	// UKismetSystemLibrary::QuitGame's underlying PlayerController->ConsoleCommand("quit")
	// already does the right thing in both contexts this issue's AC calls out, with no
	// manual GIsEditor/IsPlayInEditor() branching needed: inside a PIE session it ends
	// PIE (never touches the running Editor process); in a packaged build or -game it
	// exits the application (Engine/Private/GameEngine.cpp's EXIT/QUIT handling - this
	// is the same "Quit Game" Blueprint node every UE main menu uses for exactly this
	// reason) - hand-rolled GIsEditor/IsPlayInEditor() context detection was considered
	// and rejected as redundant. GetOwningPlayer() resolves null in a bare
	// CreateWidget<T>(World, ...) test construction, in which case QuitGame() is a
	// guaranteed no-op (KismetSystemLibrary.cpp) - safe to call directly from a test.
	UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
}
