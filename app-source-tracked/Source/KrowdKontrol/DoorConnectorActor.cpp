#include "DoorConnectorActor.h"
#include "RoomActor.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
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
}

void ADoorConnectorActor::RecomputeConnectorGeometry()
{
	if (!ConnectsValidRooms())
	{
		ConnectorFloorMeshComponent->SetVisibility(false);
		return;
	}

	const FVector LocationA = RoomA->GetActorLocation();
	const FVector LocationB = RoomB->GetActorLocation();
	const FVector Midpoint = (LocationA + LocationB) * 0.5f;
	const FVector Delta = LocationB - LocationA;
	const float Length = Delta.Size();
	if (Length <= KINDA_SMALL_NUMBER)
	{
		ConnectorFloorMeshComponent->SetVisibility(false);
		return;
	}

	// Top face at local Z=0, matching ARoomActor's own floor convention (Task 2), so
	// room floors and the connector strip sit flush at the same height.
	ConnectorFloorMeshComponent->SetWorldLocation(Midpoint - FVector(0.f, 0.f, ConnectorFloorThickness * 0.5f));
	ConnectorFloorMeshComponent->SetWorldRotation(Delta.Rotation());
	ConnectorFloorMeshComponent->SetWorldScale3D(FVector(Length / 100.f, ConnectorFloorWidth / 100.f, ConnectorFloorThickness / 100.f));
	ConnectorFloorMeshComponent->SetVisibility(true);
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
}
