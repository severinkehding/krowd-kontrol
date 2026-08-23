#include "AbilityCooldownTrayWidget.h"
#include "AbilityUnlockComponent.h"
#include "AbilityLockoutComponent.h"
#include "AbilityCooldownComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "AbilityData.h"
#include "HUDChromeColours.h"
#include "AbilityTooltipWidget.h"

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

	// Neutral desaturated silver, not one of the 5 reserved gameplay-information
	// colours (MISSION.md Hard Invariant 3) and distinct from UEnergyMeterWidget's
	// saturated green fill, since this fill is a generic "time remaining" mask, not
	// an ability/enemy-identifying signal.
	FLinearColor GetCooldownFillColor()
	{
		return FLinearColor(0.55f, 0.55f, 0.60f, 0.85f);
	}

	// Placeholder-quality brightness pulse (issue #259) - lerps the chrome
	// background toward (but not fully to) white, so this never exactly equals the
	// reserved white/Stun colour while still reading as an unmistakable flash.
	FLinearColor GetReadyFlashBorderColor()
	{
		return FLinearColor::LerpUsingHSV(HUDChromeColours::GetBackground(), FLinearColor::White, 0.65f);
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
	if (BoundCooldownComponent.IsValid())
	{
		RefreshCooldownReadouts();
	}
	else
	{
		AdvanceCooldowns(InDeltaTime);
	}
	RefreshPunishmentLockoutReadouts();
	AdvanceReadyFlashTimers(InDeltaTime);
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
	SlotPunishmentLockoutActive.SetNum(NumAbilitySlots);
	SlotPunishmentLockoutRemaining.SetNum(NumAbilitySlots);
	SlotCooldownFillBars.SetNum(NumAbilitySlots);
	SlotReadyFlashRemaining.SetNum(NumAbilitySlots);

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

		// Hover tooltip (issue #260, PRD 13 REQ-2) - standard UMG hover detection via
		// SetToolTip() does the showing/hiding; constructed via WidgetTree (not
		// NewObject/CreateWidget) so its Outer matches every other child in this tree
		// and it has no owning-player dependency, since this widget's own tests
		// construct via CreateWidget<T>(World, ...) with no PlayerController at all.
		UAbilityTooltipWidget* SlotTooltip = WidgetTree->ConstructWidget<UAbilityTooltipWidget>(
			UAbilityTooltipWidget::StaticClass(), *FString::Printf(TEXT("SlotTooltip_%d"), Index));
		SlotTooltip->SetAbility(CurrentSlot);
		IconBorder->SetToolTip(SlotTooltip);

		UTextBlock* IconLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *FString::Printf(TEXT("SlotIconLabel_%d"), Index));
		IconLabel->SetColorAndOpacity(FSlateColor(AbilityData::Get(CurrentSlot).Colour));
		IconLabel->SetText(FText::FromString(SlotLabels[Index]));
		IconBorder->SetContent(IconLabel);

		// Vertical-drain cooldown fill (issue #259) - added after the icon
		// border/label, before the cooldown text, so the overlay z-order is: icon
		// border (bottom) -> fill bar -> cooldown text (top, stays legible). Wrapped
		// in a USizeBox since a bare UProgressBar has no useful intrinsic size in this
		// widget's auto-sized UHorizontalBox layout.
		USizeBox* FillSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), *FString::Printf(TEXT("SlotFillSizeBox_%d"), Index));
		FillSizeBox->SetWidthOverride(IconTileSizePx);
		FillSizeBox->SetHeightOverride(IconTileSizePx);
		UProgressBar* FillBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), *FString::Printf(TEXT("SlotCooldownFillBar_%d"), Index));
		FillBar->SetBarFillType(EProgressBarFillType::TopToBottom);
		FillBar->SetFillColorAndOpacity(GetCooldownFillColor());
		FillBar->SetPercent(0.0f);
		FillBar->SetVisibility(ESlateVisibility::Collapsed);
		FillSizeBox->AddChild(FillBar);
		SlotOverlay->AddChildToOverlay(FillSizeBox);
		SlotCooldownFillBars[Index] = FillBar;

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
	if (SlotReadyFlashRemaining.IsValidIndex(Index))
	{
		SlotReadyFlashRemaining[Index] = 0.0f;
	}
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

