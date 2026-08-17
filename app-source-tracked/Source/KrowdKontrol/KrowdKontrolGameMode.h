// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "KrowdKontrolGameMode.generated.h"

// Exists solely to point PlayerControllerClass at AKrowdKontrolPlayerController -
// see issue #132. No project GameMode existed before this (grep-confirmed); both
// playable pawns self-possess via AutoPossessPlayer regardless of GameMode, so this
// class intentionally does nothing else (no DefaultPawnClass override, no
// spawn/login logic).
UCLASS()
class KROWDKONTROL_API AKrowdKontrolGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AKrowdKontrolGameMode();
};
