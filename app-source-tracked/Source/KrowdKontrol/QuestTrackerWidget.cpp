#include "QuestTrackerWidget.h"
#include "HUDChromeColours.h"
#include "TargetZone.h"
#include "EnemyBase.h"
#include "WaveSpawnerComponent.h"
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

		// Late-subscribe catch-up - see this function's header comment and
		// ULevelLifecycleSubsystem::HasLevelBegun()'s comment for why this widget's
		// creation isn't guaranteed to precede the broadcast it just subscribed to.
		if (LifecycleSubsystem->HasLevelBegun())
		{
			HandleLevelBegin(NAME_None);
		}
	}
}

void UQuestTrackerWidget::HandleLevelBegin(FName MapName)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	RecountTotalEnemies();

	for (TActorIterator<ATargetZone> It(World); It; ++It)
	{
		It->OnActorBanked.AddUniqueDynamic(this, &UQuestTrackerWidget::HandleActorBanked);
	}

	// TActorIterator<AActor> + GetComponents(), not a bare
	// TObjectIterator<UWaveSpawnerComponent>: same rationale as
	// ULevelLifecycleSubsystem::RefreshLevelClearState() - avoids picking up a spawner
	// from another concurrently-loaded Automation test world.
	for (TActorIterator<AActor> ActorIt(World); ActorIt; ++ActorIt)
	{
		TArray<UWaveSpawnerComponent*> Spawners;
		ActorIt->GetComponents<UWaveSpawnerComponent>(Spawners);
		for (UWaveSpawnerComponent* Spawner : Spawners)
		{
			Spawner->OnWaveSpawned.AddUniqueDynamic(this, &UQuestTrackerWidget::HandleWaveSpawned);
		}
	}

	RefreshDisplay();
}

void UQuestTrackerWidget::HandleActorBanked(AActor* BankedActor)
{
	if (BankedActors.ContainsByPredicate(
		[BankedActor](const TWeakObjectPtr<AActor>& Existing) { return Existing.Get() == BankedActor; }))
	{
		return;
	}
	BankedActors.Add(BankedActor);
	++BankedCount;
	RefreshDisplay();
}

void UQuestTrackerWidget::HandleWaveSpawned(int32 WaveIndex)
{
	RecountTotalEnemies();
	RefreshDisplay();
}

void UQuestTrackerWidget::RecountTotalEnemies()
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
	FNumberFormattingOptions NoGrouping;
	NoGrouping.SetUseGrouping(false);
	BankedCountText->SetText(FText::Format(
		NSLOCTEXT("QuestTrackerWidget", "BankedCountFormat", "Robots penned: {0}/{1}"),
		FText::AsNumber(BankedCount, &NoGrouping),
		FText::AsNumber(TotalEnemyCount, &NoGrouping)));
}

FText UQuestTrackerWidget::GetQuestTrackerDisplayText() const
{
	return BankedCountText ? BankedCountText->GetText() : FText::GetEmpty();
}
