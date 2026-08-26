#include "MainMenuGameMode.h"
#include "MainMenuPlayerController.h"

AMainMenuGameMode::AMainMenuGameMode()
{
	PlayerControllerClass = AMainMenuPlayerController::StaticClass();
}
