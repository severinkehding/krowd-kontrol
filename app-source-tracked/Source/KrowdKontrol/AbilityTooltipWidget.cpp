#include "AbilityTooltipWidget.h"
#include "AbilityData.h"
#include "HUDChromeColours.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"

namespace
{
	// Per-enum (not per-ability) display-text helpers - generic mappings, not a
	// second hardcoded per-tile string table, so they don't violate this widget's
	// "all tooltip content sourced from AbilityData" acceptance criterion.
	FText GetRangeText(EAbilityRange Range)
	{
		switch (Range)
		{
		case EAbilityRange::Short:
			return NSLOCTEXT("AbilityTooltipWidget", "RangeShort", "Short");
		case EAbilityRange::Medium:
			return NSLOCTEXT("AbilityTooltipWidget", "RangeMedium", "Medium");
		case EAbilityRange::Long:
			return NSLOCTEXT("AbilityTooltipWidget", "RangeLong", "Long");
		default:
			checkNoEntry();
			return FText::GetEmpty();
		}
	}

	FText GetShapeText(EAbilityTargetType TargetType)
	{
		switch (TargetType)
		{
		case EAbilityTargetType::SelfCircle:
			return NSLOCTEXT("AbilityTooltipWidget", "ShapeSelfCircle", "self-centered circle");
		case EAbilityTargetType::Cone:
			return NSLOCTEXT("AbilityTooltipWidget", "ShapeCone", "cone");
		case EAbilityTargetType::Line:
			return NSLOCTEXT("AbilityTooltipWidget", "ShapeLine", "line");
		case EAbilityTargetType::ThrownCircle:
			return NSLOCTEXT("AbilityTooltipWidget", "ShapeThrownCircle", "thrown circle");
		default:
			checkNoEntry();
			return FText::GetEmpty();
		}
	}
}

void UAbilityTooltipWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	EnsureWidgetTreeBuilt();
}

bool UAbilityTooltipWidget::Initialize()
{
	const bool bNewlyInitialized = Super::Initialize();
	if (bNewlyInitialized)
	{
		EnsureWidgetTreeBuilt();
	}
	return bNewlyInitialized;
}

void UAbilityTooltipWidget::EnsureWidgetTreeBuilt()
{
	// Whichever of NativeOnInitialized()/Initialize() fires first builds the tree; the
	// other is then a no-op, regardless of engine call order between the two.
	if (!RootBorder)
	{
		if (!WidgetTree)
		{
			WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"), RF_Transient);
		}
		BuildWidgetTree();
	}
}

void UAbilityTooltipWidget::BuildWidgetTree()
{
	const FSlateColor TextColor(HUDChromeColours::GetText());

	UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("TooltipRootCanvas"));
	WidgetTree->RootWidget = RootCanvas;

	RootBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("TooltipRootBorder"));
	RootBorder->SetBrushColor(HUDChromeColours::GetBackground());

	UCanvasPanelSlot* RootSlot = RootCanvas->AddChildToCanvas(RootBorder);
	checkf(RootSlot, TEXT("AbilityTooltipWidget: AddChildToCanvas(RootBorder) returned null"));
	RootSlot->SetAutoSize(true);

	UVerticalBox* RowsBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("TooltipRowsBox"));
	RootBorder->SetContent(RowsBox);

	NameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TooltipNameText"));
	NameText->SetColorAndOpacity(TextColor);
	RowsBox->AddChildToVerticalBox(NameText);

	KeyBindText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TooltipKeyBindText"));
	KeyBindText->SetColorAndOpacity(TextColor);
	RowsBox->AddChildToVerticalBox(KeyBindText);

	DescriptionText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TooltipDescriptionText"));
	DescriptionText->SetColorAndOpacity(TextColor);
	RowsBox->AddChildToVerticalBox(DescriptionText);

	DurationText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TooltipDurationText"));
	DurationText->SetColorAndOpacity(TextColor);
	RowsBox->AddChildToVerticalBox(DurationText);

	RangeShapeText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TooltipRangeShapeText"));
	RangeShapeText->SetColorAndOpacity(TextColor);
	RowsBox->AddChildToVerticalBox(RangeShapeText);

	UHorizontalBox* EnemyRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("TooltipEnemyRow"));
	RowsBox->AddChildToVerticalBox(EnemyRow);

	USizeBox* SwatchSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("TooltipSwatchSizeBox"));
	SwatchSizeBox->SetWidthOverride(16.0f);
	SwatchSizeBox->SetHeightOverride(16.0f);

	SwatchBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("TooltipSwatchBorder"));
	SwatchSizeBox->AddChild(SwatchBorder);
	UHorizontalBoxSlot* SwatchRowSlot = EnemyRow->AddChildToHorizontalBox(SwatchSizeBox);
	checkf(SwatchRowSlot, TEXT("AbilityTooltipWidget: AddChildToHorizontalBox(SwatchSizeBox) returned null"));
	SwatchRowSlot->SetPadding(FMargin(0.0f, 0.0f, 4.0f, 0.0f));

	EnemyTypeText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TooltipEnemyTypeText"));
	EnemyTypeText->SetColorAndOpacity(TextColor);
	EnemyRow->AddChildToHorizontalBox(EnemyTypeText);
}

