#include "AbilityCooldownTrayWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"

namespace
{
	// Short text labels, same order as EAbilitySlot - shape/text redundancy rather
	// than colour-coded icons (no real ability art pipeline exists yet for this
	// widget's scope).
	const TCHAR* SlotLabels[UAbilityCooldownTrayWidget::NumAbilitySlots] = { TEXT("STN"), TEXT("SLP"), TEXT("ROT"), TEXT("FER"), TEXT("SNR") };
}

void UAbilityCooldownTrayWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	EnsureWidgetTreeBuilt();
}

bool UAbilityCooldownTrayWidget::Initialize()
{
	const bool bNewlyInitialized = Super::Initialize();
	if (bNewlyInitialized)
	{
		EnsureWidgetTreeBuilt();
	}
	return bNewlyInitialized;
}

void UAbilityCooldownTrayWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	AdvanceCooldowns(InDeltaTime);
}

void UAbilityCooldownTrayWidget::EnsureWidgetTreeBuilt()
{
	// Whichever of NativeOnInitialized()/Initialize() fires first builds the tree; the
	// other is then a no-op, regardless of engine call order between the two.
	if (SlotIconBorders.Num() == 0)
	{
		// UUserWidget::WidgetTree is normally lazily created inside Initialize()
		// (before it conditionally calls NativeOnInitialized()) - but NativeOnInitialized()
		// can also be invoked directly, bypassing Initialize() entirely (e.g. this class's
		// own Automation test exercises that call order to prove idempotency). WidgetTree
		// would still be null in that case, and WidgetTree->ConstructWidget<T>() on a null
		// WidgetTree doesn't crash on the call itself - it silently passes a null Outer
		// into NewObject<T>(), which the engine then treats as fatal. Mirror
		// UUserWidget::Initialize()'s own lazy-creation exactly so this is safe regardless
		// of call order.
		if (!WidgetTree)
		{
			WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"), RF_Transient);
		}
		BuildWidgetTree();
		SeedPlaceholderCooldowns();
	}
}

void UAbilityCooldownTrayWidget::BuildWidgetTree()
{
	// Desaturated near-black background + light-gray (not pure white) text -
	// MISSION.md Hard Invariant 3 / PRD 13 REQ-4 / PRD 11 REQ-1 reserve
	// Purple/Teal/Orange/Blue/White for gameplay information; this tray's chrome must
	// not use any of them. Same already-reviewed palette as UPostRunSummaryWidget
	// (issue #74).
	const FLinearColor ChromeBackgroundColor(0.05f, 0.05f, 0.05f, 0.92f);
	const FSlateColor TextColor(FLinearColor(0.85f, 0.85f, 0.85f, 1.0f));

	// First use of UCanvasPanel in this codebase - needed because corner anchoring
	// requires a UCanvasPanelSlot, which only a UCanvasPanel child gets.
	UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("TrayRootCanvas"));
	WidgetTree->RootWidget = RootCanvas;

	UHorizontalBox* SlotsBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("TraySlotsBox"));

	UCanvasPanelSlot* TraySlot = RootCanvas->AddChildToCanvas(SlotsBox);
	// Bottom-right corner anchoring - see TrayMarginPx's doc-comment in the header for
	// why bottom-right is this plan's explicit choice (the energy meter, issue #64,
	// hasn't landed yet, so there's no corner to be literally "opposite" of).
	TraySlot->SetAnchors(FAnchors(1.0f, 1.0f, 1.0f, 1.0f));
	TraySlot->SetAlignment(FVector2D(1.0f, 1.0f));
	TraySlot->SetAutoSize(true);
	TraySlot->SetPosition(FVector2D(-TrayMarginPx, -TrayMarginPx));

	SlotIconBorders.SetNum(NumAbilitySlots);
	SlotCooldownTexts.SetNum(NumAbilitySlots);
	SlotCooldownRemaining.SetNum(NumAbilitySlots);
	SlotCooldownDuration.SetNum(NumAbilitySlots);

	for (int32 Index = 0; Index < NumAbilitySlots; ++Index)
	{
		UOverlay* SlotOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), *FString::Printf(TEXT("SlotOverlay_%d"), Index));
		UHorizontalBoxSlot* SlotsBoxSlot = SlotsBox->AddChildToHorizontalBox(SlotOverlay);
		SlotsBoxSlot->SetPadding(FMargin(4.0f));

		UBorder* IconBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), *FString::Printf(TEXT("SlotIconBorder_%d"), Index));
		IconBorder->SetBrushColor(ChromeBackgroundColor);
		SlotOverlay->AddChildToOverlay(IconBorder);

		UTextBlock* IconLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *FString::Printf(TEXT("SlotIconLabel_%d"), Index));
		IconLabel->SetColorAndOpacity(TextColor);
		IconLabel->SetText(FText::FromString(SlotLabels[Index]));
		IconBorder->SetContent(IconLabel);

		// Added after IconBorder in the same overlay, so it renders on top.
		UTextBlock* CooldownText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *FString::Printf(TEXT("SlotCooldownText_%d"), Index));
		CooldownText->SetColorAndOpacity(TextColor);
		SlotOverlay->AddChildToOverlay(CooldownText);

		SlotIconBorders[Index] = IconBorder;
		SlotCooldownTexts[Index] = CooldownText;
	}
}

