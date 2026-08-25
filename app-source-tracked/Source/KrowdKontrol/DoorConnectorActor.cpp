#include "DoorConnectorActor.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "RoomActor.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	void ConfigureCorridorGuardRail(UBoxComponent* GuardRail, USceneComponent* Root)
	{
		GuardRail->SetupAttachment(Root);
		GuardRail->SetCollisionResponseToAllChannels(ECR_Ignore);
		GuardRail->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
		GuardRail->SetGenerateOverlapEvents(false);
		// Starts disabled/zero-sized like the door's other connector-span geometry -
		// RecomputeConnectorGeometry() positions and enables it once RoomA/RoomB resolve to
		// a valid span. Unlike GateBlockingComponent, never re-toggled after that - the
		// corridor sides are always solid, not gated.
		GuardRail->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

ADoorConnectorActor::ADoorConnectorActor()
{
	// Ticks (cheap, 4Hz) so the player-beyond-door term of RefreshGateState() tracks
	// pawn movement - the room-cleared term stays event-driven via
	// OnRoomClearedStateChanged, but no event exists for "the player crossed the
	// door", and polling at 0.25s is imperceptible at door scale.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.25f;

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
	// comment below for why that channel is WorldDynamic, not WorldStatic or Pawn).
	GateBlockingComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	GateBlockingComponent->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	GateBlockingComponent->SetGenerateOverlapEvents(false);

	CorridorGuardRailAComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("CorridorGuardRailAComponent"));
	ConfigureCorridorGuardRail(CorridorGuardRailAComponent, DoorConnectorRoot);

	CorridorGuardRailBComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("CorridorGuardRailBComponent"));
	ConfigureCorridorGuardRail(CorridorGuardRailBComponent, DoorConnectorRoot);
}

void ADoorConnectorActor::HideConnectorVisuals()
{
	ConnectorFloorMeshComponent->SetVisibility(false);
	DoorMarkerMeshComponent->SetVisibility(false);
	DoorMarkerLightComponent->SetVisibility(false);
	CorridorGuardRailAComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CorridorGuardRailBComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	// Route through RefreshGateState() rather than hardcoding NoCollision, so a
	// GatingRoom that's still un-cleared stays blocked even while the connector's visuals
	// are hidden (e.g. RecomputeConnectorGeometry() called again post-placement with a
	// degenerate RoomA/RoomB span - see GatingRoom's header comment).
	RefreshGateState();
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

	// The rails must span only the corridor gap BETWEEN the two room perimeters, never
	// the full centre-to-centre distance: a Length*0.5 half-extent (2026-08-26 operator
	// playtest) runs each rail from room centre to room centre, carving an impassable
	// 320cm-wide channel through both room interiors that walls the player off from
	// their own room. Clamp each end to the room's floor edge along the connector axis
	// (support-function projection of the axis-aligned floor slab - rooms in both
	// hand-authored levels are unrotated, per ARoomActor::RoomFloorExtent's spacing
	// comment).
	const FVector Direction = Delta / Length;
	auto ProjectedFloorHalfExtent = [&Direction](const ARoomActor* Room)
	{
		return FMath::Abs(Direction.X) * Room->RoomFloorExtent.X +
			FMath::Abs(Direction.Y) * Room->RoomFloorExtent.Y;
	};
	const float GapStartDistance = ProjectedFloorHalfExtent(RoomA);
	const float GapEndDistance = Length - ProjectedFloorHalfExtent(RoomB);
	const float GuardRailHalfLength = (GapEndDistance - GapStartDistance) * 0.5f;

	const FVector LateralDirection = FRotationMatrix(Delta.Rotation()).GetUnitAxis(EAxis::Y);
	const float GuardRailOffset = ConnectorFloorWidth * 0.5f + CorridorGuardRailThickness * 0.5f;
	const FVector GapMidpoint = LocationA + Direction * (GapStartDistance + GapEndDistance) * 0.5f;
	const FVector GuardRailExtent(GuardRailHalfLength, CorridorGuardRailThickness * 0.5f, CorridorGuardRailHeight * 0.5f);

	auto PlaceGuardRail = [&](UBoxComponent* GuardRail, const FVector& Location)
	{
		GuardRail->SetWorldLocation(Location);
		GuardRail->SetWorldRotation(Delta.Rotation());
		GuardRail->SetBoxExtent(GuardRailExtent);
		// Overlapping/adjacent room floors leave no corridor gap - degenerate rails
		// would sit inside a room, so disable them entirely rather than block there.
		GuardRail->SetCollisionEnabled(GuardRailHalfLength > 0.f
			? ECollisionEnabled::QueryOnly
			: ECollisionEnabled::NoCollision);
	};
	PlaceGuardRail(CorridorGuardRailAComponent, GapMidpoint + LateralDirection * GuardRailOffset);
	PlaceGuardRail(CorridorGuardRailBComponent, GapMidpoint - LateralDirection * GuardRailOffset);
}

void ADoorConnectorActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RecomputeConnectorGeometry();
}

void ADoorConnectorActor::BeginPlay()
{
	Super::BeginPlay();

	if (!GatingRoom && ConnectsValidRooms())
	{
		GatingRoom = RoomA->GetActorLocation().X <= RoomB->GetActorLocation().X ? RoomA : RoomB;
	}

	// After GatingRoom resolution, not before - PR #229 code review: geometry
	// recompute must never observe a half-initialized gating state.
	RecomputeConnectorGeometry();

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

void ADoorConnectorActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	RefreshGateState();
}

void ADoorConnectorActor::RefreshGateState()
{
	// Reconciled AC3/AC4 rule (operator decision, 2026-08-22, PR #229 escalation -
	// replaces the bGateEverOpened permanent latch, which satisfied AC3 by
	// inverting AC4): the door gates LIVE on its GatingRoom's cleared state, so a
	// wave-spawned enemy re-gates its room (AC4) - but only while the player is
	// still on the gating-room side. Once the player is closer to the far room,
	// the door is "behind them" and stays open (AC3): re-closing then would wall
	// the player away from a fight they already earned their way past. With no
	// player pawn (headless Automation worlds) the player term is false and the
	// rule reduces to the pure cleared-state gate the gating tests pin.
	const bool bRoomCleared = (GatingRoom == nullptr) || GatingRoom->IsRoomCleared();
	bool bPlayerBeyondDoor = false;
	if (GatingRoom && RoomA && RoomB)
	{
		const ARoomActor* FarRoom = (GatingRoom == RoomA) ? ToRawPtr(RoomB) : ToRawPtr(RoomA);
		if (const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0))
		{
			const FVector PlayerLocation = PlayerPawn->GetActorLocation();
			bPlayerBeyondDoor =
				FVector::DistSquared(PlayerLocation, FarRoom->GetActorLocation()) <
				FVector::DistSquared(PlayerLocation, GatingRoom->GetActorLocation());
		}
	}
	bIsGateOpen = bRoomCleared || bPlayerBeyondDoor;

	// Root-cause fix (issue #218, attempt 3): live PIE inspection of the real possessed
	// player pawn (AFlatCamera3DPrototypePawn::MeshComponent) shows it presents
	// objectType=ECC_WorldDynamic, collisionProfileName=BlockAllDynamic - not
	// ECC_WorldStatic as attempt 2 assumed from source inspection alone (attempt 1
	// blocked only ECC_Pawn, also a no-op against the real player). QueryOnly still
	// blocks WorldDynamic-channel sweeps (the constructor's collision-response setup
	// above), so this line only needs to toggle enabled/disabled, not the per-channel
	// responses.
	GateBlockingComponent->SetCollisionEnabled(
		bIsGateOpen ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryOnly);
}
