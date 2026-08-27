#include "PostRunSummaryWidget.h"
#include "HUDChromeColours.h"
#include "LevelClearTimeSubsystem.h"
#include "CrowdMasterySubsystem.h"
#include "LevelLifecycleSubsystem.h"
#include "LevelSequenceSubsystem.h"
#include "KrowdKontrolPlayerController.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/PanelSlot.h"
#include "Components/SizeBox.h"
#include "Components/VerticalBox.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"

void UPostRunSummaryWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	EnsureWidgetTreeBuilt();
	BindToLevelLifecycle();
}

bool UPostRunSummaryWidget::Initialize()
{
	const bool bNewlyInitialized = Super::Initialize();
	if (bNewlyInitialized)
	{
		EnsureWidgetTreeBuilt();
		BindToLevelLifecycle();
	}
	return bNewlyInitialized;
}

void UPostRunSummaryWidget::EnsureWidgetTreeBuilt()
{
	// Whichever of NativeOnInitialized()/Initialize() fires first builds the tree;
	// the other is then a no-op, regardless of engine call order between the two.
	if (!ClearTimeText)
	{
		// UUserWidget::WidgetTree is normally lazily created inside Initialize()
		// (before it conditionally calls NativeOnInitialized()) - but
		// NativeOnInitialized() can also be invoked directly, bypassing Initialize()
		// entirely (this class's own Automation test exercises that call order to
		// prove idempotency). WidgetTree would still be null in that case, and
		// WidgetTree->ConstructWidget<T>() on a null WidgetTree doesn't crash on the
		// call itself - it silently passes a null Outer into NewObject<T>(), which
		// the engine then treats as fatal. Mirror UUserWidget::Initialize()'s own
		// lazy-creation exactly so this is safe regardless of call order - same fix
		// as UAbilityCooldownTrayWidget::EnsureWidgetTreeBuilt() (issue #66).
		if (!WidgetTree)
		{
			WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"), RF_Transient);
		}
		BuildWidgetTree();
		SetSummaryValues(PlaceholderClearTimeSeconds, PlaceholderBestClearTimeSeconds, PlaceholderCrowdMasteryCount);
	}
}

void UPostRunSummaryWidget::BuildWidgetTree()
{
	// First UCanvasPanel usage needed here for the same reason
	// UPunishmentDebugMenuWidget::BuildWidgetTree() needs one (issue #319): centering
	// the content block requires a UCanvasPanelSlot, which only a UCanvasPanel child
	// gets - a bare UBorder root has no anchor/alignment mechanism at all.
	UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("SummaryRootCanvas"));
	WidgetTree->RootWidget = RootCanvas;

	// Chrome background/text come from HUDChromeColours (issue #93), shared across all
	// HUD widgets - stays inside PRD 11 REQ-2's desaturated white/gray/black base
	// palette and outside MISSION.md Hard Invariant 3's reserved gameplay colours.
	RootBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("SummaryRootBorder"));
	RootBorder->SetBrushColor(HUDChromeColours::GetBackground());

	// Width cap + auto height, matching UQuestTrackerWidget's issue #310 fix -
	// AutoWrapText below (not a fixed-size slot) is what turns "too wide" into
	// "taller", not "clipped off-screen", at the 1280x720 minimum target resolution.
	// MaxDesiredHeight caps how much taller that growth can push the block, so it stays
	// within the same 1280x720 minimum target resolution vertically too.
	USizeBox* WidthCap = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("SummaryWidthCap"));
	WidthCap->SetWidthOverride(ContentWidthPx);
	WidthCap->SetMaxDesiredHeight(ContentHeightPx);
	WidthCap->SetContent(RootBorder);

	UCanvasPanelSlot* ContentSlot = RootCanvas->AddChildToCanvas(WidthCap);
	checkf(ContentSlot, TEXT("UPostRunSummaryWidget: AddChildToCanvas(WidthCap) returned null"));
	// Centred on both axes (issue #319 / Post-Run Progression PRD REQ-1's locked
	// 2026-08-26 operator design decision) - mirrors
	// UPunishmentDebugMenuWidget::BuildWidgetTree()'s identical centering call.
	ContentSlot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
	ContentSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	ContentSlot->SetAutoSize(true);

	UVerticalBox* Layout = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("SummaryLayout"));
	UPanelSlot* LayoutSlot = RootBorder->SetContent(Layout);
	checkf(LayoutSlot, TEXT("UPostRunSummaryWidget: SetContent(Layout) returned null"));

	const FSlateColor TextColor(HUDChromeColours::GetText());

	ClearTimeText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ClearTimeText"));
	ClearTimeText->SetColorAndOpacity(TextColor);
	ClearTimeText->SetAutoWrapText(true);
	Layout->AddChildToVerticalBox(ClearTimeText);

	BestClearTimeText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("BestClearTimeText"));
	BestClearTimeText->SetColorAndOpacity(TextColor);
	BestClearTimeText->SetAutoWrapText(true);
	Layout->AddChildToVerticalBox(BestClearTimeText);

	CrowdMasteryText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CrowdMasteryText"));
	CrowdMasteryText->SetColorAndOpacity(TextColor);
	CrowdMasteryText->SetAutoWrapText(true);
	Layout->AddChildToVerticalBox(CrowdMasteryText);

	RerunButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("SummaryRerunButton"));
	RerunButton->OnClicked.AddDynamic(this, &UPostRunSummaryWidget::HandleRerunClicked);

	RerunButtonLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SummaryRerunButtonLabel"));
	RerunButtonLabel->SetColorAndOpacity(TextColor);
	RerunButtonLabel->SetText(NSLOCTEXT("PostRunSummaryWidget", "RerunLevel", "RERUN LEVEL"));
	RerunButtonLabel->SetAutoWrapText(true);
	RerunButton->SetContent(RerunButtonLabel);

	Layout->AddChildToVerticalBox(RerunButton);

	NextLevelButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("SummaryNextLevelButton"));
	NextLevelButton->OnClicked.AddDynamic(this, &UPostRunSummaryWidget::HandleNextLevelClicked);

	NextLevelButtonLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SummaryNextLevelButtonLabel"));
	NextLevelButtonLabel->SetColorAndOpacity(TextColor);
	NextLevelButtonLabel->SetText(NSLOCTEXT("PostRunSummaryWidget", "NextLevel", "NEXT LEVEL"));
	NextLevelButtonLabel->SetAutoWrapText(true);
	NextLevelButton->SetContent(NextLevelButtonLabel);

	Layout->AddChildToVerticalBox(NextLevelButton);
}

