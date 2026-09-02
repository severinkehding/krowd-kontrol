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
#include "Materials/MaterialInterface.h"
#include "EnemyBase.h"
#include "EngineUtils.h"

namespace
{
	void ConfigureCorridorGuardRail(UBoxComponent* GuardRail, USceneComponent* Root)
	{
		GuardRail->SetupAttachment(Root);
		ARoomActor::ConfigureWorldDynamicBlockingCollision(GuardRail);
		// Starts disabled/zero-sized like the door's other connector-span geometry -
		// RecomputeConnectorGeometry() positions and enables it once RoomA/RoomB resolve to
		// a valid span, and HideConnectorVisuals() force-disables it again whenever they stop
		// resolving. Unlike GateBlockingComponent, never gated open/closed by room-clear
		// state - the corridor sides are always solid or fully absent, never conditionally
		// blocking. Overrides the shared helper's QueryOnly default back to NoCollision,
		// same as today's behavior.
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
	// Was 0.25s (gate polling only); the sliding leaves animate per-frame now and
	// RefreshGateState() stays cheap enough that the throttle bought nothing real.
	PrimaryActorTick.TickInterval = 0.f;

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
	// Operator art pass (2026-08-30): match the rooms' floor material when the
	// EnvKit content exists (see ARoomActor::FloorMaterial's rationale).
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ConnectorFloorMatFinder(
		TEXT("/Game/EnvKit/Materials/MIC_KitFloor.MIC_KitFloor"));
	if (ConnectorFloorMatFinder.Succeeded())
	{
		ConnectorFloorMeshComponent->SetMaterial(0, ConnectorFloorMatFinder.Object);
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
	// Unconfigured primitives default to BlockAllDynamic - the shared helper resets to
	// Ignore-all first so the gate only ever blocks the one channel it's meant to (see
	// RefreshGateState()'s comment below for why that channel is WorldDynamic, not
	// WorldStatic or Pawn). Overrides the helper's QueryOnly default back to
	// NoCollision, same as today's behavior.
	ARoomActor::ConfigureWorldDynamicBlockingCollision(GateBlockingComponent);
	GateBlockingComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	CorridorGuardRailAComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("CorridorGuardRailAComponent"));
	ConfigureCorridorGuardRail(CorridorGuardRailAComponent, DoorConnectorRoot);

	DoorPanelLeftComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorPanelLeftComponent"));
	DoorPanelLeftComponent->SetupAttachment(DoorConnectorRoot);
	DoorPanelRightComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorPanelRightComponent"));
	DoorPanelRightComponent->SetupAttachment(DoorConnectorRoot);
	// The packs' matched sliding pair - both pivots sit at the closed meeting
	// edge, so co-locating them at the door centre closes the pair naturally.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> DoorLeafLeftFinder(
		TEXT("/Game/Space_Station_2/Meshes/SM_door_002.SM_door_002"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> DoorLeafRightFinder(
		TEXT("/Game/Space_Station_2/Meshes/SM_door_006.SM_door_006"));
	for (UStaticMeshComponent* Panel : { DoorPanelLeftComponent.Get(), DoorPanelRightComponent.Get() })
	{
		Panel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Panel->SetCollisionProfileName(TEXT("NoCollision"));
		Panel->SetGenerateOverlapEvents(false);
		Panel->SetCanEverAffectNavigation(false);
	}
	if (DoorLeafLeftFinder.Succeeded())
	{
		DoorPanelLeftComponent->SetStaticMesh(DoorLeafLeftFinder.Object);
	}
	if (DoorLeafRightFinder.Succeeded())
	{
		DoorPanelRightComponent->SetStaticMesh(DoorLeafRightFinder.Object);
	}

	CorridorGuardRailBComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("CorridorGuardRailBComponent"));
	ConfigureCorridorGuardRail(CorridorGuardRailBComponent, DoorConnectorRoot);
}

void ADoorConnectorActor::HideConnectorVisuals()
{
	bDoorPlaneValid = false;
	if (DoorPanelLeftComponent)
	{
		DoorPanelLeftComponent->SetVisibility(false);
	}
	if (DoorPanelRightComponent)
	{
		DoorPanelRightComponent->SetVisibility(false);
	}
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

	// Door-plane frame for the sliding leaves: leaves stand across the corridor
	// (their width axis along the connector's lateral Y), meeting at the centre.
	DoorPlaneCenter = Midpoint;
	DoorLateralDirection = FRotationMatrix(Delta.Rotation()).GetUnitAxis(EAxis::Y);
	DoorPanelRotation = Delta.Rotation() + FRotator(0.f, 90.f, 0.f);
	bDoorPlaneValid = true;
	const FVector PanelScale(ConnectorFloorWidth * 0.5f / 97.6f, 1.f, 300.f / 217.8f);
	for (UStaticMeshComponent* Panel : { DoorPanelLeftComponent.Get(), DoorPanelRightComponent.Get() })
	{
		Panel->SetWorldRotation(DoorPanelRotation);
		Panel->SetWorldScale3D(PanelScale);
		Panel->SetVisibility(true);
	}
	TickDoorPanels(0.f);

	GateBlockingComponent->SetWorldLocation(Midpoint);
	GateBlockingComponent->SetWorldRotation(Delta.Rotation());
	GateBlockingComponent->SetBoxExtent(FVector(
		ConnectorFloorThickness, ConnectorFloorWidth * 0.5f, DoorMarkerHeight));

	// The rails must span only the corridor gap BETWEEN the two room perimeters, never
	// the full centre-to-centre distance: a Length*0.5 half-extent (2026-08-26 operator
	// playtest) runs each rail from room centre to room centre, carving an impassable
	// 320cm-wide channel through both room interiors that walls the player off from
	// their own room. Clamp each end to the room's floor edge along the connector axis
	// using the true ray-exit distance (ARoomActor::ComputeAxisExitDistance), not the
	// box's support function - the two only agree for axis-aligned Direction (issue
	// #243 code-review Finding 2b: the support function overshoots for any diagonal
	// room-to-room pair, leaving unfenced stretches at the door mouths).
	const FVector Direction = Delta / Length;
	// Direction is 3D-unit, so dropping Z leaves a sub-unit 2D vector for any
	// non-coplanar room pair - re-normalize so ComputeAxisExitDistance's exit-distance
	// math (which assumes a unit 2D direction) stays correct off-plane too.
	const FVector2D Direction2D = FVector2D(Direction.X, Direction.Y).GetSafeNormal();
	const float GapStartDistance = ARoomActor::ComputeAxisExitDistance(RoomA->RoomFloorExtent, Direction2D);
	const float GapEndDistance = Length - ARoomActor::ComputeAxisExitDistance(RoomB->RoomFloorExtent, Direction2D);
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
	TickDoorPanels(DeltaSeconds);
}

void ADoorConnectorActor::TickDoorPanels(float DeltaSeconds)
{
	if (!bDoorPlaneValid || !DoorPanelLeftComponent || !DoorPanelRightComponent)
	{
		return;
	}
	// Open only when the gate itself is passable AND someone is actually at the
	// door - the player pawn or any enemy (controlled trains and fleeing robots
	// cross doors without the player right next to them).
	bool bWantsOpen = false;
	if (bIsGateOpen && GetWorld())
	{
		const float RadiusSquared = DoorProximityRadius * DoorProximityRadius;
		if (const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0))
		{
			bWantsOpen = FVector::DistSquared(PlayerPawn->GetActorLocation(), DoorPlaneCenter) < RadiusSquared;
		}
		if (!bWantsOpen)
		{
			for (TActorIterator<AEnemyBase> It(GetWorld()); It; ++It)
			{
				if (FVector::DistSquared(It->GetActorLocation(), DoorPlaneCenter) < RadiusSquared)
				{
					bWantsOpen = true;
					break;
				}
			}
		}
	}
	DoorPanelSlide01 = FMath::FInterpConstantTo(DoorPanelSlide01, bWantsOpen ? 1.f : 0.f, DeltaSeconds, 2.2f);
	const FVector SlideOffset = DoorLateralDirection * DoorSlideDistance * DoorPanelSlide01;
	DoorPanelLeftComponent->SetWorldLocation(DoorPlaneCenter - SlideOffset);
	DoorPanelRightComponent->SetWorldLocation(DoorPlaneCenter + SlideOffset);
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
