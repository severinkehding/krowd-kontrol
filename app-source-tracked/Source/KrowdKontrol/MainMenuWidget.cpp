#include "MainMenuWidget.h"
#include "HUDChromeColours.h"
#include "MainMenuLevelButtonWidget.h"
#include "LevelSequenceSubsystem.h"
#include "CrowdMasteryTotalSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/PanelSlot.h"
#include "Components/VerticalBox.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/SizeBox.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

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
		// WidgetTree doesn't crash on the call itself - it silently passes a null Outer
		// into NewObject<T>(), which the engine then treats as fatal. Mirror
		// UUserWidget::Initialize()'s own lazy-creation exactly so this is safe regardless
		// of call order - same fix as UAbilityCooldownTrayWidget::EnsureWidgetTreeBuilt()
		// (issue #66).
		if (!WidgetTree)
		{
			WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"), RF_Transient);
		}
		BuildWidgetTree();
		PopulateLevelSelectButtons();
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

	LevelSelectBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MainMenuLevelSelectBox"));
	Layout->AddChildToVerticalBox(LevelSelectBox);

	// Reserved, empty region for the Crowd Mastery display PRD - fixed-size so it
	// occupies real layout space today even though nothing has called
	// SetMasteryDisplayContent() yet (an empty USizeBox with no override collapses to
	// zero size, which would reserve nothing).
	MasteryDisplayAnchor = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("MasteryDisplayAnchor"));
	MasteryDisplayAnchor->SetWidthOverride(MasteryDisplayAnchorWidthPx);
	MasteryDisplayAnchor->SetHeightOverride(MasteryDisplayAnchorHeightPx);
	Layout->AddChildToVerticalBox(MasteryDisplayAnchor);

	// Crowd Mastery total display (issue #328, docs/prd-crowd-mastery-persistence.md
	// REQ-2) - fills the anchor reserved above via the widget's own already-public
	// SetMasteryDisplayContent() API (the seam #324 built specifically for this).
	MasteryDisplayText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("MasteryDisplayText"));
	MasteryDisplayText->SetColorAndOpacity(TextColor);
	MasteryDisplayText->SetAutoWrapText(true);
	SetMasteryDisplayContent(MasteryDisplayText);
	RefreshMasteryDisplayText();

	QuitButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("MainMenuQuitButton"));
	QuitButton->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleQuitClicked);

	QuitButtonLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("MainMenuQuitButtonLabel"));
	QuitButtonLabel->SetColorAndOpacity(TextColor);
	QuitButtonLabel->SetText(NSLOCTEXT("MainMenuWidget", "Quit", "Quit"));
	QuitButton->SetContent(QuitButtonLabel); // UButton is a UContentWidget, not a UPanelWidget - SetContent(), not AddChild().

	Layout->AddChildToVerticalBox(QuitButton);
}

void UMainMenuWidget::PopulateLevelSelectButtons()
{
	if (!LevelSelectBox)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UMainMenuWidget::PopulateLevelSelectButtons: LevelSelectBox is null on '%s' (tree not built?) - level-select list will be empty."),
			*GetNameSafe(this));
		return;
	}

	UWorld* World = GetWorld();
	ULevelSequenceSubsystem* SequenceSubsystem = World ? World->GetSubsystem<ULevelSequenceSubsystem>() : nullptr;
	if (!SequenceSubsystem)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UMainMenuWidget::PopulateLevelSelectButtons: no ULevelSequenceSubsystem available on '%s' - level-select list will be empty."),
			*GetNameSafe(this));
		return;
	}

	for (const FName& LevelMapName : SequenceSubsystem->GetShippedLevelMapNames())
	{
		UMainMenuLevelButtonWidget* LevelButtonWidget = WidgetTree->ConstructWidget<UMainMenuLevelButtonWidget>(
			UMainMenuLevelButtonWidget::StaticClass(),
			*FString::Printf(TEXT("MainMenuLevelButton_%s"), *LevelMapName.ToString()));
		LevelButtonWidget->SetLevelMapName(LevelMapName);
		LevelButtonWidget->OnLevelSelected.AddDynamic(this, &UMainMenuWidget::HandleLevelSelected);
		LevelSelectBox->AddChildToVerticalBox(LevelButtonWidget);
		LevelSelectButtons.Add(LevelButtonWidget);
	}
}

