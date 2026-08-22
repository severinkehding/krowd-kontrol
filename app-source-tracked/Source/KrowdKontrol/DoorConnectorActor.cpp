#include "DoorConnectorActor.h"
#include "RoomActor.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

ADoorConnectorActor::ADoorConnectorActor()
{
	PrimaryActorTick.bCanEverTick = false;

	USceneComponent* DoorConnectorRoot = CreateDefaultSubobject<USceneComponent>(TEXT("DoorConnectorRoot"));
	RootComponent = DoorConnectorRoot;

	ConnectorFloorMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ConnectorFloorMeshComponent"));
	ConnectorFloorMeshComponent->SetupAttachment(DoorConnectorRoot);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMeshFinder(
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMeshFinder.Succeeded())
	{
		ConnectorFloorMeshComponent->SetStaticMesh(CubeMeshFinder.Object);
	}
	// RoomA/RoomB aren't set yet at construction time - start hidden until
	// RecomputeConnectorGeometry() has something valid to show.
	ConnectorFloorMeshComponent->SetVisibility(false);

	DoorMarkerMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorMarkerMeshComponent"));
	DoorMarkerMeshComponent->SetupAttachment(DoorConnectorRoot);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> MarkerMeshFinder(
		TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (MarkerMeshFinder.Succeeded())
	{
		DoorMarkerMeshComponent->SetStaticMesh(MarkerMeshFinder.Object);
	}
	// Small placeholder "lantern" - engine sphere at half the room-cube's 100uu unit
	// size, deliberately not scaled to any real art proportions yet.
	DoorMarkerMeshComponent->SetRelativeScale3D(FVector(0.5f, 0.5f, 0.5f));
	// No collision - a visual-only wayfinding marker must never block the connector
	// path players walk through, mirroring ARoomActor's wall meshes (RoomActor.cpp:24).
	DoorMarkerMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	// RoomA/RoomB aren't set yet at construction time - starts hidden until
	// RecomputeConnectorGeometry() has something valid to show, same reasoning as
	// ConnectorFloorMeshComponent above.
	DoorMarkerMeshComponent->SetVisibility(false);

	DoorMarkerLightComponent = CreateDefaultSubobject<UPointLightComponent>(TEXT("DoorMarkerLightComponent"));
	DoorMarkerLightComponent->SetupAttachment(DoorMarkerMeshComponent);
	// SetupAttachment does not imply inherited visibility - a light component's own
	// bVisible defaults true regardless of its parent's, so without this it would shine
	// at the actor's origin even while DoorMarkerMeshComponent above is hidden. Kept in
	// lockstep with the marker mesh in RecomputeConnectorGeometry() below.
	DoorMarkerLightComponent->SetVisibility(false);
	// Desaturated warm neutral, deliberately NOT saturated - issue #72's beacon used a
	// saturated green and drew an unresolved CRITICAL review finding over whether that
	// constitutes a 6th Hard Invariant 3 colour (see that issue's comments). Issue #186's
	// lighting rig sidestepped the same question with a desaturated neutral and shipped
	// clean; this marker follows #186's approach rather than #72's. Also not one of the
	// 5 reserved colours (Purple/Teal/Orange/Blue/White) - see
	// ReservedGameplayColours.h.
	DoorMarkerLightComponent->SetLightColor(FLinearColor(0.75f, 0.65f, 0.5f));
	// Placeholder tuning, not calibrated against any real level's ambient lighting yet.
	DoorMarkerLightComponent->SetIntensity(2500.0f);
	DoorMarkerLightComponent->SetAttenuationRadius(250.0f);

	GateBlockingComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("GateBlockingComponent"));
	GateBlockingComponent->SetupAttachment(DoorConnectorRoot);
	// Starts open (no collision) - matches bIsGateOpen's default and "ungated until a
	// GatingRoom is assigned" behaviour; RefreshGateState() corrects this once real
	// room/gating data is available (called from BeginPlay, after GatingRoom resolves).
	GateBlockingComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	// Unconfigured primitives default to BlockAllDynamic - reset to Ignore-all first so
	// the gate only ever blocks the one channel it's meant to (see RefreshGateState()'s
	// comment below for why that channel is WorldStatic, not Pawn).
	GateBlockingComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	GateBlockingComponent->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	GateBlockingComponent->SetGenerateOverlapEvents(false);
}

