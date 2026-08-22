#include "QuestTrackerWidget.h"
#include "HUDChromeColours.h"
#include "TargetZone.h"
#include "EnemyBase.h"
#include "LevelLifecycleSubsystem.h"
#include "EngineUtils.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"

void UQuestTrackerWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	EnsureWidgetTreeBuilt();
	BindToLevelLifecycle();
}

bool UQuestTrackerWidget::Initialize()
{
	const bool bNewlyInitialized = Super::Initialize();
	if (bNewlyInitialized)
	{
		EnsureWidgetTreeBuilt();
		BindToLevelLifecycle();
	}
	return bNewlyInitialized;
}

void UQuestTrackerWidget::EnsureWidgetTreeBuilt()
{
	if (!BankedCountText)
	{
		if (!WidgetTree)
		{
			WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"), RF_Transient);
		}
		BuildWidgetTree();
		RefreshDisplay();
	}
}

void UQuestTrackerWidget::BuildWidgetTree()
{
	const FLinearColor ChromeBackgroundColor = HUDChromeColours::GetBackground();
	const FSlateColor TextColor(HUDChromeColours::GetText());

	UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("QuestTrackerRootCanvas"));
	WidgetTree->RootWidget = RootCanvas;

	ChromeBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("QuestTrackerChromeBorder"));
	ChromeBorder->SetBrushColor(ChromeBackgroundColor);

	UCanvasPanelSlot* TrackerSlot = RootCanvas->AddChildToCanvas(ChromeBorder);
	checkf(TrackerSlot, TEXT("QuestTrackerWidget: AddChildToCanvas(ChromeBorder) returned null"));
	// Top-right corner anchoring - see this class's header comment for why this
	// corner. Diagonally opposite UOnScreenPromptWidget's top-center (not a corner
	// widget) and distinct from UEnergyMeterWidget (top-left)/UAbilityCooldownTrayWidget
	// (bottom-right).
	TrackerSlot->SetAnchors(FAnchors(1.0f, 0.0f, 1.0f, 0.0f));
	TrackerSlot->SetAlignment(FVector2D(1.0f, 0.0f));
	TrackerSlot->SetAutoSize(false);
	TrackerSlot->SetSize(FVector2D(TrackerWidthPx, TrackerHeightPx));
	TrackerSlot->SetPosition(FVector2D(-TrackerMarginPx, TrackerMarginPx));

	BankedCountText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("QuestTrackerBankedCountText"));
	BankedCountText->SetColorAndOpacity(TextColor);
	ChromeBorder->SetContent(BankedCountText);
}

void UQuestTrackerWidget::BindToLevelLifecycle()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	if (ULevelLifecycleSubsystem* LifecycleSubsystem = World->GetSubsystem<ULevelLifecycleSubsystem>())
	{
		LifecycleSubsystem->OnLevelBegin.AddUniqueDynamic(this, &UQuestTrackerWidget::HandleLevelBegin);
	}
}

void UQuestTrackerWidget::HandleLevelBegin(FName MapName)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	TotalEnemyCount = 0;
	for (TActorIterator<AEnemyBase> It(World); It; ++It)
	{
		++TotalEnemyCount;
	}

	for (TActorIterator<ATargetZone> It(World); It; ++It)
	{
		It->OnActorBanked.AddUniqueDynamic(this, &UQuestTrackerWidget::HandleActorBanked);
	}

	RefreshDisplay();
}

void UQuestTrackerWidget::HandleActorBanked(AActor* BankedActor)
{
	++BankedCount;
	RefreshDisplay();
}

void UQuestTrackerWidget::RefreshDisplay()
{
	if (!BankedCountText)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UQuestTrackerWidget: BankedCountText is null on '%s' (tree not built?) - banked count will render blank."),
			*GetNameSafe(this));
		return;
	}
	BankedCountText->SetText(FText::Format(
		NSLOCTEXT("QuestTrackerWidget", "BankedCountFormat", "Robots penned: {0}/{1}"),
		FText::AsNumber(BankedCount),
		FText::AsNumber(TotalEnemyCount)));
}

FText UQuestTrackerWidget::GetQuestTrackerDisplayText() const
{
	return BankedCountText ? BankedCountText->GetText() : FText::GetEmpty();
}
