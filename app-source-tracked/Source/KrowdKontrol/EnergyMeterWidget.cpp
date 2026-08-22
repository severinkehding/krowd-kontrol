#include "EnergyMeterWidget.h"
#include "PlayerEnergyComponent.h"
#include "HUDChromeColours.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/Border.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"

void UEnergyMeterWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	EnsureWidgetTreeBuilt();
}

bool UEnergyMeterWidget::Initialize()
{
	const bool bNewlyInitialized = Super::Initialize();
	if (bNewlyInitialized)
	{
		EnsureWidgetTreeBuilt();
	}
	return bNewlyInitialized;
}

void UEnergyMeterWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	AdvanceDamageFlashTimer(InDeltaTime);
}

void UEnergyMeterWidget::EnsureWidgetTreeBuilt()
{
	// Whichever of NativeOnInitialized()/Initialize() fires first builds the tree; the
	// other is then a no-op, regardless of engine call order between the two.
	if (!FillBar)
	{
		// See UAbilityCooldownTrayWidget::EnsureWidgetTreeBuilt() for why WidgetTree
		// must be lazily created here rather than assumed non-null.
		if (!WidgetTree)
		{
			WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"), RF_Transient);
		}
		BuildWidgetTree();
		SetEnergy(PlaceholderCurrentEnergy, PlaceholderMaxEnergy);
	}
}

void UEnergyMeterWidget::BuildWidgetTree()
{
	// Chrome background/text come from HUDChromeColours (issue #93), shared across all
	// HUD widgets. The fill colour is new (no precedent uses a filled bar yet) - a
	// saturated green, clearly outside the reserved palette (MISSION.md Hard
	// Invariant 3 / PRD 13 REQ-4), since the fill IS informational (unlike the chrome)
	// and benefits from being visually distinct at a glance.
	const FLinearColor ChromeBackgroundColor = HUDChromeColours::GetBackground();
	const FLinearColor FillColor(0.25f, 0.85f, 0.35f, 1.0f);
	const FSlateColor TextColor(HUDChromeColours::GetText());

	UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("MeterRootCanvas"));
	WidgetTree->RootWidget = RootCanvas;

	UOverlay* MeterOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("MeterOverlay"));
	UCanvasPanelSlot* MeterSlot = RootCanvas->AddChildToCanvas(MeterOverlay);
	checkf(MeterSlot, TEXT("EnergyMeterWidget: AddChildToCanvas(MeterOverlay) returned null"));
	// Top-left corner anchoring - diagonally opposite UAbilityCooldownTrayWidget's
	// bottom-right anchor (issue #66), per PRD 13 REQ-2's "opposite" requirement.
	MeterSlot->SetAnchors(FAnchors(0.0f, 0.0f, 0.0f, 0.0f));
	MeterSlot->SetAlignment(FVector2D(0.0f, 0.0f));
	MeterSlot->SetAutoSize(false);
	MeterSlot->SetSize(FVector2D(MeterWidthPx, MeterHeightPx));
	MeterSlot->SetPosition(FVector2D(MeterMarginPx, MeterMarginPx));

	BackgroundBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("MeterBackgroundBorder"));
	BackgroundBorder->SetBrushColor(ChromeBackgroundColor);
	MeterOverlay->AddChildToOverlay(BackgroundBorder);

	FillBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("MeterFillBar"));
	FillBar->SetFillColorAndOpacity(FillColor);
	MeterOverlay->AddChildToOverlay(FillBar);

	ValueText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("MeterValueText"));
	ValueText->SetColorAndOpacity(TextColor);
	UOverlaySlot* TextSlot = MeterOverlay->AddChildToOverlay(ValueText);
	if (TextSlot)
	{
		TextSlot->SetHorizontalAlignment(HAlign_Center);
		TextSlot->SetVerticalAlignment(VAlign_Center);
	}
	else
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UEnergyMeterWidget: AddChildToOverlay(ValueText) returned null slot on '%s' - value text will render unaligned."),
			*GetNameSafe(this));
	}

	// Saturated red, deliberately outside the 5 reserved gameplay colours (Purple/
	// Snare, Teal/Root, Orange/Fear, Blue/Sleep, White/Stun - MISSION.md Hard
	// Invariant 3; nearest is Orange at (1.0, 0.5, 0.0), well separated by the green
	// channel). Invariant 3 reserves these five as an ability/enemy-type-identifying
	// channel ("no other gameplay-relevant object... may use these five colours for
	// non-informational purposes"; "a 6th saturated information colour must never be
	// introduced"). This flash does not identify an ability or enemy type - it is a
	// generic, momentary "you were hit" reaction with no persistent meaning, the same
	// category as a screen-shake or hit-stop, not a 6th entry in the ability/enemy
	// colour channel. PRODUCT-OWNER SIGN-OFF NEEDED (flagged by PR #232 pass-1
	// security review): this reasoning has not yet had explicit human confirmation -
	// see app-changelog/issue-222.md's Governance note.
	const FLinearColor DamageFlashColor(0.95f, 0.12f, 0.12f, 1.0f);

	DamageFlashOverlay = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("MeterDamageFlashOverlay"));
	DamageFlashOverlay->SetBrushColor(DamageFlashColor);
	// Hidden by default - PlayDamageFlash() is what makes this visible. Never
	// ESlateVisibility::Visible - HitTestInvisible (used when shown, in
	// PlayDamageFlash()) keeps it from ever intercepting player input, matching
	// OnScreenPromptWidget.cpp's identical choice.
	DamageFlashOverlay->SetVisibility(ESlateVisibility::Collapsed);
	MeterOverlay->AddChildToOverlay(DamageFlashOverlay); // added last - renders on top of fill/text
}

