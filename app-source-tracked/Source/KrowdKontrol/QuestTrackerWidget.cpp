#include "QuestTrackerWidget.h"
#include "HUDChromeColours.h"
#include "TargetZone.h"
#include "EnemyBase.h"
#include "WaveSpawnerComponent.h"
#include "LevelLifecycleSubsystem.h"
#include "AbilityData.h"
#include "AbilityUnlockComponent.h"
#include "EnemyTypeIndicatorComponent.h"
#include "RoomActor.h"
#include "EngineUtils.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
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
		RefreshSuggestedAbilityDisplay();
		RefreshRoomStateDisplay();
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

	UVerticalBox* Rows = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("QuestTrackerRows"));
	ChromeBorder->SetContent(Rows);

	BankedCountText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("QuestTrackerBankedCountText"));
	BankedCountText->SetColorAndOpacity(TextColor);
	Rows->AddChildToVerticalBox(BankedCountText);

	SuggestedAbilityText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("QuestTrackerSuggestedAbilityText"));
	Rows->AddChildToVerticalBox(SuggestedAbilityText);

	RoomStateText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("QuestTrackerRoomStateText"));
	RoomStateText->SetColorAndOpacity(TextColor);
	Rows->AddChildToVerticalBox(RoomStateText);
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

	for (TActorIterator<ARoomActor> It(World); It; ++It)
	{
		It->OnRoomClearedStateChanged.AddUniqueDynamic(this, &UQuestTrackerWidget::HandleRoomClearedStateChanged);
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
	RefreshSuggestedAbilityDisplay();
	RefreshRoomStateDisplay();
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
	RefreshSuggestedAbilityDisplay();
}

void UQuestTrackerWidget::HandleWaveSpawned(int32 WaveIndex)
{
	RecountTotalEnemies();
	RefreshDisplay();
	RefreshSuggestedAbilityDisplay();
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

void UQuestTrackerWidget::BindAbilityUnlockComponent(UAbilityUnlockComponent* UnlockComponent)
{
	if (!UnlockComponent)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UQuestTrackerWidget::BindAbilityUnlockComponent called with null component - suggested-ability line keeps its current fallback state."));
		return;
	}
	BoundUnlockComponent = UnlockComponent;
	// AddUniqueDynamic - same "safe to call more than once" rationale as
	// BindToLevelLifecycle()'s own identical idiom above.
	UnlockComponent->OnAbilityUnlocked.AddUniqueDynamic(this, &UQuestTrackerWidget::HandleAbilityUnlocked);
	RefreshSuggestedAbilityDisplay();
}

void UQuestTrackerWidget::HandleAbilityUnlocked(EAbilitySlot Ability)
{
	RefreshSuggestedAbilityDisplay();
}

EAbilitySlot UQuestTrackerWidget::ComputeSuggestedAbility() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return EAbilitySlot::Stun;
	}

	TSet<EEnemyType> RemainingTypes;
	for (TActorIterator<AEnemyBase> It(World); It; ++It)
	{
		if (It->GetEnemyState() == EEnemyState::Banked)
		{
			continue;
		}
		if (const UEnemyTypeIndicatorComponent* Indicator = It->FindComponentByClass<UEnemyTypeIndicatorComponent>())
		{
			RemainingTypes.Add(Indicator->EnemyType);
		}
	}

	const UAbilityUnlockComponent* UnlockComponent = BoundUnlockComponent.Get();
	if (UnlockComponent)
	{
		// AbilityData::GetAll() order (Stun, Sleep, Root, Fear, Snare) is this
		// function's deterministic tie-break when more than one remaining type has
		// an unlocked counter - the issue's AC doesn't specify a priority, and no
		// other existing code establishes one.
		for (const FAbilityData& Data : AbilityData::GetAll())
		{
			if (!Data.bIsColourNeutral && RemainingTypes.Contains(Data.CounteredEnemyType)
				&& UnlockComponent->IsAbilityUnlocked(Data.Ability))
			{
				return Data.Ability;
			}
		}
	}

	// Universal fallback - Stun is the only bIsColourNeutral ability (MISSION.md
	// Hard Invariant 4), so this return value doubles as the fallback sentinel
	// RefreshSuggestedAbilityDisplay() below branches on.
	return EAbilitySlot::Stun;
}