void UAbilityCooldownTrayWidget::SeedPlaceholderCooldowns()
{
	for (int32 Index = 0; Index < NumAbilitySlots; ++Index)
	{
		SlotCooldownDuration[Index] = PlaceholderCooldownDurations[Index];
		SlotCooldownRemaining[Index] = SlotCooldownDuration[Index];
		UpdateSlotVisual((EAbilitySlot)Index);
	}
}

void UAbilityCooldownTrayWidget::StartCooldown(EAbilitySlot AbilitySlot, float DurationSeconds)
{
	const int32 Index = (int32)AbilitySlot;
	if (!SlotCooldownDuration.IsValidIndex(Index))
	{
		return;
	}
	SlotCooldownDuration[Index] = FMath::Max(0.0f, DurationSeconds);
	SlotCooldownRemaining[Index] = SlotCooldownDuration[Index];
	UpdateSlotVisual(AbilitySlot);
}

void UAbilityCooldownTrayWidget::AdvanceCooldowns(float DeltaSeconds)
{
	for (int32 Index = 0; Index < SlotCooldownRemaining.Num(); ++Index)
	{
		if (SlotCooldownRemaining[Index] > 0.0f)
		{
			SlotCooldownRemaining[Index] = FMath::Max(0.0f, SlotCooldownRemaining[Index] - DeltaSeconds);
			UpdateSlotVisual((EAbilitySlot)Index);
		}
	}
}

void UAbilityCooldownTrayWidget::UpdateSlotVisual(EAbilitySlot AbilitySlot)
{
	const int32 Index = (int32)AbilitySlot;
	if (!SlotCooldownTexts.IsValidIndex(Index) || !SlotCooldownRemaining.IsValidIndex(Index))
	{
		return;
	}

	UTextBlock* CooldownText = SlotCooldownTexts[Index];
	if (!CooldownText)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UAbilityCooldownTrayWidget: SlotCooldownTexts[%d] is null on '%s' - cooldown overlay will render blank."),
			Index, *GetNameSafe(this));
		return;
	}

	if (SlotCooldownRemaining[Index] > 0.0f)
	{
		CooldownText->SetText(FText::AsNumber(FMath::CeilToInt(SlotCooldownRemaining[Index])));
		CooldownText->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	else
	{
		CooldownText->SetVisibility(ESlateVisibility::Collapsed);
	}
}

float UAbilityCooldownTrayWidget::GetSlotRemainingSeconds(EAbilitySlot AbilitySlot) const
{
	const int32 Index = (int32)AbilitySlot;
	return SlotCooldownRemaining.IsValidIndex(Index) ? SlotCooldownRemaining[Index] : 0.0f;
}

bool UAbilityCooldownTrayWidget::IsSlotOnCooldown(EAbilitySlot AbilitySlot) const
{
	const int32 Index = (int32)AbilitySlot;
	return SlotCooldownRemaining.IsValidIndex(Index) && SlotCooldownRemaining[Index] > 0.0f;
}

FText UAbilityCooldownTrayWidget::GetSlotCooldownDisplayText(EAbilitySlot AbilitySlot) const
{
	const int32 Index = (int32)AbilitySlot;
	if (!SlotCooldownTexts.IsValidIndex(Index) || !SlotCooldownTexts[Index])
	{
		return FText::GetEmpty();
	}
	return SlotCooldownTexts[Index]->GetText();
}