void ADoorConnectorActor::HideConnectorVisuals()
{
	ConnectorFloorMeshComponent->SetVisibility(false);
	DoorMarkerMeshComponent->SetVisibility(false);
	DoorMarkerLightComponent->SetVisibility(false);
	GateBlockingComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ADoorConnectorActor::RecomputeConnectorGeometry()
{
	if (!ConnectsValidRooms())
	{
		HideConnectorVisuals();
		return;
	}

	const FVector LocationA = RoomA->GetActorLocation();
	const FVector LocationB = RoomB->GetActorLocation();
	const FVector Midpoint = (LocationA + LocationB) * 0.5f;
	const FVector Delta = LocationB - LocationA;
	const float Length = Delta.Size();
	if (Length <= KINDA_SMALL_NUMBER)
	{
		HideConnectorVisuals();
		return;
	}

	// Top face at local Z=0, matching ARoomActor's own floor convention, so room floors
	// and the connector strip sit flush at the same height.
	ConnectorFloorMeshComponent->SetWorldLocation(Midpoint - FVector(0.f, 0.f, ConnectorFloorThickness * 0.5f));
	ConnectorFloorMeshComponent->SetWorldRotation(Delta.Rotation());
	ConnectorFloorMeshComponent->SetWorldScale3D(FVector(Length / 100.f, ConnectorFloorWidth / 100.f, ConnectorFloorThickness / 100.f));
	ConnectorFloorMeshComponent->SetVisibility(true);

	DoorMarkerMeshComponent->SetWorldLocation(Midpoint + FVector(0.f, 0.f, DoorMarkerHeight));
	DoorMarkerMeshComponent->SetVisibility(true);
	DoorMarkerLightComponent->SetVisibility(true);

	GateBlockingComponent->SetWorldLocation(Midpoint);
	GateBlockingComponent->SetWorldRotation(Delta.Rotation());
	GateBlockingComponent->SetBoxExtent(FVector(
		ConnectorFloorThickness, ConnectorFloorWidth * 0.5f, DoorMarkerHeight));
}

void ADoorConnectorActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RecomputeConnectorGeometry();
}

void ADoorConnectorActor::BeginPlay()
{
	Super::BeginPlay();
	RecomputeConnectorGeometry();

	if (!GatingRoom && ConnectsValidRooms())
	{
		GatingRoom = RoomA->GetActorLocation().X <= RoomB->GetActorLocation().X ? RoomA : RoomB;
	}

	if (GatingRoom)
	{
		GatingRoom->OnRoomClearedStateChanged.AddUniqueDynamic(this, &ADoorConnectorActor::RefreshGateState);
	}
	else
	{
		UE_LOG(LogTemp, Warning,
			TEXT("ADoorConnectorActor: '%s' has no resolvable GatingRoom (RoomA/RoomB unset, ")
			TEXT("identical, or auto-derivation skipped) - this door will never gate and stays ")
			TEXT("permanently open."),
			*GetNameSafe(this));
	}

	RefreshGateState();
}

void ADoorConnectorActor::RefreshGateState()
{
	bIsGateOpen = (GatingRoom == nullptr) || GatingRoom->IsRoomCleared();

	// Root-cause fix (issue #218, attempt 2): the real player pawn's moving collision
	// component (AFlatCamera3DPrototypePawn::MeshComponent) has never had an explicit
	// profile set, so it inherits UStaticMeshComponent's engine-default "BlockAll"
	// profile - object type WorldStatic, not Pawn (FlatCamera3DPrototypePawn.cpp never
	// calls SetCollisionObjectType/SetCollisionProfileName; RoomActor.cpp's
	// FloorMeshComponent comment confirms the same default already applies to the level
	// geometry the player walks on today). Attempt 1 blocked only ECC_Pawn here, which
	// is a no-op against the real player - QueryOnly still blocks WorldStatic-channel
	// traces (the constructor's collision-response setup above), so this line only
	// needs to toggle enabled/disabled, not the per-channel responses.
	GateBlockingComponent->SetCollisionEnabled(
		bIsGateOpen ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryOnly);
}
