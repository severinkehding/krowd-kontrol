#include "RoomActor.h"
#include "PlaceholderTargetZoneActor.h"
#include "TargetZone.h"
#include "EnemyBase.h"
#include "AbilityData.h"
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
	EnsureBankingZonesWired();
}

void ARoomActor::EnsureBankingZonesWired()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// AbilityData::GetAll() is the single source of truth for which ability counters
	// which EEnemyType (FAbilityData::CounteredEnemyType, AbilityData.h:47-51) - build
	// a lookup once rather than re-deriving it per TargetZones entry below.
	TArray<FAbilityData> AllAbilities = AbilityData::GetAll();

	for (const FRoomTargetZone& Zone : TargetZones)
	{
		if (!Zone.MarkerActor)
		{
			continue;
		}

		// Idempotency check: skip *spawning* a marker that already has an attached
		// ATargetZone, so repeated calls (e.g. from a test that also drives BeginPlay)
		// never double-spawn - same "safe to call more than once" contract
		// EnsureBeaconHierarchy() establishes for the sibling self-heal pattern this
		// mirrors. This must not also skip *binding*: a zone can be attached to a
		// marker through a path other than this function (e.g. hand-placed by a level
		// designer), in which case its OnActorBanked delegate was never bound - so any
		// already-attached zone still gets AddUniqueDynamic'd below before the loop
		// moves on, using the same idempotent-bind idiom TargetZone.cpp:25 already
		// establishes for this codebase.
		TArray<AActor*> AttachedActors;
		Zone.MarkerActor->GetAttachedActors(AttachedActors);
		ATargetZone* ExistingZone = nullptr;
		for (AActor* Attached : AttachedActors)
		{
			if (ATargetZone* AttachedZone = Cast<ATargetZone>(Attached))
			{
				ExistingZone = AttachedZone;
				break;
			}
		}
		if (ExistingZone)
		{
			ExistingZone->OnActorBanked.AddUniqueDynamic(this, &ARoomActor::HandleZoneActorBanked);
			continue;
		}

		// Resolve this marker's EnemyType to the one non-Stun ability that counters
		// it (Sleep<->SN_1PR, Root<->TR_UPR, Fear<->B0_0MR, Snare<->RU_NNR - see
		// AbilityData.cpp), then use that ability's ColourTag. A marker whose
		// EnemyType has no countering ability entry (should not happen given the 4
		// locked types each have exactly one counter) is left NAME_None, matching
		// ATargetZone::ZoneColourTag's own safe default.
		FName ResolvedColourTag = NAME_None;
		for (const FAbilityData& AbilityEntry : AllAbilities)
		{
			if (!AbilityEntry.bIsColourNeutral && AbilityEntry.CounteredEnemyType == Zone.EnemyType)
			{
				ResolvedColourTag = AbilityEntry.ColourTag;
				break;
			}
		}

		ATargetZone* BankingZone = World->SpawnActor<ATargetZone>(
			Zone.MarkerActor->GetActorLocation(), Zone.MarkerActor->GetActorRotation());
		if (!BankingZone)
		{
			continue;
		}

		// Attach the banking zone to the marker (issue #211's literal ask) rather than
		// to this room - KeepWorldTransform since SpawnActor above already placed it
		// at the marker's world location/rotation.
		BankingZone->AttachToActor(Zone.MarkerActor, FAttachmentTransformRules::KeepWorldTransform);
		BankingZone->ZoneColourTag = ResolvedColourTag;
		BankingZone->OnActorBanked.AddUniqueDynamic(this, &ARoomActor::HandleZoneActorBanked);
	}
}

void ARoomActor::HandleZoneActorBanked(AActor* BankedActor)
{
	if (AEnemyBase* Enemy = Cast<AEnemyBase>(BankedActor))
	{
		Enemy->TransitionToBanked();
	}
}