void UPostRunSummaryWidget::SetSummaryValues(float ClearTimeSeconds, float BestClearTimeSeconds, int32 CrowdMasteryCount)
{
	const FText ClearTimeDisplay = FText::Format(
		NSLOCTEXT("PostRunSummaryWidget", "ClearTimeFormat", "Clear Time: {0}"),
		FormatClockSeconds(ClearTimeSeconds));
	SetTextBlockSafe(ClearTimeText, ClearTimeDisplay, TEXT("ClearTimeText"));

	const FText BestClearTimeDisplay = FText::Format(
		NSLOCTEXT("PostRunSummaryWidget", "BestClearTimeFormat", "Best: {0}"),
		FormatClockSeconds(BestClearTimeSeconds));
	SetTextBlockSafe(BestClearTimeText, BestClearTimeDisplay, TEXT("BestClearTimeText"));

	const FText CrowdMasteryDisplay = FText::Format(
		NSLOCTEXT("PostRunSummaryWidget", "CrowdMasteryFormat", "Crowd Mastery: {0}"),
		FText::AsNumber(FMath::Max(0, CrowdMasteryCount)));
	SetTextBlockSafe(CrowdMasteryText, CrowdMasteryDisplay, TEXT("CrowdMasteryText"));
}

FText UPostRunSummaryWidget::FormatClockSeconds(float TotalSeconds)
{
	const int32 ClampedSeconds = FMath::Max(0, FMath::RoundToInt(TotalSeconds));
	return FText::Format(
		NSLOCTEXT("PostRunSummaryWidget", "ClockFormat", "{0}:{1}"),
		FText::AsNumber(ClampedSeconds / 60),
		FText::FromString(FString::Printf(TEXT("%02d"), ClampedSeconds % 60)));
}

void UPostRunSummaryWidget::SetTextBlockSafe(UTextBlock* TextBlock, const FText& Text, const TCHAR* FieldName) const
{
	if (TextBlock)
	{
		TextBlock->SetText(Text);
	}
	else
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UPostRunSummaryWidget: %s is null on '%s' - field will render blank."),
			FieldName, *GetNameSafe(this));
	}
}

FText UPostRunSummaryWidget::GetClearTimeDisplayText() const
{
	return ClearTimeText ? ClearTimeText->GetText() : FText::GetEmpty();
}

FText UPostRunSummaryWidget::GetBestClearTimeDisplayText() const
{
	return BestClearTimeText ? BestClearTimeText->GetText() : FText::GetEmpty();
}