void UAbilityCooldownTrayWidget::BindAbilityLockoutComponent(UAbilityLockoutComponent* LockoutComponent)
{
	if (!LockoutComponent)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("AbilityCooldownTrayWidget::BindAbilityLockoutComponent called with null component - tray keeps its current locked states"));
		return;
	}
	UAbilityLockoutComponent* PreviousComponent = BoundLockoutComponent.Get();
	if (PreviousComponent && PreviousComponent != LockoutComponent)
	{
		// Without this, a rebind to a still-live component would leave the old
		// component's delegate subscribed too - since FOnAbilityLockoutChanged has
		// no sender parameter, HandleAbilityLockoutChanged always reads from
		// whichever component is *currently* bound, so a stale broadcast could
		// show a wrong countdown or clear an in-progress lockout. Mirrors
		// UEnergyMeterWidget::BindToEnergyComponent's identical guard.
		PreviousComponent->OnAbilityLockoutChanged.RemoveDynamic(this, &UAbilityCooldownTrayWidget::HandleAbilityLockoutChanged);
	}
	BoundLockoutComponent = LockoutComponent;
	// AddUniqueDynamic so a repeated bind (e.g. HUD rebuild on level transition,
	// issue #132) can't stack duplicate subscriptions - same reasoning as
	// BindAbilityUnlockComponent's identical guard above.
	LockoutComponent->OnAbilityLockoutChanged.AddUniqueDynamic(this, &UAbilityCooldownTrayWidget::HandleAbilityLockoutChanged);
}

void UAbilityCooldownTrayWidget::HandleAbilityLockoutChanged(EAbilitySlot Ability, bool bLocked)
{
	const int32 Index = static_cast<int32>(Ability);
	if (!SlotPunishmentLockoutActive.IsValidIndex(Index))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UAbilityCooldownTrayWidget::HandleAbilityLockoutChanged: index %d invalid on '%s' (tray not yet built?) - lockout state dropped."),
			Index, *GetNameSafe(this));
		return;
	}
	SlotPunishmentLockoutActive[Index] = bLocked;
	SlotPunishmentLockoutRemaining[Index] = (bLocked && BoundLockoutComponent.IsValid())
		? BoundLockoutComponent->GetRemainingLockoutSeconds(Ability)
		: 0.0f;
	UpdateSlotVisual(Ability);
}

void UAbilityCooldownTrayWidget::RefreshPunishmentLockoutReadouts()
{
	if (!BoundLockoutComponent.IsValid())
	{
		return;
	}
	for (int32 Index = 0; Index < SlotPunishmentLockoutActive.Num(); ++Index)
	{
		if (SlotPunishmentLockoutActive[Index])
		{
			SlotPunishmentLockoutRemaining[Index] = BoundLockoutComponent->GetRemainingLockoutSeconds(static_cast<EAbilitySlot>(Index));
			UpdateSlotVisual(static_cast<EAbilitySlot>(Index));
		}
	}
}

void UAbilityCooldownTrayWidget::BindAbilityCooldownComponent(UAbilityCooldownComponent* CooldownComponent)
{
	if (!CooldownComponent)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("AbilityCooldownTrayWidget::BindAbilityCooldownComponent called with null component - tray keeps its current cooldown states"));
		return;
	}
	UAbilityCooldownComponent* PreviousComponent = BoundCooldownComponent.Get();
	if (PreviousComponent && PreviousComponent != CooldownComponent)
	{
		// Without this, a rebind to a still-live component would leave the old
		// component's delegate subscribed too - mirrors
		// BindAbilityLockoutComponent's identical guard above.
		PreviousComponent->OnAbilityCooldownChanged.RemoveDynamic(this, &UAbilityCooldownTrayWidget::HandleAbilityCooldownChanged);
	}
	BoundCooldownComponent = CooldownComponent;
	// AddUniqueDynamic so a repeated bind (e.g. HUD rebuild on level transition,
	// issue #132) can't stack duplicate subscriptions - same reasoning as
	// BindAbilityLockoutComponent's identical guard above.
	CooldownComponent->OnAbilityCooldownChanged.AddUniqueDynamic(this, &UAbilityCooldownTrayWidget::HandleAbilityCooldownChanged);
}

