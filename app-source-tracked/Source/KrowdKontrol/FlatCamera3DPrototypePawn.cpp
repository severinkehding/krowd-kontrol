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
#include "AbilityCooldownComponent.h"
#include "AbilityCastComponent.h"
#include "AbilityLockoutComponent.h"
#include "AbilityCastVFXComponent.h"
#include "GizmoFirstContactComponent.h"
#include "FirstStunBeaconComponent.h"
#include "AbilityMatchupSignalComponent.h"
#include "AbilityMatchupNudgeComponent.h"
#include "PunishmentManagerComponent.h"
#include "SpeedReductionPunishmentComponent.h"
#include "LevelFailComponent.h"

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
	PunishmentManagerComponent = CreateDefaultSubobject<UPunishmentManagerComponent>(TEXT("PunishmentManagerComponent"));
	PlayerEnergyComponent->OnEnergyChanged.AddDynamic(PunishmentManagerComponent, &UPunishmentManagerComponent::HandleEnergyChanged);
	SpeedReductionPunishmentComponent = CreateDefaultSubobject<USpeedReductionPunishmentComponent>(TEXT("SpeedReductionPunishmentComponent"));
	SpeedReductionPunishmentComponent->MovementComponent = MovementComponent;
	PunishmentManagerComponent->OnPunishmentTriggered.AddDynamic(SpeedReductionPunishmentComponent, &USpeedReductionPunishmentComponent::HandlePunishmentTriggered);
	LevelFailComponent = CreateDefaultSubobject<ULevelFailComponent>(TEXT("LevelFailComponent"));
	PlayerEnergyComponent->OnEnergyChanged.AddDynamic(LevelFailComponent, &ULevelFailComponent::HandleEnergyChanged);
	AbilityCooldownComponent = CreateDefaultSubobject<UAbilityCooldownComponent>(TEXT("AbilityCooldownComponent"));
	AbilityCastComponent = CreateDefaultSubobject<UAbilityCastComponent>(TEXT("AbilityCastComponent"));
	AbilityCastVFXComponent = CreateDefaultSubobject<UAbilityCastVFXComponent>(TEXT("AbilityCastVFXComponent"));
	// Explicit wiring at the call site (same idiom as MovementComponent's
	// SetUpdatedComponent above) rather than a lookup-by-class in BeginPlay -
	// both components are guaranteed to exist by this point in the constructor.
	AbilityCastComponent->OnAbilityCastApplied.AddDynamic(AbilityCastVFXComponent, &UAbilityCastVFXComponent::HandleAbilityCastApplied);

	GizmoFirstContactComponent = CreateDefaultSubobject<UGizmoFirstContactComponent>(TEXT("GizmoFirstContactComponent"));
	AbilityCastComponent->OnAbilityCastApplied.AddDynamic(GizmoFirstContactComponent, &UGizmoFirstContactComponent::HandleAbilityCastApplied);

	FirstStunBeaconComponent = CreateDefaultSubobject<UFirstStunBeaconComponent>(TEXT("FirstStunBeaconComponent"));
	AbilityCastComponent->OnAbilityCastApplied.AddDynamic(FirstStunBeaconComponent, &UFirstStunBeaconComponent::HandleAbilityCastApplied);

	AbilityMatchupSignalComponent = CreateDefaultSubobject<UAbilityMatchupSignalComponent>(TEXT("AbilityMatchupSignalComponent"));
	AbilityCastComponent->OnAbilityCastApplied.AddDynamic(AbilityMatchupSignalComponent, &UAbilityMatchupSignalComponent::HandleAbilityCastApplied);

	AbilityMatchupNudgeComponent = CreateDefaultSubobject<UAbilityMatchupNudgeComponent>(TEXT("AbilityMatchupNudgeComponent"));
	AbilityMatchupSignalComponent->OnAbilityMatchupSignal.AddDynamic(AbilityMatchupNudgeComponent, &UAbilityMatchupNudgeComponent::HandleAbilityMatchupSignal);

	AbilityLockoutComponent = CreateDefaultSubobject<UAbilityLockoutComponent>(TEXT("AbilityLockoutComponent"));
	AbilityCastComponent->OnAbilityCastApplied.AddDynamic(AbilityLockoutComponent, &UAbilityLockoutComponent::HandleAbilityCastApplied);
	PunishmentManagerComponent->OnPunishmentTriggered.AddDynamic(AbilityLockoutComponent, &UAbilityLockoutComponent::HandlePunishmentTriggered);

	AutoPossessPlayer = EAutoReceiveInput::Player0;
}

void AFlatCamera3DPrototypePawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	PlayerInputComponent->BindAxis(TEXT("MoveForward"), this, &AFlatCamera3DPrototypePawn::MoveForward);
	PlayerInputComponent->BindAxis(TEXT("MoveRight"), this, &AFlatCamera3DPrototypePawn::MoveRight);

	PlayerInputComponent->BindAction(TEXT("CastStun"), IE_Pressed, this, &AFlatCamera3DPrototypePawn::CastStunAbility);
	PlayerInputComponent->BindAction(TEXT("CastSleep"), IE_Pressed, this, &AFlatCamera3DPrototypePawn::CastSleepAbility);
	PlayerInputComponent->BindAction(TEXT("CastRoot"), IE_Pressed, this, &AFlatCamera3DPrototypePawn::CastRootAbility);
	PlayerInputComponent->BindAction(TEXT("CastFear"), IE_Pressed, this, &AFlatCamera3DPrototypePawn::CastFearAbility);
	PlayerInputComponent->BindAction(TEXT("CastSnare"), IE_Pressed, this, &AFlatCamera3DPrototypePawn::CastSnareAbility);
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

void AFlatCamera3DPrototypePawn::CastStunAbility()
{
	if (AbilityCastComponent)
	{
		AbilityCastComponent->TryCastAbility(EAbilitySlot::Stun);
	}
}

void AFlatCamera3DPrototypePawn::CastSleepAbility()
{
	if (AbilityCastComponent)
	{
		AbilityCastComponent->TryCastAbility(EAbilitySlot::Sleep);
	}
}

void AFlatCamera3DPrototypePawn::CastRootAbility()
{
	if (AbilityCastComponent)
	{
		AbilityCastComponent->TryCastAbility(EAbilitySlot::Root);
	}
}

void AFlatCamera3DPrototypePawn::CastFearAbility()
{
	if (AbilityCastComponent)
	{
		AbilityCastComponent->TryCastAbility(EAbilitySlot::Fear);
	}
}

void AFlatCamera3DPrototypePawn::CastSnareAbility()
{
	if (AbilityCastComponent)
	{
		AbilityCastComponent->TryCastAbility(EAbilitySlot::Snare);
	}
}
