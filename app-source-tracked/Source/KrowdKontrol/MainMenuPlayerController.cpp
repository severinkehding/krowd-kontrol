#include "MainMenuPlayerController.h"
#include "MainMenuWidget.h"

void AMainMenuPlayerController::BeginPlay()
{
	Super::BeginPlay();
	// Mirrors AKrowdKontrolPlayerController::BeginPlay()'s issue #262 cursor precedent
	// exactly. SetInputMode() is deliberately NOT added speculatively here - see
	// app-changelog/issue-324.md for the rationale.
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
