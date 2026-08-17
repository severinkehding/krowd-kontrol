#include "AbilityCooldownTrayWidget.h"
#include "AbilityUnlockComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "AbilityData.h"
#include "HUDChromeColours.h"

namespace
{
	// Short text labels, same order as EAbilitySlot - the shape/text differentiator
	// (issue #66), now paired with a per-slot colour tint sourced from
	// AbilityData::Get() (issue #76 / PRD 13 REQ-7). No real ability art pipeline
	// exists yet for this widget's scope, so the label itself stays a text
	// placeholder - only its colour changed.
	const TCHAR* SlotLabels[UAbilityCooldownTrayWidget::NumAbilitySlots] = { TEXT("STN"), TEXT("SLP"), TEXT("ROT"), TEXT("FER"), TEXT("SNR") };

	// Locked-state label (PRD 13 REQ-3) - replaces the slot's ability abbreviation
	// entirely rather than being appended/overlaid, so a locked slot reads as a
	// distinct state at a glance, not a modified cooldown.
	const TCHAR* LockedSlotLabel = TEXT("LCK");

	// Desaturated dark red - reads as a "warning/blocked" treatment without becoming
	// a 6th saturated gameplay-information colour (MISSION.md Hard Invariant 3 /
	// PRD 13 REQ-4). Combined with the LCK label swap below, this makes the locked
	// state distinguishable via more than colour alone (PRD 13 REQ-3/REQ-7's spirit).
	FLinearColor GetLockedBorderColor()
	{
		return FLinearColor(0.35f, 0.05f, 0.05f, 0.92f);
	}
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
	const FSlateColor TextColor(HUDChromeColours::GetText());

	// First use of UCanvasPanel in this codebase - needed because corner anchoring
	// requires a UCanvasPanelSlot, which only a UCanvasPanel child gets.
	UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("TrayRootCanvas"));
	WidgetTree->RootWidget = RootCanvas;

	UHorizontalBox* SlotsBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("TraySlotsBox"));

	UCanvasPanelSlot* TraySlot = RootCanvas->AddChildToCanvas(SlotsBox);
	checkf(TraySlot, TEXT("AbilityCooldownTrayWidget: AddChildToCanvas(SlotsBox) returned null"));
	// Bottom-right corner anchoring - diagonally opposite UEnergyMeterWidget's
	// top-left anchor (issue #64, landed). See TrayMarginPx's doc-comment in the
	// header for why bottom-right (not top-right) was picked before the meter existed.
	TraySlot->SetAnchors(FAnchors(1.0f, 1.0f, 1.0f, 1.0f));
	TraySlot->SetAlignment(FVector2D(1.0f, 1.0f));
	TraySlot->SetAutoSize(true);
	TraySlot->SetPosition(FVector2D(-TrayMarginPx, -TrayMarginPx));

	SlotIconBorders.SetNum(NumAbilitySlots);
	SlotCooldownTexts.SetNum(NumAbilitySlots);
	SlotIconLabels.SetNum(NumAbilitySlots);
	SlotCooldownRemaining.SetNum(NumAbilitySlots);
	SlotCooldownDuration.SetNum(NumAbilitySlots);
	SlotLocked.SetNum(NumAbilitySlots);

	for (int32 Index = 0; Index < NumAbilitySlots; ++Index)
	{
		UOverlay* SlotOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), *FString::Printf(TEXT("SlotOverlay_%d"), Index));
		UHorizontalBoxSlot* SlotsBoxSlot = SlotsBox->AddChildToHorizontalBox(SlotOverlay);
		checkf(SlotsBoxSlot, TEXT("AbilityCooldownTrayWidget: AddChildToHorizontalBox(SlotOverlay) returned null"));
		SlotsBoxSlot->SetPadding(FMargin(4.0f));

		UBorder* IconBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), *FString::Printf(TEXT("SlotIconBorder_%d"), Index));
		IconBorder->SetBrushColor(HUDChromeColours::GetBackground());
		SlotOverlay->AddChildToOverlay(IconBorder);

		const EAbilitySlot CurrentSlot = static_cast<EAbilitySlot>(Index);
		UTextBlock* IconLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *FString::Printf(TEXT("SlotIconLabel_%d"), Index));
		IconLabel->SetColorAndOpacity(FSlateColor(AbilityData::Get(CurrentSlot).Colour));
		IconLabel->SetText(FText::FromString(SlotLabels[Index]));
		IconBorder->SetContent(IconLabel);

		// Added after IconBorder in the same overlay, so it renders on top.
		UTextBlock* CooldownText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *FString::Printf(TEXT("SlotCooldownText_%d"), Index));
		CooldownText->SetColorAndOpacity(TextColor);
		SlotOverlay->AddChildToOverlay(CooldownText);

		SlotIconBorders[Index] = IconBorder;
		SlotCooldownTexts[Index] = CooldownText;
		SlotIconLabels[Index] = IconLabel;
	}
}

