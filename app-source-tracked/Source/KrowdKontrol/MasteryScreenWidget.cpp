#include "MasteryScreenWidget.h"
#include "HUDChromeColours.h"
#include "CrowdMasteryTotalSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/PanelSlot.h"
#include "Components/VerticalBox.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Engine/GameInstance.h"

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

	// Unspent-points display (this issue) - no node/bubble tree content below it yet;
	// that is a separate follow-up issue, see MasteryScreenWidget.h's header comment.
	PointsText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("MasteryScreenPointsText"));
	PointsText->SetColorAndOpacity(TextColor);
	RefreshPointsDisplayText();
	Layout->AddChildToVerticalBox(PointsText);

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

	// "Unspent" == GetAccumulatedTotal() verbatim for this issue only - there is no
	// spend/refund API on UCrowdMasteryTotalSubsystem yet (docs/prd-mastery-skill-tree.md
	// REQ-1, separate follow-up). A missing GameInstance (every KrowdKontrol.Unit.* test
	// that constructs this widget via CreateNewMap() hits this) is the unremarkable
	// default and stays unlogged; a present GameInstance with no resolvable subsystem is
	// a real failure and is warned once below - same shape as
	// UMainMenuWidget::RefreshMasteryDisplayText().
	int32 UnspentPoints = 0;
	if (CachedMasteryTotalSubsystem)
	{
		UnspentPoints = CachedMasteryTotalSubsystem->GetAccumulatedTotal();
	}
	else if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UCrowdMasteryTotalSubsystem* MasterySubsystem = GameInstance->GetSubsystem<UCrowdMasteryTotalSubsystem>())
		{
			CachedMasteryTotalSubsystem = MasterySubsystem;
			UnspentPoints = MasterySubsystem->GetAccumulatedTotal();
		}
		else if (!bHasWarnedMissingMasteryTotalSubsystem)
		{
			bHasWarnedMissingMasteryTotalSubsystem = true;
			UE_LOG(LogTemp, Warning,
				TEXT("UMasteryScreenWidget::RefreshPointsDisplayText: no UCrowdMasteryTotalSubsystem available on '%s' - points display will show 0 instead of the real unspent total."),
				*GetNameSafe(this));
		}
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
