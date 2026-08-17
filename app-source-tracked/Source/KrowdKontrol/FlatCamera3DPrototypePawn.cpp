// Fill out your copyright notice in the Description page of Project Settings.

#include "FlatCamera3DPrototypePawn.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/InputComponent.h"
#include "AbilityUnlockComponent.h"
#include "PlayerEnergyComponent.h"

AFlatCamera3DPrototypePawn::AFlatCamera3DPrototypePawn()
{
	PrimaryActorTick.bCanEverTick = false;

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	RootComponent = MeshComponent;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMeshFinder(
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMeshFinder.Succeeded())
	{
		MeshComponent->SetStaticMesh(CubeMeshFinder.Object);
	}

	MovementComponent = CreateDefaultSubobject<UFloatingPawnMovement>(TEXT("MovementComponent"));
	// MeshComponent is already RootComponent, so OnRegister()'s auto-detection would
	// reach the same UpdatedComponent via the same setter either way. Set explicitly
	// here anyway so the wiring is visible at the call site rather than implicit.
	MovementComponent->SetUpdatedComponent(MeshComponent);

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->SetRelativeRotation(FRotator(-80.0f, 0.0f, 0.0f));
	CameraBoom->TargetArmLength = 800.0f;
	CameraBoom->bDoCollisionTest = false;
	CameraBoom->bUsePawnControlRotation = false;

	TopDownCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("TopDownCamera"));
	TopDownCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	TopDownCamera->bUsePawnControlRotation = false;

	AbilityUnlockComponent = CreateDefaultSubobject<UAbilityUnlockComponent>(TEXT("AbilityUnlockComponent"));
	PlayerEnergyComponent = CreateDefaultSubobject<UPlayerEnergyComponent>(TEXT("PlayerEnergyComponent"));

	AutoPossessPlayer = EAutoReceiveInput::Player0;
}

void AFlatCamera3DPrototypePawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	PlayerInputComponent->BindAxis(TEXT("MoveForward"), this, &AFlatCamera3DPrototypePawn::MoveForward);
	PlayerInputComponent->BindAxis(TEXT("MoveRight"), this, &AFlatCamera3DPrototypePawn::MoveRight);
}

void AFlatCamera3DPrototypePawn::MoveForward(float Value)
{
	// World-space axis, not actor-relative - keeps top-down movement independent of the
	// pawn's facing, matching "basic WASD/arrow input" scope rather than a
	// rotate-to-face-movement scheme.
	AddMovementInput(FVector::ForwardVector, Value);
}

void AFlatCamera3DPrototypePawn::MoveRight(float Value)
{
	AddMovementInput(FVector::RightVector, Value);
}