void UAbilityCooldownTrayWidget::SeedPlaceholderCooldowns()
{
	for (int32 Index = 0; Index < NumAbilitySlots; ++Index)
	{
		SlotCooldownDuration[Index] = PlaceholderCooldownDurations[Index];
		SlotCooldownRemaining[Index] = SlotCooldownDuration[Index];
		UpdateSlotVisual(static_cast<EAbilitySlot>(Index));
	}
}

void UAbilityCooldownTrayWidget::StartCooldown(EAbilitySlot AbilitySlot, float DurationSeconds)
{
	const int32 Index = static_cast<int32>(AbilitySlot);
	if (!SlotCooldownDuration.IsValidIndex(Index))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UAbilityCooldownTrayWidget::StartCooldown: index %d invalid on '%s' (tray not yet built?) - cooldown dropped."),
			Index, *GetNameSafe(this));
		return;
	}
	SlotCooldownDuration[Index] = FMath::Max(0.0f, DurationSeconds);
	SlotCooldownRemaining[Index] = SlotCooldownDuration[Index];
	UpdateSlotVisual(AbilitySlot);
}

void UAbilityCooldownTrayWidget::SetSlotLocked(EAbilitySlot AbilitySlot, bool bLocked)
{
	const int32 Index = static_cast<int32>(AbilitySlot);
	if (!SlotLocked.IsValidIndex(Index))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UAbilityCooldownTrayWidget::SetSlotLocked: index %d invalid on '%s' (tray not yet built?) - lock state dropped."),
			Index, *GetNameSafe(this));
		return;
	}
	SlotLocked[Index] = bLocked;
	UpdateSlotVisual(AbilitySlot);
}

void UAbilityCooldownTrayWidget::BindAbilityUnlockComponent(UAbilityUnlockComponent* UnlockComponent)
{
	if (!UnlockComponent)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("AbilityCooldownTrayWidget::BindAbilityUnlockComponent called with null component - tray keeps its current locked states"));
		return;
	}
	for (int32 Index = 0; Index < NumAbilitySlots; ++Index)
	{
		const EAbilitySlot CurrentSlot = static_cast<EAbilitySlot>(Index);
		SetSlotLocked(CurrentSlot, !UnlockComponent->IsAbilityUnlocked(CurrentSlot));
	}
	// AddUniqueDynamic so a repeated bind (e.g. HUD rebuild on level transition,
	// issue #132) can't stack duplicate subscriptions.
	UnlockComponent->OnAbilityUnlocked.AddUniqueDynamic(this, &UAbilityCooldownTrayWidget::HandleAbilityUnlocked);
}

void UAbilityCooldownTrayWidget::HandleAbilityUnlocked(EAbilitySlot Ability)
{
	SetSlotLocked(Ability, false);
}

void UAbilityCooldownTrayWidget::AdvanceCooldowns(float DeltaSeconds)
{
	for (int32 Index = 0; Index < SlotCooldownRemaining.Num(); ++Index)
	{
		if (SlotCooldownRemaining[Index] > 0.0f)
		{
			SlotCooldownRemaining[Index] = FMath::Max(0.0f, SlotCooldownRemaining[Index] - DeltaSeconds);
			UpdateSlotVisual(static_cast<EAbilitySlot>(Index));
		}
	}
}

