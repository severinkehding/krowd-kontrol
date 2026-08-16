// Fill out your copyright notice in the Description page of Project Settings.

#include "Paper2DPrototypePawn.h"
#include "PaperSpriteComponent.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/InputComponent.h"

APaper2DPrototypePawn::APaper2DPrototypePawn()
{
	PrimaryActorTick.bCanEverTick = false;

	SpriteComponent = CreateDefaultSubobject<UPaperSpriteComponent>(TEXT("SpriteComponent"));
	RootComponent = SpriteComponent;

	// No UPaperSprite asset assignment here - unlike flat-camera-3D's
	// /Engine/BasicShapes/Cube.Cube, Paper2D ships no engine-default sprite asset to
	// FObjectFinder against. See docs/paper2d-prototype-notes.md.

	// Paper2D sprites default to a vertical XZ-plane orientation meant for side-view
	// cameras, which would render edge-on (invisible) under a top-down camera. This lays
	// the sprite flat in the ground (XY) plane instead.
	SpriteComponent->SetRelativeRotation(FRotator(-90.0f, 0.0f, 0.0f));

	MovementComponent = CreateDefaultSubobject<UFloatingPawnMovement>(TEXT("MovementComponent"));
	// Goes through the engine's setter rather than a raw field assignment so
	// UpdatedPrimitive gets populated and the physics-volume-changed delegate gets bound -
	// both are side effects OnRegister()'s auto-detection wouldn't have skipped, but a
	// direct field write would.
	MovementComponent->SetUpdatedComponent(SpriteComponent);

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	// CameraBoom is attached to RootComponent, which is SpriteComponent - so this
	// relative pitch composes with SpriteComponent's own -90 rotation above rather than
	// being independent of it. TODO(#55): verify the resulting world-space camera
	// direction is actually straight down; the automation tests below only assert the
	// boom's *relative* pitch, not its composed world pitch.
	CameraBoom->SetRelativeRotation(FRotator(-90.0f, 0.0f, 0.0f));
	CameraBoom->TargetArmLength = 800.0f;
	CameraBoom->bDoCollisionTest = false;
	CameraBoom->bUsePawnControlRotation = false;

	TopDownCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("TopDownCamera"));
	TopDownCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	TopDownCamera->bUsePawnControlRotation = false;
	TopDownCamera->ProjectionMode = ECameraProjectionMode::Orthographic;
	TopDownCamera->OrthoWidth = 1024.0f;

	AutoPossessPlayer = EAutoReceiveInput::Player0;
}

void APaper2DPrototypePawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	PlayerInputComponent->BindAxis(TEXT("MoveForward"), this, &APaper2DPrototypePawn::MoveForward);
	PlayerInputComponent->BindAxis(TEXT("MoveRight"), this, &APaper2DPrototypePawn::MoveRight);
}

void APaper2DPrototypePawn::MoveForward(float Value)
{
	// World-space axis, not actor-relative - keeps top-down movement independent of the
	// pawn's facing, matching "basic WASD/arrow input" scope rather than a
	// rotate-to-face-movement scheme.
	AddMovementInput(FVector::ForwardVector, Value);
}

void APaper2DPrototypePawn::MoveRight(float Value)
{
	AddMovementInput(FVector::RightVector, Value);
}