void UAbilityTooltipWidget::SetAbility(EAbilitySlot AbilitySlot)
{
	EnsureWidgetTreeBuilt();

	const FAbilityData& Data = AbilityData::Get(AbilitySlot);

	NameText->SetText(StaticEnum<EAbilitySlot>()->GetDisplayNameTextByValue(static_cast<int64>(AbilitySlot)));
	KeyBindText->SetText(FText::Format(
		NSLOCTEXT("AbilityTooltipWidget", "KeyBindFormat", "Key: {0}"), Data.KeyBindingLabel));
	DescriptionText->SetText(Data.EffectDescription);
	DurationText->SetText(FText::Format(
		NSLOCTEXT("AbilityTooltipWidget", "DurationFormat", "Duration: {0}s"),
		FText::AsNumber(FMath::RoundToInt(Data.BaseDurationSeconds))));
	RangeShapeText->SetText(FText::Format(
		NSLOCTEXT("AbilityTooltipWidget", "RangeShapeFormat", "Range: {0}-range {1}"),
		GetRangeText(Data.Range), GetShapeText(Data.TargetType)));
	SwatchBorder->SetBrushColor(Data.Colour);
	EnemyTypeText->SetText(Data.bIsColourNeutral
		? NSLOCTEXT("AbilityTooltipWidget", "NoEnemyMatch", "No enemy colour match")
		: FText::Format(
			  NSLOCTEXT("AbilityTooltipWidget", "CountersFormat", "Counters {0}"),
			  StaticEnum<EEnemyType>()->GetDisplayNameTextByValue(static_cast<int64>(Data.CounteredEnemyType))));
}

FText UAbilityTooltipWidget::GetAbilityNameText() const
{
	return NameText ? NameText->GetText() : FText::GetEmpty();
}

FText UAbilityTooltipWidget::GetKeyBindingText() const
{
	return KeyBindText ? KeyBindText->GetText() : FText::GetEmpty();
}

FText UAbilityTooltipWidget::GetDescriptionText() const
{
	return DescriptionText ? DescriptionText->GetText() : FText::GetEmpty();
}

FText UAbilityTooltipWidget::GetDurationText() const
{
	return DurationText ? DurationText->GetText() : FText::GetEmpty();
}

FText UAbilityTooltipWidget::GetRangeShapeText() const
{
	return RangeShapeText ? RangeShapeText->GetText() : FText::GetEmpty();
}

FText UAbilityTooltipWidget::GetEnemyTypeText() const
{
	return EnemyTypeText ? EnemyTypeText->GetText() : FText::GetEmpty();
}

FLinearColor UAbilityTooltipWidget::GetSwatchColour() const
{
	return SwatchBorder ? SwatchBorder->GetBrushColor() : FLinearColor::Black;
}