void UAbilityCooldownTrayWidget::UpdateSlotVisual(EAbilitySlot AbilitySlot)
{
	const int32 Index = static_cast<int32>(AbilitySlot);
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

	const bool bLocked = SlotLocked.IsValidIndex(Index) && SlotLocked[Index];

	if (SlotIconBorders.IsValidIndex(Index) && SlotIconBorders[Index])
	{
		SlotIconBorders[Index]->SetBrushColor(bLocked ? GetLockedBorderColor() : HUDChromeColours::GetBackground());
	}
	else if (SlotIconBorders.IsValidIndex(Index))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UAbilityCooldownTrayWidget: SlotIconBorders[%d] is null on '%s' - locked/chrome border colour will not update."),
			Index, *GetNameSafe(this));
	}
	if (SlotIconLabels.IsValidIndex(Index) && SlotIconLabels[Index])
	{
		SlotIconLabels[Index]->SetText(FText::FromString(bLocked ? LockedSlotLabel : SlotLabels[Index]));
	}
	else if (SlotIconLabels.IsValidIndex(Index))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UAbilityCooldownTrayWidget: SlotIconLabels[%d] is null on '%s' - label will not update."),
			Index, *GetNameSafe(this));
	}

	if (bLocked)
	{
		// Locked overrides the cooldown countdown entirely (PRD 13 REQ-3) - a locked
		// slot isn't "ready soon", it's inaccessible, so a numeric ETA would be
		// actively misleading regardless of any cooldown time still counting down
		// underneath.
		CooldownText->SetText(FText::GetEmpty());
		CooldownText->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	if (SlotCooldownRemaining[Index] > 0.0f)
	{
		CooldownText->SetText(FText::AsNumber(FMath::CeilToInt(SlotCooldownRemaining[Index])));
		CooldownText->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	else
	{
		CooldownText->SetText(FText::GetEmpty());
		CooldownText->SetVisibility(ESlateVisibility::Collapsed);
	}
}

float UAbilityCooldownTrayWidget::GetSlotRemainingSeconds(EAbilitySlot AbilitySlot) const
{
	const int32 Index = static_cast<int32>(AbilitySlot);
	return SlotCooldownRemaining.IsValidIndex(Index) ? SlotCooldownRemaining[Index] : 0.0f;
}

bool UAbilityCooldownTrayWidget::IsSlotOnCooldown(EAbilitySlot AbilitySlot) const
{
	const int32 Index = static_cast<int32>(AbilitySlot);
	return SlotCooldownRemaining.IsValidIndex(Index) && SlotCooldownRemaining[Index] > 0.0f;
}

bool UAbilityCooldownTrayWidget::IsSlotLocked(EAbilitySlot AbilitySlot) const
{
	const int32 Index = static_cast<int32>(AbilitySlot);
	return SlotLocked.IsValidIndex(Index) && SlotLocked[Index];
}

FText UAbilityCooldownTrayWidget::GetSlotCooldownDisplayText(EAbilitySlot AbilitySlot) const
{
	const int32 Index = static_cast<int32>(AbilitySlot);
	if (!SlotCooldownTexts.IsValidIndex(Index) || !SlotCooldownTexts[Index])
	{
		return FText::GetEmpty();
	}
	return SlotCooldownTexts[Index]->GetText();
}

FText UAbilityCooldownTrayWidget::GetSlotIconLabelText(EAbilitySlot AbilitySlot) const
{
	const int32 Index = static_cast<int32>(AbilitySlot);
	if (!SlotIconLabels.IsValidIndex(Index) || !SlotIconLabels[Index])
	{
		return FText::GetEmpty();
	}
	return SlotIconLabels[Index]->GetText();
}

FLinearColor UAbilityCooldownTrayWidget::GetSlotIconTintColour(EAbilitySlot AbilitySlot) const
{
	const int32 Index = static_cast<int32>(AbilitySlot);
	if (!SlotIconLabels.IsValidIndex(Index) || !SlotIconLabels[Index])
	{
		return FLinearColor::Black;
	}
	return SlotIconLabels[Index]->GetColorAndOpacity().GetSpecifiedColor();
}
