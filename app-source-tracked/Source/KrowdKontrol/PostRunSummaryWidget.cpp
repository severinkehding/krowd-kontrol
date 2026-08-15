#include "PostRunSummaryWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/TextBlock.h"

void UPostRunSummaryWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildWidgetTree();
	SetSummaryValues(PlaceholderClearTimeSeconds, PlaceholderCrowdMasteryCount);
}

bool UPostRunSummaryWidget::Initialize()
{
	const bool bNewlyInitialized = Super::Initialize();
	// If NativeOnInitialized() already ran (real gameplay usage with a valid player),
	// ClearTimeText is already set - skip rebuilding the tree a second time.
	if (bNewlyInitialized && !ClearTimeText)
	{
		BuildWidgetTree();
		SetSummaryValues(PlaceholderClearTimeSeconds, PlaceholderCrowdMasteryCount);
	}
	return bNewlyInitialized;
}

void UPostRunSummaryWidget::BuildWidgetTree()
{
	// Desaturated near-black background + light-gray (not pure white) text -
	// MISSION.md Hard Invariant 3 / PRD 13 REQ-4 / PRD 11 REQ-1 reserve
	// Purple/Teal/Orange/Blue/White for gameplay information; this screen's chrome
	// must not use any of them. Both colours stay inside PRD 11 REQ-2's desaturated
	// white/gray/black base palette.
	UBorder* RootBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("SummaryRootBorder"));
	RootBorder->SetBrushColor(FLinearColor(0.05f, 0.05f, 0.05f, 0.92f));
	WidgetTree->RootWidget = RootBorder;

	UVerticalBox* Layout = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("SummaryLayout"));
	RootBorder->SetContent(Layout);

	const FSlateColor TextColor(FLinearColor(0.85f, 0.85f, 0.85f, 1.0f));

	ClearTimeText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ClearTimeText"));
	ClearTimeText->SetColorAndOpacity(TextColor);
	Layout->AddChildToVerticalBox(ClearTimeText);

	CrowdMasteryText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CrowdMasteryText"));
	CrowdMasteryText->SetColorAndOpacity(TextColor);
	Layout->AddChildToVerticalBox(CrowdMasteryText);
}

void UPostRunSummaryWidget::SetSummaryValues(float ClearTimeSeconds, int32 CrowdMasteryCount)
{
	const int32 ClampedSeconds = FMath::Max(0, FMath::RoundToInt(ClearTimeSeconds));
	const int32 Minutes = ClampedSeconds / 60;
	const int32 Seconds = ClampedSeconds % 60;
	const FText ClearTimeDisplay = FText::Format(
		NSLOCTEXT("PostRunSummaryWidget", "ClearTimeFormat", "Clear Time: {0}:{1}"),
		FText::AsNumber(Minutes),
		FText::FromString(FString::Printf(TEXT("%02d"), Seconds)));
	if (ClearTimeText)
	{
		ClearTimeText->SetText(ClearTimeDisplay);
	}

	const FText CrowdMasteryDisplay = FText::Format(
		NSLOCTEXT("PostRunSummaryWidget", "CrowdMasteryFormat", "Crowd Mastery: {0}"),
		FText::AsNumber(FMath::Max(0, CrowdMasteryCount)));
	if (CrowdMasteryText)
	{
		CrowdMasteryText->SetText(CrowdMasteryDisplay);
	}
}

FText UPostRunSummaryWidget::GetClearTimeDisplayText() const
{
	return ClearTimeText ? ClearTimeText->GetText() : FText::GetEmpty();
}

FText UPostRunSummaryWidget::GetCrowdMasteryDisplayText() const
{
	return CrowdMasteryText ? CrowdMasteryText->GetText() : FText::GetEmpty();
}
