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

	// KeepWorldTransform, not SnapToTargetNotIncludingScale - the marker spawns at the
	// world origin (no FTransform passed to SpawnActor above), so snapping to the
	// room's origin on attach would cause an unwanted jump for a marker a designer may
	// reposition after spawning.
	MarkerActor->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);

	FRoomTargetZone TargetZone;
	TargetZone.EnemyType = EnemyType;
	TargetZone.MarkerActor = MarkerActor;
	TargetZones.Add(TargetZone);

	return MarkerActor;
}