void UAbilityCooldownTrayWidget::HandleAbilityCooldownChanged(EAbilitySlot Ability, bool bOnCooldown)
{
	const int32 Index = static_cast<int32>(Ability);
	if (!SlotCooldownRemaining.IsValidIndex(Index))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UAbilityCooldownTrayWidget::HandleAbilityCooldownChanged: index %d invalid on '%s' (tray not yet built?) - cooldown state dropped."),
			Index, *GetNameSafe(this));
		return;
	}
	if (bOnCooldown)
	{
		const float Duration = BoundCooldownComponent.IsValid() ? BoundCooldownComponent->GetRemainingCooldownSeconds(Ability) : 0.0f;
		StartCooldown(Ability, Duration);
	}
	else
	{
		SlotCooldownRemaining[Index] = 0.0f;
		UpdateSlotVisual(Ability);
		PlayReadyFlash(Ability);
	}
}

void UAbilityCooldownTrayWidget::RefreshCooldownReadouts()
{
	if (!BoundCooldownComponent.IsValid())
	{
		return;
	}
	for (int32 Index = 0; Index < SlotCooldownRemaining.Num(); ++Index)
	{
		if (SlotCooldownRemaining[Index] > 0.0f)
		{
			SlotCooldownRemaining[Index] = BoundCooldownComponent->GetRemainingCooldownSeconds(static_cast<EAbilitySlot>(Index));
			UpdateSlotVisual(static_cast<EAbilitySlot>(Index));
		}
	}
}

void UAbilityCooldownTrayWidget::PlayReadyFlash(EAbilitySlot AbilitySlot)
{
	const int32 Index = static_cast<int32>(AbilitySlot);
	if (!SlotReadyFlashRemaining.IsValidIndex(Index))
	{
		return;
	}
	SlotReadyFlashRemaining[Index] = ReadyFlashDurationSeconds;
	UpdateSlotVisual(AbilitySlot);
}

void UAbilityCooldownTrayWidget::AdvanceReadyFlashTimers(float DeltaSeconds)
{
	for (int32 Index = 0; Index < SlotReadyFlashRemaining.Num(); ++Index)
	{
		if (SlotReadyFlashRemaining[Index] > 0.0f)
		{
			SlotReadyFlashRemaining[Index] = FMath::Max(0.0f, SlotReadyFlashRemaining[Index] - DeltaSeconds);
			if (SlotReadyFlashRemaining[Index] <= 0.0f)
			{
				UpdateSlotVisual(static_cast<EAbilitySlot>(Index));
			}
		}
	}
}

