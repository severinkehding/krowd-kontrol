#include "MasteryScreenWidget.h"
#include "HUDChromeColours.h"
#include "ReservedGameplayColours.h"
#include "CrowdMasteryTotalSubsystem.h"
#include "MasteryTreeData.h"
#include "MasterySkillBubbleWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/PanelSlot.h"
#include "Components/VerticalBox.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SizeBox.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"

namespace
{
	// Placeholder-art tuning knobs for the currently-authored ~2-node tree. The
	// AC's own "Alpha-sized tree must fit on one screen - no pan/zoom" framing
	// means these are expected to be retuned by eye once running in-editor, not
	// treated as load-bearing.
	constexpr float TreeCanvasWidthPx = 1000.0f;
	constexpr float TreeCanvasHeightPx = 800.0f;
	constexpr float TreeStartXPx = 200.0f;
	constexpr float TreeStartYPx = 140.0f;
	constexpr float NodeColumnSpacingPx = 320.0f;
	constexpr float PhaseRowSpacingPx = 260.0f;
	constexpr float NodeSizePx = 96.0f;
	constexpr float BubbleSizePx = 48.0f;
	constexpr float BubbleRingRadiusPx = 85.0f;
}

void UMasteryScreenWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	EnsureWidgetTreeBuilt();
}

bool UMasteryScreenWidget::Initialize()
{
	const bool bNewlyInitialized = Super::Initialize();
	if (bNewlyInitialized)
	{
		EnsureWidgetTreeBuilt();
	}
	return bNewlyInitialized;
}

void UMasteryScreenWidget::NativeConstruct()
{
	Super::NativeConstruct();
	// AddToViewport() (and any future re-add of this same widget instance) fires
	// NativeConstruct() - refreshing here too, not just at construction time inside
	// BuildWidgetTree(), makes "refreshed on screen open" an explicit, structural
	// guarantee - mirrors UMainMenuWidget::NativeConstruct()'s identical rationale.
	RefreshPointsDisplayText();
}

void UMasteryScreenWidget::EnsureWidgetTreeBuilt()
{
	// Whichever of NativeOnInitialized()/Initialize() fires first builds the tree; the
	// other is then a no-op, regardless of engine call order between the two - same
	// null-WidgetTree lazy-creation guard as UMainMenuWidget::EnsureWidgetTreeBuilt()
	// (issue #66 precedent).
	if (!TitleText)
	{
		if (!WidgetTree)
		{
			WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"), RF_Transient);
		}
		BuildWidgetTree();
	}
}

void UMasteryScreenWidget::BuildWidgetTree()
{
	// Chrome comes from HUDChromeColours (issue #93), shared with every other HUD
	// widget - stays inside PRD 11 REQ-2's neutral base palette and outside MISSION.md
	// Hard Invariant 3's reserved gameplay colours.
	RootBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("MasteryScreenRootBorder"));
	RootBorder->SetBrushColor(HUDChromeColours::GetBackground());
	WidgetTree->RootWidget = RootBorder;

	UVerticalBox* Layout = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MasteryScreenLayout"));
	UPanelSlot* LayoutSlot = RootBorder->SetContent(Layout);
	checkf(LayoutSlot, TEXT("UMasteryScreenWidget: SetContent(Layout) returned null"));

	const FSlateColor TextColor(HUDChromeColours::GetText());

	TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("MasteryScreenTitleText"));
	TitleText->SetColorAndOpacity(TextColor);
	TitleText->SetText(NSLOCTEXT("MasteryScreenWidget", "Title", "MASTERY"));
	Layout->AddChildToVerticalBox(TitleText);

	// Unspent-points display.
	PointsText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("MasteryScreenPointsText"));
	PointsText->SetColorAndOpacity(TextColor);
	RefreshPointsDisplayText();
	Layout->AddChildToVerticalBox(PointsText);

	// Skill tree content (issue #374) - fixed-size canvas, no pan/zoom, sized to
	// fit the currently-authored Alpha tree.
	USizeBox* TreeCanvasSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("MasteryTreeCanvasSizeBox"));
	TreeCanvasSizeBox->SetWidthOverride(TreeCanvasWidthPx);
	TreeCanvasSizeBox->SetHeightOverride(TreeCanvasHeightPx);
	TreeCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("MasteryTreeCanvas"));
	TreeCanvasSizeBox->AddChild(TreeCanvas); // USizeBox is a UContentWidget - AddChild(), same family as UBorder::SetContent()/UButton::SetContent(), just named differently.
	Layout->AddChildToVerticalBox(TreeCanvasSizeBox);
	PopulateTreeContent();

	BackButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("MasteryScreenBackButton"));
	BackButton->OnClicked.AddDynamic(this, &UMasteryScreenWidget::HandleBackClicked);

	BackButtonLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("MasteryScreenBackButtonLabel"));
	BackButtonLabel->SetColorAndOpacity(TextColor);
	BackButtonLabel->SetText(NSLOCTEXT("MasteryScreenWidget", "Back", "BACK"));
	BackButton->SetContent(BackButtonLabel); // UButton is a UContentWidget, not a UPanelWidget - SetContent(), not AddChild().

	Layout->AddChildToVerticalBox(BackButton);
}