FText UPostRunSummaryWidget::GetCrowdMasteryDisplayText() const
{
	return CrowdMasteryText ? CrowdMasteryText->GetText() : FText::GetEmpty();
}

FText UPostRunSummaryWidget::GetNextLevelButtonDisplayText() const
{
	return NextLevelButtonLabel ? NextLevelButtonLabel->GetText() : FText::GetEmpty();
}

void UPostRunSummaryWidget::BindToLevelLifecycle()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	if (ULevelLifecycleSubsystem* LifecycleSubsystem = World->GetSubsystem<ULevelLifecycleSubsystem>())
	{
		LifecycleSubsystem->OnLevelClear.AddUniqueDynamic(this, &UPostRunSummaryWidget::HandleLevelClear);
	}
}

void UPostRunSummaryWidget::HandleLevelClear()
{
	UWorld* World = GetWorld();
	const FName LevelID = World ? FName(*World->GetMapName()) : NAME_None;

	float RunClearTimeSeconds = 0.0f;
	float BestClearTimeSeconds = 0.0f;
	if (ULevelClearTimeSubsystem* ClearTimeSubsystem = ResolveLevelClearTimeSubsystem())
	{
		RunClearTimeSeconds = ClearTimeSubsystem->GetLastClearTimeSeconds();
		ClearTimeSubsystem->GetBestClearTimeSeconds(LevelID, BestClearTimeSeconds);
	}

	int32 CrowdMasteryCount = 0;
	if (UCrowdMasterySubsystem* CrowdMasterySubsystem = World ? World->GetSubsystem<UCrowdMasterySubsystem>() : nullptr)
	{
		CrowdMasteryCount = CrowdMasterySubsystem->GetRunningMaxControlledCount();
	}

	if (ULevelSequenceSubsystem* SequenceSubsystem = World ? World->GetSubsystem<ULevelSequenceSubsystem>() : nullptr)
	{
		ResolvedNextLevelMapName = SequenceSubsystem->ComputeNextLevelMapName();
	}
	else
	{
		ResolvedNextLevelMapName = NAME_None;
		UE_LOG(LogTemp, Warning,
			TEXT("UPostRunSummaryWidget::HandleLevelClear: no ULevelSequenceSubsystem available on '%s' - next-level button will show the final-level state."),
			*GetNameSafe(this));
	}
	const FText NextLevelLabel = ResolvedNextLevelMapName == NAME_None
		? NSLOCTEXT("PostRunSummaryWidget", "FinishRun", "FINISH RUN (More Levels Coming)")
		: NSLOCTEXT("PostRunSummaryWidget", "NextLevel", "NEXT LEVEL");
	SetTextBlockSafe(NextLevelButtonLabel, NextLevelLabel, TEXT("NextLevelButtonLabel"));

	SetSummaryValues(RunClearTimeSeconds, BestClearTimeSeconds, CrowdMasteryCount);
	AddToViewport();

	// Issue #320 AC: the post-clear screen's primary action must work via both mouse
	// click and keyboard (Enter/Space) without requiring the player to Tab into it
	// first. Scoped to only fire here (once gameplay is already over for this run), not
	// globally in BeginPlay(), so this cannot affect in-level WASD/ability input routing.
	// Focuses NextLevelButton, not RerunButton (issue #342 bundled fix) - continuing
	// forward (NEXT LEVEL / FINISH RUN) is the more-primary action on this screen than
	// retrying, and until this fix NEXT LEVEL was unreachable for keyboard-only players
	// unless Tab-navigation happened to work.
	if (APlayerController* OwningController = GetOwningPlayer())
	{
		FInputModeGameAndUI InputMode;
		if (NextLevelButton)
		{
			InputMode.SetWidgetToFocus(NextLevelButton->TakeWidget());
		}
		OwningController->SetInputMode(InputMode);
	}
	else if (!bHasWarnedMissingOwningControllerForFocus)
	{
		bHasWarnedMissingOwningControllerForFocus = true;
		UE_LOG(LogTemp, Warning,
			TEXT("UPostRunSummaryWidget::HandleLevelClear: no owning player on '%s' - RerunButton will not receive keyboard focus."),
			*GetNameSafe(this));
	}
}