void UAbilityCooldownTrayWidget::AdvanceCooldowns(float DeltaSeconds)
{
	for (int32 Index = 0; Index < SlotCooldownRemaining.Num(); ++Index)
	{
		if (SlotCooldownRemaining[Index] > 0.0f)
		{
			SlotCooldownRemaining[Index] = FMath::Max(0.0f, SlotCooldownRemaining[Index] - DeltaSeconds);
			UpdateSlotVisual(static_cast<EAbilitySlot>(Index));
			if (SlotCooldownRemaining[Index] <= 0.0f)
			{
				PlayReadyFlash(static_cast<EAbilitySlot>(Index));
			}
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

	const bool bNotYetUnlocked = SlotLocked.IsValidIndex(Index) && SlotLocked[Index];
	const bool bPunishmentLockout = SlotPunishmentLockoutActive.IsValidIndex(Index) && SlotPunishmentLockoutActive[Index];
	const bool bLockedStyle = bNotYetUnlocked || bPunishmentLockout;

	const bool bReadyFlash = SlotReadyFlashRemaining.IsValidIndex(Index) && SlotReadyFlashRemaining[Index] > 0.0f;

	if (SlotIconBorders.IsValidIndex(Index) && SlotIconBorders[Index])
	{
		FLinearColor BorderColor;
		if (bReadyFlash)
		{
			BorderColor = GetReadyFlashBorderColor();
		}
		else if (bLockedStyle)
		{
			BorderColor = GetLockedBorderColor();
		}
		else
		{
			BorderColor = HUDChromeColours::GetBackground();
		}
		SlotIconBorders[Index]->SetBrushColor(BorderColor);
	}
	else if (SlotIconBorders.IsValidIndex(Index))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UAbilityCooldownTrayWidget: SlotIconBorders[%d] is null on '%s' - locked/chrome border colour will not update."),
			Index, *GetNameSafe(this));
	}
	if (SlotIconLabels.IsValidIndex(Index) && SlotIconLabels[Index])
	{
		SlotIconLabels[Index]->SetText(FText::FromString(bLockedStyle ? LockedSlotLabel : SlotLabels[Index]));
	}
	else if (SlotIconLabels.IsValidIndex(Index))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UAbilityCooldownTrayWidget: SlotIconLabels[%d] is null on '%s' - label will not update."),
			Index, *GetNameSafe(this));
	}

	if (SlotCooldownFillBars.IsValidIndex(Index) && SlotCooldownFillBars[Index])
	{
		if (bLockedStyle || SlotCooldownRemaining[Index] <= 0.0f)
		{
			SlotCooldownFillBars[Index]->SetPercent(0.0f);
			SlotCooldownFillBars[Index]->SetVisibility(ESlateVisibility::Collapsed);
		}
		else
		{
			const float Duration = SlotCooldownDuration.IsValidIndex(Index) && SlotCooldownDuration[Index] > 0.0f ? SlotCooldownDuration[Index] : 1.0f;
			SlotCooldownFillBars[Index]->SetPercent(FMath::Clamp(SlotCooldownRemaining[Index] / Duration, 0.0f, 1.0f));
			SlotCooldownFillBars[Index]->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
	}

	if (bPunishmentLockout)
	{
		// Punishment lockout gets its own live numeric readout (issue #261),
		// layered on top of the same locked-style border/label as not-yet-unlocked -
		// this is the one piece of this function that makes the two locked-style
		// states visually distinguishable from each other.
		const float Remaining = SlotPunishmentLockoutRemaining.IsValidIndex(Index) ? SlotPunishmentLockoutRemaining[Index] : 0.0f;
		CooldownText->SetText(FText::AsNumber(FMath::CeilToInt(Remaining)));
		CooldownText->SetVisibility(ESlateVisibility::HitTestInvisible);
		return;
	}

	if (bNotYetUnlocked)
	{
		// Not-yet-unlocked overrides the cooldown countdown entirely (PRD 13 REQ-3) -
		// a locked slot isn't "ready soon", it's inaccessible, so a numeric ETA would
		// be actively misleading regardless of any cooldown time still counting down
		// underneath. Unlike punishment lockout above, this state never shows a
		// timer - that's the explicit, unchanged behavior this issue must preserve.
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

EAbilityTileState UAbilityCooldownTrayWidget::GetSlotState(EAbilitySlot AbilitySlot) const
{
	const int32 Index = static_cast<int32>(AbilitySlot);
	if (SlotPunishmentLockoutActive.IsValidIndex(Index) && SlotPunishmentLockoutActive[Index])
	{
		return EAbilityTileState::PunishmentLockout;
	}
	if (SlotLocked.IsValidIndex(Index) && SlotLocked[Index])
	{
		return EAbilityTileState::NotYetUnlocked;
	}
	if (IsSlotOnCooldown(AbilitySlot))
	{
		return EAbilityTileState::Cooldown;
	}
	return EAbilityTileState::Ready;
}

float UAbilityCooldownTrayWidget::GetSlotPunishmentLockoutRemainingSeconds(EAbilitySlot AbilitySlot) const
{
	const int32 Index = static_cast<int32>(AbilitySlot);
	return SlotPunishmentLockoutRemaining.IsValidIndex(Index) ? SlotPunishmentLockoutRemaining[Index] : 0.0f;
}

float UAbilityCooldownTrayWidget::GetSlotCooldownFillFraction(EAbilitySlot AbilitySlot) const
{
	const int32 Index = static_cast<int32>(AbilitySlot);
	return SlotCooldownFillBars.IsValidIndex(Index) && SlotCooldownFillBars[Index] ? SlotCooldownFillBars[Index]->GetPercent() : 0.0f;
}

bool UAbilityCooldownTrayWidget::IsSlotReadyFlashActive(EAbilitySlot AbilitySlot) const
{
	const int32 Index = static_cast<int32>(AbilitySlot);
	return SlotReadyFlashRemaining.IsValidIndex(Index) && SlotReadyFlashRemaining[Index] > 0.0f;
}

float UAbilityCooldownTrayWidget::GetSlotReadyFlashRemainingSeconds(EAbilitySlot AbilitySlot) const
{
	const int32 Index = static_cast<int32>(AbilitySlot);
	return SlotReadyFlashRemaining.IsValidIndex(Index) ? SlotReadyFlashRemaining[Index] : 0.0f;
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