void UMasteryScreenWidget::RefreshPointsDisplayText()
{
	if (!PointsText)
	{
		return;
	}

	// "Unspent" == GetAccumulatedTotal() - GetSpentPoints() (issue #374's bugfix -
	// this previously read GetAccumulatedTotal() verbatim, a stale carryover from
	// before #371 added spend tracking). A missing GameInstance (every
	// KrowdKontrol.Unit.* test that constructs this widget via CreateNewMap() hits
	// this) is the unremarkable default and stays unlogged; a present GameInstance
	// with no resolvable subsystem is a real failure and is warned once inside
	// ResolveMasteryTotalSubsystem() - same shape as
	// UMainMenuWidget::RefreshMasteryDisplayText().
	int32 UnspentPoints = 0;
	if (UCrowdMasteryTotalSubsystem* Subsystem = ResolveMasteryTotalSubsystem())
	{
		UnspentPoints = Subsystem->GetAccumulatedTotal() - Subsystem->GetSpentPoints();
	}

	FNumberFormattingOptions NoGrouping;
	NoGrouping.SetUseGrouping(false);
	PointsText->SetText(FText::Format(
		NSLOCTEXT("MasteryScreenWidget", "UnspentPointsFormat", "UNSPENT POINTS: {0}"),
		FText::AsNumber(UnspentPoints, &NoGrouping)));
}

FText UMasteryScreenWidget::GetPointsDisplayText() const
{
	return PointsText ? PointsText->GetText() : FText::GetEmpty();
}

void UMasteryScreenWidget::HandleBackClicked()
{
	OnBackRequested.Broadcast();
}

UCrowdMasteryTotalSubsystem* UMasteryScreenWidget::ResolveMasteryTotalSubsystem()
{
	if (CachedMasteryTotalSubsystem)
	{
		return CachedMasteryTotalSubsystem;
	}
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UCrowdMasteryTotalSubsystem* MasterySubsystem = GameInstance->GetSubsystem<UCrowdMasteryTotalSubsystem>())
		{
			CachedMasteryTotalSubsystem = MasterySubsystem;
			return MasterySubsystem;
		}
		else if (!bHasWarnedMissingMasteryTotalSubsystem)
		{
			bHasWarnedMissingMasteryTotalSubsystem = true;
			UE_LOG(LogTemp, Warning,
				TEXT("UMasteryScreenWidget::ResolveMasteryTotalSubsystem: no UCrowdMasteryTotalSubsystem available on '%s' - tree/points display will show 0/empty."),
				*GetNameSafe(this));
		}
	}
	return nullptr;
}