void UEnergyMeterWidget::SetEnergy(float CurrentEnergy, float MaxEnergy)
{
	const float SafeMax = FMath::Max(0.0f, MaxEnergy);
	const float ClampedCurrent = FMath::Clamp(CurrentEnergy, 0.0f, SafeMax);
	const float Fraction = SafeMax > 0.0f ? ClampedCurrent / SafeMax : 0.0f;

	if (FillBar)
	{
		FillBar->SetPercent(Fraction);
	}
	else
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UEnergyMeterWidget: FillBar is null on '%s' (tree not built?) - fill will not render."),
			*GetNameSafe(this));
	}

	if (ValueText)
	{
		ValueText->SetText(FText::Format(
			NSLOCTEXT("EnergyMeterWidget", "EnergyValueFormat", "{0}/{1}"),
			FText::AsNumber(FMath::RoundToInt(ClampedCurrent)),
			FText::AsNumber(FMath::RoundToInt(SafeMax))));
	}
	else
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UEnergyMeterWidget: ValueText is null on '%s' (tree not built?) - value text will render blank."),
			*GetNameSafe(this));
	}
}

void UEnergyMeterWidget::BindToEnergyComponent(UPlayerEnergyComponent* EnergyComponent)
{
	if (BoundEnergyComponent && BoundEnergyComponent != EnergyComponent)
	{
		BoundEnergyComponent->OnEnergyChanged.RemoveDynamic(this, &UEnergyMeterWidget::HandleEnergyChanged);
	}
	BoundEnergyComponent = EnergyComponent;
	if (!BoundEnergyComponent)
	{
		return;
	}
	BoundEnergyComponent->OnEnergyChanged.AddUniqueDynamic(this, &UEnergyMeterWidget::HandleEnergyChanged);
	SetEnergy(BoundEnergyComponent->GetCurrentEnergy(), BoundEnergyComponent->MaxEnergy);
}

void UEnergyMeterWidget::HandleEnergyChanged(float NewEnergy)
{
	SetEnergy(NewEnergy, BoundEnergyComponent ? BoundEnergyComponent->MaxEnergy : PlaceholderMaxEnergy);
	PlayDamageFlash();
}

void UEnergyMeterWidget::PlayDamageFlash()
{
	DamageFlashRemainingSeconds = DamageFlashDurationSeconds;
	if (DamageFlashOverlay)
	{
		DamageFlashOverlay->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}

void UEnergyMeterWidget::AdvanceDamageFlashTimer(float DeltaSeconds)
{
	if (DamageFlashRemainingSeconds > 0.0f)
	{
		DamageFlashRemainingSeconds = FMath::Max(0.0f, DamageFlashRemainingSeconds - DeltaSeconds);
		if (DamageFlashRemainingSeconds <= 0.0f)
		{
			ClearDamageFlash();
		}
	}
}

void UEnergyMeterWidget::ClearDamageFlash()
{
	if (DamageFlashOverlay)
	{
		DamageFlashOverlay->SetVisibility(ESlateVisibility::Collapsed);
	}
}

bool UEnergyMeterWidget::IsDamageFlashActive() const
{
	return DamageFlashRemainingSeconds > 0.0f;
}

float UEnergyMeterWidget::GetDamageFlashRemainingSeconds() const
{
	return DamageFlashRemainingSeconds;
}

float UEnergyMeterWidget::GetDisplayedFraction() const
{
	return FillBar ? FillBar->GetPercent() : 0.0f;
}

FText UEnergyMeterWidget::GetEnergyDisplayText() const
{
	return ValueText ? ValueText->GetText() : FText::GetEmpty();
}
