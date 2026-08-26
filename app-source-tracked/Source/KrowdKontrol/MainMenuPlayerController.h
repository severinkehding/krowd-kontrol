#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "MainMenuPlayerController.generated.h"

class UMainMenuWidget;

// Displays UMainMenuWidget on BeginPlay (issue #324). Deliberately NOT
// AKrowdKontrolPlayerController - that class's CreateHUDWidgets() wires up 7
// gameplay-only HUD widgets (ability tray, energy meter, quest tracker, etc.) that
// have no meaning on a menu screen and would need to be conditionally suppressed;
// a dedicated, minimal controller (mirroring AMainMenuGameMode's separation from
// AKrowdKontrolGameMode) avoids adding menu-awareness branches into gameplay code
// that 15+ existing tests already exercise.
UCLASS()
class KROWDKONTROL_API AMainMenuPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;

	UPROPERTY(BlueprintReadOnly, Category = "Main Menu")
	TObjectPtr<UMainMenuWidget> MainMenuWidgetInstance;
};
