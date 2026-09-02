#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "IntroGameMode.generated.h"

// Game mode for the intro flythrough map (L_Intro): no pawn, no HUD, no game
// systems - players start as spectators and AIntroFlythroughDirector takes the
// view over on BeginPlay. Set as the intro map's WorldSettings game-mode
// override; the project default game mode stays untouched.
UCLASS()
class KROWDKONTROL_API AIntroGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AIntroGameMode();
};