void UPostRunSummaryWidget::HandleRerunClicked()
{
	if (AKrowdKontrolPlayerController* PlayerController = Cast<AKrowdKontrolPlayerController>(GetOwningPlayer()))
	{
		// bFreshRun=true (issue #342): this is a voluntary post-clear rerun, not a
		// defeat-restart - it must not inherit a latched boss checkpoint or flip
		// WasRestartRequested(), both of which are defeat-only affordances.
		PlayerController->RequestLevelRestart(/*bFreshRun=*/true);
	}
	else if (!bHasWarnedMissingOwningControllerOnRerun)
	{
		bHasWarnedMissingOwningControllerOnRerun = true;
		UE_LOG(LogTemp, Warning,
			TEXT("UPostRunSummaryWidget::HandleRerunClicked: owning player is not an AKrowdKontrolPlayerController on '%s' - rerun will not run."),
			*GetNameSafe(this));
	}
}

void UPostRunSummaryWidget::HandleNextLevelClicked()
{
	if (ResolvedNextLevelMapName == NAME_None)
	{
		if (AKrowdKontrolPlayerController* PlayerController = Cast<AKrowdKontrolPlayerController>(GetOwningPlayer()))
		{
			// bFreshRun=true (issue #342): same "voluntary, not defeat" reasoning as
			// HandleRerunClicked() above - the final-level FINISH RUN fallback reruns the
			// current level, it is not a death.
			PlayerController->RequestLevelRestart(/*bFreshRun=*/true);
		}
		else if (!bHasWarnedMissingOwningController)
		{
			bHasWarnedMissingOwningController = true;
			UE_LOG(LogTemp, Warning,
				TEXT("UPostRunSummaryWidget::HandleNextLevelClicked: owning player is not an AKrowdKontrolPlayerController on '%s' - final-level restart will not run."),
				*GetNameSafe(this));
		}
		return;
	}
	if (ULevelSequenceSubsystem* SequenceSubsystem = GetWorld() ? GetWorld()->GetSubsystem<ULevelSequenceSubsystem>() : nullptr)
	{
		SequenceSubsystem->AdvanceToNextLevel();
	}
	else if (!bHasWarnedMissingSequenceSubsystemOnClick)
	{
		bHasWarnedMissingSequenceSubsystemOnClick = true;
		UE_LOG(LogTemp, Warning,
			TEXT("UPostRunSummaryWidget::HandleNextLevelClicked: no ULevelSequenceSubsystem available on '%s' - next-level advance will not run."),
			*GetNameSafe(this));
	}
}

ULevelClearTimeSubsystem* UPostRunSummaryWidget::ResolveLevelClearTimeSubsystem()
{
	if (CachedLevelClearTimeSubsystem)
	{
		return CachedLevelClearTimeSubsystem;
	}
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		CachedLevelClearTimeSubsystem = GameInstance->GetSubsystem<ULevelClearTimeSubsystem>();
	}
	if (!CachedLevelClearTimeSubsystem && !bHasWarnedMissingLevelClearTimeSubsystem)
	{
		bHasWarnedMissingLevelClearTimeSubsystem = true;
		UE_LOG(LogTemp, Warning,
			TEXT("UPostRunSummaryWidget: no ULevelClearTimeSubsystem available - ")
			TEXT("clear-time/best-time fields will show 0 on the next level clear."));
	}
	return CachedLevelClearTimeSubsystem;
}

namespace
{
	// Dev-only trigger (issue #74 E2E gap): the widget was previously only ever
	// constructed directly by test code (CreateWidget/NewObject in
	// KrowdKontrolPostRunSummaryWidgetTest.cpp), leaving no in-game path an E2E pass
	// could use to visually confirm the rendered clear-time/Crowd-Mastery text or
	// chrome colours. Adds the widget to the local player's viewport with its
	// existing placeholder values - this is an observation path only, not a real
	// gameplay trigger (a real "run cleared" event, HandleLevelClear() above, is
	// what production code uses since issue #175).
	void ShowPostRunSummary(const TArray<FString>& Args, UWorld* World)
	{
		APlayerController* PlayerController = World ? World->GetFirstPlayerController() : nullptr;
		if (!PlayerController)
		{
			return;
		}
		if (UPostRunSummaryWidget* Widget = CreateWidget<UPostRunSummaryWidget>(PlayerController, UPostRunSummaryWidget::StaticClass()))
		{
			Widget->AddToViewport();
		}
	}

	FAutoConsoleCommandWithWorldAndArgs ShowPostRunSummaryCommand(
		TEXT("KrowdKontrol.ShowPostRunSummary"),
		TEXT("Dev-only: adds UPostRunSummaryWidget to the local player's viewport with its placeholder clear-time/Crowd-Mastery values, so it can be visually confirmed through a real player-observable path rather than only direct construction in test code."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ShowPostRunSummary));
}
