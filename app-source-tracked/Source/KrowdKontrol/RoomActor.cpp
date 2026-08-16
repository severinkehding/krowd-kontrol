#include "RoomActor.h"
#include "PlaceholderTargetZoneActor.h"
#include "Engine/World.h"
#include "Components/SceneComponent.h"

ARoomActor::ARoomActor()
{
	PrimaryActorTick.bCanEverTick = false;

	USceneComponent* RoomRoot = CreateDefaultSubobject<USceneComponent>(TEXT("RoomRoot"));
	RootComponent = RoomRoot;
}

AActor* ARoomActor::AddTargetZone(EEnemyType EnemyType, TSubclassOf<AActor> MarkerClass)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	TSubclassOf<AActor> ClassToSpawn = MarkerClass ? MarkerClass : APlaceholderTargetZoneActor::StaticClass();
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
