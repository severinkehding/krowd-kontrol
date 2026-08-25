#include "PunishmentDebugMenuWidget.h"
#include "AbilityLockoutComponent.h"
#include "SpeedReductionPunishmentComponent.h"
#include "OvercrowdDetectionComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/VerticalBox.h"
#include "Components/HorizontalBox.h"
#include "Components/Border.h"
#include "Components/PanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/CheckBox.h"
#include "HUDChromeColours.h"
#include "HAL/IConsoleManager.h"

void UPunishmentDebugMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	EnsureWidgetTreeBuilt();
}

bool UPunishmentDebugMenuWidget::Initialize()
{
	const bool bNewlyInitialized = Super::Initialize();
	if (bNewlyInitialized)
	{
		EnsureWidgetTreeBuilt();
	}
	return bNewlyInitialized;
}

void UPunishmentDebugMenuWidget::EnsureWidgetTreeBuilt()
{
	// Whichever of NativeOnInitialized()/Initialize() fires first builds the tree; the
	// other is then a no-op, regardless of engine call order between the two - mirrors
	// UAbilityCooldownTrayWidget::EnsureWidgetTreeBuilt().
	if (!LockoutCheckBox)
	{
		if (!WidgetTree)
		{
			WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"), RF_Transient);
		}
		BuildWidgetTree();
	}
}

void UPunishmentDebugMenuWidget::BuildWidgetTree()
{
	const FSlateColor TextColor(HUDChromeColours::GetText());

	// First UCanvasPanel usage needed for the same reason as
	// UAbilityCooldownTrayWidget::BuildWidgetTree() - centering the menu requires a
	// UCanvasPanelSlot, which only a UCanvasPanel child gets.
	UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("PunishmentDebugMenuRootCanvas"));
	WidgetTree->RootWidget = RootCanvas;

	UBorder* RootBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PunishmentDebugMenuBorder"));
	RootBorder->SetBrushColor(HUDChromeColours::GetBackground());

	UCanvasPanelSlot* BorderSlot = RootCanvas->AddChildToCanvas(RootBorder);
	checkf(BorderSlot, TEXT("PunishmentDebugMenuWidget: AddChildToCanvas(RootBorder) returned null"));
	BorderSlot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
	BorderSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	BorderSlot->SetAutoSize(true);

	UVerticalBox* RowsBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("PunishmentDebugMenuRowsBox"));
	UPanelSlot* RowsBoxSlot = RootBorder->SetContent(RowsBox);
	checkf(RowsBoxSlot, TEXT("PunishmentDebugMenuWidget: SetContent(RowsBox) returned null"));

	UTextBlock* TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PunishmentDebugMenuTitle"));
	TitleText->SetText(FText::FromString(TEXT("Punishment Debug Toggles")));
	TitleText->SetColorAndOpacity(TextColor);
	RowsBox->AddChildToVerticalBox(TitleText);

	// Three explicit row blocks - only 3 fixed, named items, matching this codebase's
	// preference for explicit per-item code over a loop/lambda when the count is small
	// and fixed.

	UHorizontalBox* LockoutRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("PunishmentDebugMenuLockoutRow"));
	LockoutCheckBox = WidgetTree->ConstructWidget<UCheckBox>(UCheckBox::StaticClass(), TEXT("PunishmentDebugMenuLockoutCheckBox"));
	LockoutCheckBox->OnCheckStateChanged.AddDynamic(this, &UPunishmentDebugMenuWidget::HandleLockoutCheckStateChanged);
	LockoutRow->AddChildToHorizontalBox(LockoutCheckBox);
	UTextBlock* LockoutLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PunishmentDebugMenuLockoutLabel"));
	LockoutLabel->SetText(FText::FromString(TEXT("Ability Lockout (P1)")));
	LockoutLabel->SetColorAndOpacity(TextColor);
	LockoutRow->AddChildToHorizontalBox(LockoutLabel);
	RowsBox->AddChildToVerticalBox(LockoutRow);

	UHorizontalBox* SpeedReductionRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("PunishmentDebugMenuSpeedReductionRow"));
	SpeedReductionCheckBox = WidgetTree->ConstructWidget<UCheckBox>(UCheckBox::StaticClass(), TEXT("PunishmentDebugMenuSpeedReductionCheckBox"));
	SpeedReductionCheckBox->OnCheckStateChanged.AddDynamic(this, &UPunishmentDebugMenuWidget::HandleSpeedReductionCheckStateChanged);
	SpeedReductionRow->AddChildToHorizontalBox(SpeedReductionCheckBox);
	UTextBlock* SpeedReductionLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PunishmentDebugMenuSpeedReductionLabel"));
	SpeedReductionLabel->SetText(FText::FromString(TEXT("Speed Reduction (P2)")));
	SpeedReductionLabel->SetColorAndOpacity(TextColor);
	SpeedReductionRow->AddChildToHorizontalBox(SpeedReductionLabel);
	RowsBox->AddChildToVerticalBox(SpeedReductionRow);

	UHorizontalBox* OvercrowdRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("PunishmentDebugMenuOvercrowdRow"));
	OvercrowdCheckBox = WidgetTree->ConstructWidget<UCheckBox>(UCheckBox::StaticClass(), TEXT("PunishmentDebugMenuOvercrowdCheckBox"));
	OvercrowdCheckBox->OnCheckStateChanged.AddDynamic(this, &UPunishmentDebugMenuWidget::HandleOvercrowdCheckStateChanged);
	OvercrowdRow->AddChildToHorizontalBox(OvercrowdCheckBox);
	UTextBlock* OvercrowdLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PunishmentDebugMenuOvercrowdLabel"));
	OvercrowdLabel->SetText(FText::FromString(TEXT("Overcrowd (P3)")));
	OvercrowdLabel->SetColorAndOpacity(TextColor);
	OvercrowdRow->AddChildToHorizontalBox(OvercrowdLabel);
	RowsBox->AddChildToVerticalBox(OvercrowdRow);

	// Checkbox state must reflect whatever each CVar already is (e.g. a previous
	// session/console command left it disabled), not always default to checked.
	if (IConsoleVariable* LockoutCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("kk.Punishment.LockoutEnabled")))
	{
		LockoutCheckBox->SetIsChecked(LockoutCVar->GetInt() != 0);
	}
	if (IConsoleVariable* SpeedReductionCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("kk.Punishment.SpeedReductionEnabled")))
	{
		SpeedReductionCheckBox->SetIsChecked(SpeedReductionCVar->GetInt() != 0);
	}
	if (IConsoleVariable* OvercrowdCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("kk.Punishment.OvercrowdEnabled")))
	{
		OvercrowdCheckBox->SetIsChecked(OvercrowdCVar->GetInt() != 0);
	}

	// Hidden by default - toggled into view by AKrowdKontrolPlayerController::
	// HandleToggleDebugMenu() via ToggleMenuVisibility().
	SetVisibility(ESlateVisibility::Collapsed);
}

