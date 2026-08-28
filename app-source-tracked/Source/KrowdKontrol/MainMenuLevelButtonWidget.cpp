#include "MainMenuLevelButtonWidget.h"
#include "HUDChromeColours.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"

void UMainMenuLevelButtonWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	EnsureWidgetTreeBuilt();
}

bool UMainMenuLevelButtonWidget::Initialize()
{
	const bool bNewlyInitialized = Super::Initialize();
	if (bNewlyInitialized)
	{
		EnsureWidgetTreeBuilt();
	}
	return bNewlyInitialized;
}

void UMainMenuLevelButtonWidget::EnsureWidgetTreeBuilt()
{
	if (!LevelButton)
	{
		if (!WidgetTree)
		{
			WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"), RF_Transient);
		}
		BuildWidgetTree();
	}
}

void UMainMenuLevelButtonWidget::BuildWidgetTree()
{
	LevelButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("MainMenuLevelButton"));
	LevelButton->OnClicked.AddDynamic(this, &UMainMenuLevelButtonWidget::HandleClicked);
	WidgetTree->RootWidget = LevelButton;

	LevelButtonLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("MainMenuLevelButtonLabel"));
	LevelButtonLabel->SetColorAndOpacity(FSlateColor(HUDChromeColours::GetText()));
	LevelButton->SetContent(LevelButtonLabel); // UButton is a UContentWidget, not a UPanelWidget - SetContent(), not AddChild().

	RefreshLabel();
}

void UMainMenuLevelButtonWidget::SetLevelMapName(FName InMapName)
{
	MapName = InMapName;
	RefreshLabel();
}

void UMainMenuLevelButtonWidget::RefreshLabel()
{
	if (LevelButtonLabel)
	{
		LevelButtonLabel->SetText(FText::FromName(MapName));
	}
}

void UMainMenuLevelButtonWidget::HandleClicked()
{
	OnLevelSelected.Broadcast(MapName);
}
