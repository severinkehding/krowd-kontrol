#include "DoorConnectorActor.h"
#include "Components/SceneComponent.h"

ADoorConnectorActor::ADoorConnectorActor()
{
	PrimaryActorTick.bCanEverTick = false;

	USceneComponent* DoorConnectorRoot = CreateDefaultSubobject<USceneComponent>(TEXT("DoorConnectorRoot"));
	RootComponent = DoorConnectorRoot;
}
