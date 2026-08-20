// Fill out your copyright notice in the Description page of Project Settings.

#include "Paper2DPrototypePawn.h"
#include "Components/SceneComponent.h"
#include "PaperSpriteComponent.h"
#include "PaperSprite.h"
#include "UObject/ConstructorHelpers.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/InputComponent.h"
#include "PlayerEnergyComponent.h"
#include "PunishmentManagerComponent.h"
#include "SpeedReductionPunishmentComponent.h"
#include "LevelFailComponent.h"

APaper2DPrototypePawn::APaper2DPrototypePawn()
{
	PrimaryActorTick.bCanEverTick = false;

	// Unrotated root, not SpriteComponent. SpriteComponent is tilted -90 pitch to lay
	// flat into the ground plane (see below); if CameraBoom were attached to that
	// tilted component instead of to this identity-rotation root, its own -90 relative
	// pitch would compose with the sprite's -90 tilt into a 180-degree world rotation -
	// the camera ends up facing horizontally backward-and-upside-down rather than
	// straight down (verified directly against FRotator::Quaternion()'s composition
	// formula; see docs/paper2d-prototype-notes.md). Keep SpriteComponent and
	// CameraBoom as siblings under this root so each one's relative rotation is also
	// its true world rotation - matches the RoomActor/DoorConnectorActor convention
	// already used elsewhere in this module.
	PawnRoot = CreateDefaultSubobject<USceneComponent>(TEXT("PawnRoot"));
	RootComponent = PawnRoot;

	SpriteComponent = CreateDefaultSubobject<UPaperSpriteComponent>(TEXT("SpriteComponent"));
	SpriteComponent->SetupAttachment(RootComponent);

	// Paper2D sprites default to a vertical XZ-plane orientation meant for side-view
	// cameras, which would render edge-on (invisible) under a top-down camera. This lays
	// the sprite flat in the ground (XY) plane instead.
	SpriteComponent->SetRelativeRotation(FRotator(-90.0f, 0.0f, 0.0f));

	// Engine-shipped default sprite, directly analogous to the flat-camera-3D sibling's
	// /Engine/BasicShapes/Cube.Cube - closes the "no default sprite to assign" gap a
	// prior attempt left open.
	static ConstructorHelpers::FObjectFinder<UPaperSprite> DummySpriteFinder(
		TEXT("/Paper2D/DummySprite.DummySprite"));
	if (DummySpriteFinder.Succeeded())
	{
		SpriteComponent->SetSprite(DummySpriteFinder.Object);
	}

	MovementComponent = CreateDefaultSubobject<UFloatingPawnMovement>(TEXT("MovementComponent"));
	// PawnRoot, not SpriteComponent - so translating the pawn moves the sprite and the
	// camera boom together. Pointing this at SpriteComponent would move the sprite
	// without moving the camera, visually detaching the camera from the pawn as it
	// moves.
	MovementComponent->SetUpdatedComponent(PawnRoot);

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	// Attached to RootComponent (PawnRoot, identity rotation), not SpriteComponent - so
	// this relative pitch is not composed through the sprite's own -90 tilt. See the
	// PawnRoot comment above for why.
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->SetRelativeRotation(FRotator(-90.0f, 0.0f, 0.0f));
	CameraBoom->TargetArmLength = 800.0f;
	CameraBoom->bDoCollisionTest = false;
	CameraBoom->bUsePawnControlRotation = false;

	TopDownCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("TopDownCamera"));
	TopDownCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	TopDownCamera->bUsePawnControlRotation = false;
	TopDownCamera->ProjectionMode = ECameraProjectionMode::Orthographic;
	TopDownCamera->OrthoWidth = 1024.0f;

	PlayerEnergyComponent = CreateDefaultSubobject<UPlayerEnergyComponent>(TEXT("PlayerEnergyComponent"));
	PunishmentManagerComponent = CreateDefaultSubobject<UPunishmentManagerComponent>(TEXT("PunishmentManagerComponent"));
	PlayerEnergyComponent->OnEnergyChanged.AddDynamic(PunishmentManagerComponent, &UPunishmentManagerComponent::HandleEnergyChanged);
	SpeedReductionPunishmentComponent = CreateDefaultSubobject<USpeedReductionPunishmentComponent>(TEXT("SpeedReductionPunishmentComponent"));
	SpeedReductionPunishmentComponent->MovementComponent = MovementComponent;
	PunishmentManagerComponent->OnPunishmentTriggered.AddDynamic(SpeedReductionPunishmentComponent, &USpeedReductionPunishmentComponent::HandlePunishmentTriggered);
	LevelFailComponent = CreateDefaultSubobject<ULevelFailComponent>(TEXT("LevelFailComponent"));
	PlayerEnergyComponent->OnEnergyChanged.AddDynamic(LevelFailComponent, &ULevelFailComponent::HandleEnergyChanged);

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
