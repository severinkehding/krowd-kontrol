#include "PostRunSummaryWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/TextBlock.h"

void UPostRunSummaryWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	EnsureWidgetTreeBuilt();
}

bool UPostRunSummaryWidget::Initialize()
{
	const bool bNewlyInitialized = Super::Initialize();
	if (bNewlyInitialized)
	{
		EnsureWidgetTreeBuilt();
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
		SetSummaryValues(PlaceholderClearTimeSeconds, PlaceholderCrowdMasteryCount);
	}
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
	SetTextBlockSafe(ClearTimeText, ClearTimeDisplay, TEXT("ClearTimeText"));

	const FText CrowdMasteryDisplay = FText::Format(
		NSLOCTEXT("PostRunSummaryWidget", "CrowdMasteryFormat", "Crowd Mastery: {0}"),
		FText::AsNumber(FMath::Max(0, CrowdMasteryCount)));
	SetTextBlockSafe(CrowdMasteryText, CrowdMasteryDisplay, TEXT("CrowdMasteryText"));
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

FText UPostRunSummaryWidget::GetCrowdMasteryDisplayText() const
{
	return CrowdMasteryText ? CrowdMasteryText->GetText() : FText::GetEmpty();
}