void UMainMenuWidget::SetMasteryDisplayContent(UWidget* Content)
{
	// Null-Content check FIRST: calling with nullptr on an unbuilt widget is the
	// documented no-op, and warning "content dropped" there would send a log reader
	// chasing a phantom bug when there was no content to drop (PR #333 review).
	if (!Content)
	{
		return; // Documented no-op: called with nullptr, nothing to set.
	}
	if (!MasteryDisplayAnchor)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UMainMenuWidget::SetMasteryDisplayContent: MasteryDisplayAnchor is null on '%s' (tree not built?) - content dropped."),
			*GetNameSafe(this));
		return;
	}
	MasteryDisplayAnchor->SetContent(Content);
}

FText UMainMenuWidget::GetTitleDisplayText() const
{
	return TitleText ? TitleText->GetText() : FText::GetEmpty();
}

void UMainMenuWidget::RefreshMasteryDisplayText()
{
	if (!MasteryDisplayText)
	{
		return;
	}

	// Silent resolution, deliberately no warn-on-missing: a missing GameInstance just
	// means "no GameInstance yet" (every KrowdKontrol.Unit.* test that constructs this
	// widget via CreateNewMap() hits this) - 0 is the correct, unremarkable default.
	int32 AccumulatedTotal = 0;
	if (CachedMasteryTotalSubsystem)
	{
		AccumulatedTotal = CachedMasteryTotalSubsystem->GetAccumulatedTotal();
	}
	else if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UCrowdMasteryTotalSubsystem* MasterySubsystem = GameInstance->GetSubsystem<UCrowdMasteryTotalSubsystem>())
		{
			CachedMasteryTotalSubsystem = MasterySubsystem;
			AccumulatedTotal = MasterySubsystem->GetAccumulatedTotal();
		}
	}

	MasteryDisplayText->SetText(FText::Format(
		NSLOCTEXT("MainMenuWidget", "CrowdMasteryTotalFormat", "CROWD MASTERY: {0}"),
		FText::AsNumber(FMath::Max(0, AccumulatedTotal))));
}

FText UMainMenuWidget::GetMasteryDisplayText() const
{
	return MasteryDisplayText ? MasteryDisplayText->GetText() : FText::GetEmpty();
}

void UMainMenuWidget::HandleQuitClicked()
{
	// UKismetSystemLibrary::QuitGame's underlying PlayerController->ConsoleCommand("quit")
	// already does the right thing in both contexts this issue's AC calls out, with no
	// manual GIsEditor/IsPlayInEditor() branching needed:
	// - In PIE, GEngine is a UEditorEngine (not UGameEngine), so this never reaches
	//   GameEngine.cpp's EXIT/QUIT handling. Instead ULocalPlayer::Exec_Editor()
	//   (Engine/Private/LocalPlayer.cpp:1620-1623) matches "Exit"/"Quit" and calls
	//   HandleExitCommand() -> ViewportClient->CloseRequested(...) (LocalPlayer.cpp:1333),
	//   which ends PIE without touching the running Editor process.
	// - In a packaged build or -game, GEngine is a UGameEngine, and
	//   Engine/Private/GameEngine.cpp:1530's EXIT/QUIT handling exits the application.
	// This is the same "Quit Game" Blueprint node every UE main menu uses for exactly
	// this reason - hand-rolled GIsEditor/IsPlayInEditor() context detection was
	// considered and rejected as redundant. GetOwningPlayer() resolves null in a bare
	// CreateWidget<T>(World, ...) test construction, in which case QuitGame() is a
	// guaranteed no-op (KismetSystemLibrary.cpp) - safe to call directly from a test.
	UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
}

void UMainMenuWidget::HandleLevelSelected(FName MapName)
{
	if (MapName == NAME_None)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UMainMenuWidget::HandleLevelSelected: MapName is NAME_None on '%s' - level-select DataTable row likely has an empty/None row name; ignoring."),
			*GetNameSafe(this));
		return;
	}

	LastSelectedLevelMapName = MapName;

	// Real map travel only makes sense in an actual game world (PIE or packaged) -
	// never in the Editor-type Worlds FAutomationEditorCommonUtils::CreateNewMap()
	// returns for KrowdKontrol.Unit.* tests, where OpenLevel would try to travel a
	// World that was never loaded from a real map package, hanging the Automation
	// run (same hazard ULevelSequenceSubsystem::AdvanceToNextLevel() documents,
	// issue #172).
	UWorld* World = GetWorld();
	if (World && World->IsGameWorld())
	{
		UGameplayStatics::OpenLevel(this, MapName);
	}
}