void UQuestTrackerWidget::RefreshSuggestedAbilityDisplay()
{
	if (!SuggestedAbilityText)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UQuestTrackerWidget: SuggestedAbilityText is null on '%s' (tree not built?) - suggestion will render blank."),
			*GetNameSafe(this));
		return;
	}

	const EAbilitySlot Suggested = ComputeSuggestedAbility();
	const FAbilityData& Data = AbilityData::Get(Suggested);

	const FText Line = Data.bIsColourNeutral
		? FText::Format(
			  NSLOCTEXT("QuestTrackerWidget", "SuggestedAbilityFallbackFormat", "ANY ROBOT → {0} ({1})"),
			  FText::FromString(AbilityData::GetDisplayName(Suggested)), Data.KeyBindingLabel)
		: FText::Format(
			  NSLOCTEXT("QuestTrackerWidget", "SuggestedAbilityFormat", "{0} → {1} ({2})"),
			  FText::FromString(AbilityData::GetEnemyPluralDisplayName(Data.CounteredEnemyType)),
			  FText::FromString(AbilityData::GetDisplayName(Suggested)), Data.KeyBindingLabel);

	SuggestedAbilityText->SetText(Line);
	// Genuine information swatch (Hard Invariant 3's exception) - mirrors
	// AbilityCooldownTrayWidget.cpp:174's identical SetColorAndOpacity(FSlateColor(
	// AbilityData::Get(...).Colour)) idiom. Stun's Colour is White (still one of
	// the 5 reserved colours), so the fallback line is legitimately tinted too.
	SuggestedAbilityText->SetColorAndOpacity(FSlateColor(Data.Colour));
}

FText UQuestTrackerWidget::GetSuggestedAbilityDisplayText() const
{
	return SuggestedAbilityText ? SuggestedAbilityText->GetText() : FText::GetEmpty();
}

FLinearColor UQuestTrackerWidget::GetSuggestedAbilityTextColour() const
{
	return SuggestedAbilityText ? SuggestedAbilityText->GetColorAndOpacity().GetSpecifiedColor() : FLinearColor::Black;
}

void UQuestTrackerWidget::HandleRoomClearedStateChanged()
{
	RefreshRoomStateDisplay();
}

FText UQuestTrackerWidget::GetRoomStateDisplayText() const
{
	return RoomStateText ? RoomStateText->GetText() : FText::GetEmpty();
}

void UQuestTrackerWidget::RefreshRoomStateDisplay()
{
	if (!RoomStateText)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UQuestTrackerWidget: RoomStateText is null on '%s' (tree not built?) - room-state line will render blank."),
			*GetNameSafe(this));
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		RoomStateText->SetText(FText::GetEmpty());
		return;
	}

	TArray<ARoomActor*> Rooms;
	for (TActorIterator<ARoomActor> It(World); It; ++It)
	{
		Rooms.Add(*It);
	}

	if (Rooms.Num() == 0)
	{
		// No room actors in this world (e.g. widget-only unit tests that never spawn
		// one) - stay blank rather than claiming a false "DOOR OPEN".
		RoomStateText->SetText(FText::GetEmpty());
		return;
	}

	// Chain order by world X - same convention ADoorConnectorActor::BeginPlay()'s
	// GatingRoom heuristic and KrowdKontrolLevelTestUtils::SortRoomsByX both use.
	Rooms.Sort([](const ARoomActor& A, const ARoomActor& B) { return A.GetActorLocation().X < B.GetActorLocation().X; });

	int32 FocusIndex = INDEX_NONE;
	for (int32 Index = 0; Index < Rooms.Num(); ++Index)
	{
		if (!Rooms[Index]->IsRoomCleared())
		{
			FocusIndex = Index;
			break;
		}
	}

	if (FocusIndex == INDEX_NONE)
	{
		// Every room in chain order is cleared - the last gate has opened.
		RoomStateText->SetText(NSLOCTEXT("QuestTrackerWidget", "RoomStateDoorOpen", "DOOR OPEN"));
		return;
	}

	const int32 RemainingCount = Rooms[FocusIndex]->GetRemainingEnemyCount();
	FNumberFormattingOptions NoGrouping;
	NoGrouping.SetUseGrouping(false);
	const FText Line = (RemainingCount == 1)
		? FText::Format(NSLOCTEXT("QuestTrackerWidget", "RoomStateSingularFormat", "Room {0} — {1} robot left"),
			  FText::AsNumber(FocusIndex + 1, &NoGrouping), FText::AsNumber(RemainingCount, &NoGrouping))
		: FText::Format(NSLOCTEXT("QuestTrackerWidget", "RoomStatePluralFormat", "Room {0} — {1} robots left"),
			  FText::AsNumber(FocusIndex + 1, &NoGrouping), FText::AsNumber(RemainingCount, &NoGrouping));

	RoomStateText->SetText(Line);
}
