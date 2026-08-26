#include "MainMenuPlayerController.h"
#include "MainMenuWidget.h"

void AMainMenuPlayerController::BeginPlay()
{
	Super::BeginPlay();
	// Mirrors AKrowdKontrolPlayerController::BeginPlay()'s issue #262 cursor precedent
	// exactly - see this plan's Risks section for why SetInputMode() is deliberately
	// NOT added speculatively here.
	bShowMouseCursor = true;
	if (!MainMenuWidgetInstance)
	{
		MainMenuWidgetInstance = CreateWidget<UMainMenuWidget>(this, UMainMenuWidget::StaticClass());
		if (MainMenuWidgetInstance)
		{
			MainMenuWidgetInstance->AddToViewport();
		}
	}
}