void UMasteryScreenWidget::PopulateTreeContent()
{
	if (!TreeCanvas)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UMasteryScreenWidget::PopulateTreeContent: TreeCanvas is null on '%s' (tree not built?) - skill tree will be empty."),
			*GetNameSafe(this));
		return;
	}
	UCrowdMasteryTotalSubsystem* Subsystem = ResolveMasteryTotalSubsystem();
	if (!Subsystem || !Subsystem->MasteryTreeTable)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UMasteryScreenWidget::PopulateTreeContent: no subsystem/MasteryTreeTable available on '%s' - skill tree will be empty."),
			*GetNameSafe(this));
		return;
	}

	UDataTable* Table = Subsystem->MasteryTreeTable;
	TArray<FName> RowsByPhase[static_cast<int32>(EMasteryTreePhase::Count)];
	for (const FName& RowName : Table->GetRowNames())
	{
		const FMasteryTreeNode* Node = Table->FindRow<FMasteryTreeNode>(RowName, TEXT("UMasteryScreenWidget::PopulateTreeContent"));
		if (!Node)
		{
			continue;
		}
		RowsByPhase[static_cast<int32>(Node->Phase)].Add(RowName);
	}

	for (int32 PhaseIndex = 0; PhaseIndex < static_cast<int32>(EMasteryTreePhase::Count); ++PhaseIndex)
	{
		TArray<FName>& PhaseRows = RowsByPhase[PhaseIndex];
		PhaseRows.Sort([](const FName& A, const FName& B) { return A.ToString() < B.ToString(); });

		for (int32 ColumnIndex = 0; ColumnIndex < PhaseRows.Num(); ++ColumnIndex)
		{
			const FMasteryTreeNode* Node = Table->FindRow<FMasteryTreeNode>(PhaseRows[ColumnIndex], TEXT("UMasteryScreenWidget::PopulateTreeContent"));
			if (!Node)
			{
				continue;
			}
			const FVector2D NodeCenter(TreeStartXPx + ColumnIndex * NodeColumnSpacingPx, TreeStartYPx + PhaseIndex * PhaseRowSpacingPx);

			UBorder* NodeBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), *FString::Printf(TEXT("MasteryTreeNode_%s"), *PhaseRows[ColumnIndex].ToString()));
			NodeBorder->SetBrushColor(ReservedGameplayColours::GetNeutralChrome());
			UCanvasPanelSlot* NodeSlot = TreeCanvas->AddChildToCanvas(NodeBorder);
			checkf(NodeSlot, TEXT("UMasteryScreenWidget: AddChildToCanvas(NodeBorder) returned null"));
			NodeSlot->SetAutoSize(false);
			NodeSlot->SetSize(FVector2D(NodeSizePx, NodeSizePx));
			NodeSlot->SetPosition(NodeCenter - FVector2D(NodeSizePx, NodeSizePx) * 0.5f);

			for (int32 BubbleIndex = 0; BubbleIndex < Node->Bubbles.Num(); ++BubbleIndex)
			{
				const FMasterySkillBubble& Bubble = Node->Bubbles[BubbleIndex];
				const float AngleRad = FMath::DegreesToRadians(-90.0f + 90.0f * BubbleIndex);
				const FVector2D BubbleCenter = NodeCenter + BubbleRingRadiusPx * FVector2D(FMath::Cos(AngleRad), FMath::Sin(AngleRad));

				UMasterySkillBubbleWidget* BubbleWidget = WidgetTree->ConstructWidget<UMasterySkillBubbleWidget>(
					UMasterySkillBubbleWidget::StaticClass(),
					*FString::Printf(TEXT("MasterySkillBubble_%s"), *Bubble.BubbleId.ToString()));
				BubbleWidget->SetBubbleData(Bubble.BubbleId, Bubble.PointCost);
				BubbleWidget->OnBubbleClicked.AddDynamic(this, &UMasteryScreenWidget::HandleBubbleClicked);

				UCanvasPanelSlot* BubbleSlot = TreeCanvas->AddChildToCanvas(BubbleWidget);
				checkf(BubbleSlot, TEXT("UMasteryScreenWidget: AddChildToCanvas(BubbleWidget) returned null"));
				BubbleSlot->SetAutoSize(false);
				BubbleSlot->SetSize(FVector2D(BubbleSizePx, BubbleSizePx));
				BubbleSlot->SetPosition(BubbleCenter - FVector2D(BubbleSizePx, BubbleSizePx) * 0.5f);

				BubbleWidgets.Add(BubbleWidget);
				BubbleWidgetsByBubbleId.Add(Bubble.BubbleId, BubbleWidget);
			}
		}
	}

	RefreshBubbleStates();
}

void UMasteryScreenWidget::RefreshBubbleStates()
{
	UCrowdMasteryTotalSubsystem* Subsystem = ResolveMasteryTotalSubsystem();
	const TArray<FName> Unlocked = Subsystem ? Subsystem->GetUnlockedBubbles() : TArray<FName>();
	const int32 AvailablePoints = Subsystem ? (Subsystem->GetAccumulatedTotal() - Subsystem->GetSpentPoints()) : 0;

	for (const TPair<FName, TObjectPtr<UMasterySkillBubbleWidget>>& Pair : BubbleWidgetsByBubbleId)
	{
		UMasterySkillBubbleWidget* BubbleWidget = Pair.Value;
		if (!BubbleWidget)
		{
			continue;
		}
		EMasterySkillBubbleVisualState State = EMasterySkillBubbleVisualState::Locked;
		if (Subsystem)
		{
			if (Unlocked.Contains(Pair.Key))
			{
				State = EMasterySkillBubbleVisualState::Unlocked;
			}
			else if (Subsystem->IsPrerequisiteMet(Pair.Key))
			{
				State = (AvailablePoints >= BubbleWidget->GetPointCost())
					? EMasterySkillBubbleVisualState::Affordable
					: EMasterySkillBubbleVisualState::Unaffordable;
			}
		}
		BubbleWidget->SetVisualState(State);
	}
}

void UMasteryScreenWidget::RefreshAfterRespec()
{
	RefreshBubbleStates();
	RefreshPointsDisplayText();
}

void UMasteryScreenWidget::HandleBubbleClicked(FName BubbleId)
{
	UCrowdMasteryTotalSubsystem* Subsystem = ResolveMasteryTotalSubsystem();
	if (!Subsystem)
	{
		return;
	}
	if (Subsystem->TrySpendOnBubble(BubbleId))
	{
		RefreshBubbleStates();
		RefreshPointsDisplayText();
	}
}
