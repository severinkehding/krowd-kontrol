#include "RoomActor.h"
#include "PlaceholderTargetZoneActor.h"
#include "EnemyBase.h"
#include "Engine/World.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// Shared by all 4 wall mesh components below - the engine's
	// /Engine/BasicShapes/Cube.Cube is a 100x100x100uu cube, so scale = desired
	// size-in-cm / 100. Walls have collision disabled: ARoomActor has no per-door
	// "which wall side" data, so a solid wall on all 4 sides would seal off the very
	// connector paths this issue also requires to be walkable.
	void SetupWallMeshComponent(UStaticMeshComponent* WallMeshComponent, UStaticMesh* CubeMesh,
		USceneComponent* RoomRoot, const FVector& Scale, const FVector& RelativeLocation)
	{
		WallMeshComponent->SetupAttachment(RoomRoot);
		if (CubeMesh)
		{
			WallMeshComponent->SetStaticMesh(CubeMesh);
		}
		WallMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		WallMeshComponent->SetRelativeScale3D(Scale);
		WallMeshComponent->SetRelativeLocation(RelativeLocation);
	}
}

ARoomActor::ARoomActor()
{
	PrimaryActorTick.bCanEverTick = false;

	USceneComponent* RoomRoot = CreateDefaultSubobject<USceneComponent>(TEXT("RoomRoot"));
	RootComponent = RoomRoot;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMeshFinder(
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	UStaticMesh* CubeMesh = CubeMeshFinder.Succeeded() ? CubeMeshFinder.Object : nullptr;

	// Floor: top face sits at local Z=0, where target zones/enemies are placed
	// (RoomRoot's own origin). Collision is left at the mesh's engine default so the
	// floor still acts as a visible/physical ground plane.
	FloorMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FloorMeshComponent"));
	FloorMeshComponent->SetupAttachment(RoomRoot);
	if (CubeMesh)
	{
		FloorMeshComponent->SetStaticMesh(CubeMesh);
	}
	FloorMeshComponent->SetRelativeScale3D(FVector(
		RoomFloorExtent.X * 2.f / 100.f, RoomFloorExtent.Y * 2.f / 100.f, RoomFloorThickness / 100.f));
	FloorMeshComponent->SetRelativeLocation(FVector(0.f, 0.f, -RoomFloorThickness * 0.5f));

	WallNorthMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WallNorthMeshComponent"));
	SetupWallMeshComponent(WallNorthMeshComponent, CubeMesh, RoomRoot,
		FVector(RoomFloorExtent.X * 2.f / 100.f, RoomWallThickness / 100.f, RoomWallHeight / 100.f),
		FVector(0.f, RoomFloorExtent.Y, RoomWallHeight * 0.5f));

	WallSouthMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WallSouthMeshComponent"));
	SetupWallMeshComponent(WallSouthMeshComponent, CubeMesh, RoomRoot,
		FVector(RoomFloorExtent.X * 2.f / 100.f, RoomWallThickness / 100.f, RoomWallHeight / 100.f),
		FVector(0.f, -RoomFloorExtent.Y, RoomWallHeight * 0.5f));

	WallEastMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WallEastMeshComponent"));
	SetupWallMeshComponent(WallEastMeshComponent, CubeMesh, RoomRoot,
		FVector(RoomWallThickness / 100.f, RoomFloorExtent.Y * 2.f / 100.f, RoomWallHeight / 100.f),
		FVector(RoomFloorExtent.X, 0.f, RoomWallHeight * 0.5f));

	WallWestMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WallWestMeshComponent"));
	SetupWallMeshComponent(WallWestMeshComponent, CubeMesh, RoomRoot,
		FVector(RoomWallThickness / 100.f, RoomFloorExtent.Y * 2.f / 100.f, RoomWallHeight / 100.f),
		FVector(-RoomFloorExtent.X, 0.f, RoomWallHeight * 0.5f));
}

AActor* ARoomActor::AddTargetZone(EEnemyType EnemyType, TSubclassOf<AActor> MarkerClass)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	TSubclassOf<AActor> ClassToSpawn = MarkerClass ? MarkerClass : TSubclassOf<AActor>(APlaceholderTargetZoneActor::StaticClass());
	AActor* MarkerActor = World->SpawnActor<AActor>(ClassToSpawn);
	if (!MarkerActor)
	{
		return nullptr;
	}

	// SnapToTargetNotIncludingScale, not KeepWorldTransform - the marker spawns at the
	// world origin (no FTransform passed to SpawnActor above), so it must snap to the
	// room's origin on attach or it stays visually disconnected from the room for any
	// room not itself placed at the level origin. A designer can freely reposition the
	// marker afterward; this only fixes its starting point.
	MarkerActor->AttachToActor(this, FAttachmentTransformRules::SnapToTargetNotIncludingScale);

	FRoomTargetZone TargetZone;
	TargetZone.EnemyType = EnemyType;
	TargetZone.MarkerActor = MarkerActor;
	TargetZones.Add(TargetZone);

	return MarkerActor;
}

void ARoomActor::BeginPlay()
{
	Super::BeginPlay();
	for (const TObjectPtr<AEnemyBase>& Enemy : OwnedEnemies)
	{
		BindOwnedEnemyDelegate(Enemy);
	}
}

bool ARoomActor::IsRoomCleared() const
{
	for (const TObjectPtr<AEnemyBase>& Enemy : OwnedEnemies)
	{
		if (IsValid(Enemy) && Enemy->GetEnemyState() != EEnemyState::Banked)
		{
			return false;
		}
	}
	return true;
}

void ARoomActor::BindOwnedEnemyDelegate(AEnemyBase* Enemy)
{
	if (IsValid(Enemy))
	{
		Enemy->OnEnemyBanked.AddUniqueDynamic(this, &ARoomActor::HandleOwnedEnemyBanked);
	}
}

void ARoomActor::AddOwnedEnemy(AEnemyBase* Enemy)
{
	if (!IsValid(Enemy) || OwnedEnemies.Contains(Enemy))
	{
		return;
	}
	OwnedEnemies.Add(Enemy);
	BindOwnedEnemyDelegate(Enemy);
	OnRoomClearedStateChanged.Broadcast();
}

void ARoomActor::HandleOwnedEnemyBanked()
{
	OnRoomClearedStateChanged.Broadcast();
}