void UPunishmentDebugMenuWidget::BindPunishmentComponents(
	UAbilityLockoutComponent* InLockoutComponent,
	USpeedReductionPunishmentComponent* InSpeedReductionComponent,
	UOvercrowdDetectionComponent* InOvercrowdComponent)
{
	BoundLockoutComponent = InLockoutComponent;
	BoundSpeedReductionComponent = InSpeedReductionComponent;
	BoundOvercrowdComponent = InOvercrowdComponent;
}

void UPunishmentDebugMenuWidget::ToggleMenuVisibility()
{
	SetVisibility(IsMenuVisible() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
}

bool UPunishmentDebugMenuWidget::IsMenuVisible() const
{
	return GetVisibility() == ESlateVisibility::Visible;
}

bool UPunishmentDebugMenuWidget::IsLockoutToggleChecked() const
{
	return LockoutCheckBox && LockoutCheckBox->IsChecked();
}

bool UPunishmentDebugMenuWidget::IsSpeedReductionToggleChecked() const
{
	return SpeedReductionCheckBox && SpeedReductionCheckBox->IsChecked();
}

bool UPunishmentDebugMenuWidget::IsOvercrowdToggleChecked() const
{
	return OvercrowdCheckBox && OvercrowdCheckBox->IsChecked();
}

void UPunishmentDebugMenuWidget::HandleLockoutCheckStateChanged(bool bIsChecked)
{
	if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("kk.Punishment.LockoutEnabled")))
	{
		CVar->Set(bIsChecked ? 1 : 0);
	}
	if (!bIsChecked)
	{
		if (UAbilityLockoutComponent* Component = BoundLockoutComponent.Get())
		{
			Component->EndAllLockouts();
		}
	}
}

void UPunishmentDebugMenuWidget::HandleSpeedReductionCheckStateChanged(bool bIsChecked)
{
	if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("kk.Punishment.SpeedReductionEnabled")))
	{
		CVar->Set(bIsChecked ? 1 : 0);
	}
	if (!bIsChecked)
	{
		if (USpeedReductionPunishmentComponent* Component = BoundSpeedReductionComponent.Get())
		{
			Component->EndSpeedReduction();
		}
	}
}

void UPunishmentDebugMenuWidget::HandleOvercrowdCheckStateChanged(bool bIsChecked)
{
	if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("kk.Punishment.OvercrowdEnabled")))
	{
		CVar->Set(bIsChecked ? 1 : 0);
	}
	if (!bIsChecked)
	{
		if (UOvercrowdDetectionComponent* Component = BoundOvercrowdComponent.Get())
		{
			Component->ForceEndPanicOverload();
		}
	}
}
