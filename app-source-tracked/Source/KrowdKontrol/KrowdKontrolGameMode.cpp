// Fill out your copyright notice in the Description page of Project Settings.

#include "KrowdKontrolGameMode.h"
#include "KrowdKontrolPlayerController.h"

AKrowdKontrolGameMode::AKrowdKontrolGameMode()
{
	PlayerControllerClass = AKrowdKontrolPlayerController::StaticClass();
}
