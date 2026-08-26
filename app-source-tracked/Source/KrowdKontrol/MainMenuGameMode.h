#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "MainMenuGameMode.generated.h"

// Exists solely to point PlayerControllerClass at AMainMenuPlayerController - same
// single-purpose shape as AKrowdKontrolGameMode (issue #132). Deliberately a
// separate class, not a reuse of AKrowdKontrolGameMode: this GameMode is assigned
// per-map (via WorldSettings -> GameMode Override on the temp test map, later
// L_MainMenu), never as the project's GlobalDefaultGameMode, so it must not
// conflict with or replace AKrowdKontrolGameMode's existing gameplay-level wiring.
UCLASS()
class KROWDKONTROL_API AMainMenuGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AMainMenuGameMode();
};
