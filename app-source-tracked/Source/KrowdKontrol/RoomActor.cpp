#include "RoomActor.h"
#include "PlaceholderTargetZoneActor.h"
#include "TargetZone.h"
#include "EnemyBase.h"
#include "AbilityData.h"
#include "OnScreenPromptWidget.h"
#include "KrowdKontrolPlayerController.h"
#include "DoorConnectorActor.h"
#include "Engine/World.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "EngineUtils.h"
#include "CoreGlobals.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Pawn.h"

namespace
{
	// Shared by all 4 wall mesh components below - the engine's
	// /Engine/BasicShapes/Cube.Cube is a 100x100x100uu cube, so scale = desired
	// size-in-cm / 100. Walls have collision disabled at construction time: real
	// per-door wall-side blocking collision is applied later, in
	// ARoomActor::SealRoomPerimeter() (issue #243), once every placed door in the
	// World is discoverable.
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

	enum class ERoomWallSide : uint8 { North, South, East, West };

	struct FWallGapSpan
	{
		float Start;
		float End;
	};

	// Builds 0..N+1 invisible blocking UBoxComponents covering Side's full tangent
	// span EXCEPT the given gaps (sorted here, caller order irrelevant; can build zero
	// if a gap consumes the entire span - see AddSegmentIfSolid's KINDA_SMALL_NUMBER
	// guard below) - replaces the
	// old per-door BuildWallGapFlanks, which built each door's flank pair independent
	// of every other door on the same side and could seal one door's gap inside
	// another door's "solid" flank (issue #243, PR #305 pass-1 rejection). Overlapping
	// gaps (e.g. two doors placed closer together than their combined half-widths) are
	// merged into one wider open span rather than left to seal either doorway - an
	// authoring conflict should fail open, not shut.
	void BuildWallSideFlanks(ARoomActor* Room, ERoomWallSide Side, TArray<FWallGapSpan> Gaps,
		TArray<TObjectPtr<UBoxComponent>>& OutFlanks)
	{
		Gaps.Sort([](const FWallGapSpan& A, const FWallGapSpan& B) { return A.Start < B.Start; });

		TArray<FWallGapSpan> MergedGaps;
		for (const FWallGapSpan& Gap : Gaps)
		{
			if (MergedGaps.Num() > 0 && Gap.Start <= MergedGaps.Last().End)
			{
				MergedGaps.Last().End = FMath::Max(MergedGaps.Last().End, Gap.End);
			}
			else
			{
				MergedGaps.Add(Gap);
			}
		}

		const bool bEastWest = (Side == ERoomWallSide::East || Side == ERoomWallSide::West);
		const float WallSpanHalfExtent = bEastWest ? Room->RoomFloorExtent.Y : Room->RoomFloorExtent.X;
		const float FixedAxisPosition = bEastWest
			? (Side == ERoomWallSide::East ? Room->RoomFloorExtent.X : -Room->RoomFloorExtent.X)
			: (Side == ERoomWallSide::North ? Room->RoomFloorExtent.Y : -Room->RoomFloorExtent.Y);

		auto AddSegmentIfSolid = [&](float SegmentStart, float SegmentEnd)
		{
			const float Length = SegmentEnd - SegmentStart;
			if (Length <= KINDA_SMALL_NUMBER)
			{
				return;
			}
			const float Center = (SegmentStart + SegmentEnd) * 0.5f;

			UBoxComponent* Flank = NewObject<UBoxComponent>(Room);
			Flank->SetupAttachment(Room->GetRootComponent());
			Flank->RegisterComponent();

			const FVector RelativeLocation = bEastWest
				? FVector(FixedAxisPosition, Center, Room->RoomWallHeight * 0.5f)
				: FVector(Center, FixedAxisPosition, Room->RoomWallHeight * 0.5f);
			const FVector BoxExtent = bEastWest
				? FVector(Room->RoomWallThickness * 0.5f, Length * 0.5f, Room->RoomWallHeight * 0.5f)
				: FVector(Length * 0.5f, Room->RoomWallThickness * 0.5f, Room->RoomWallHeight * 0.5f);

			Flank->SetRelativeLocation(RelativeLocation);
			Flank->SetBoxExtent(BoxExtent);
			ARoomActor::ConfigureWorldDynamicBlockingCollision(Flank);

			OutFlanks.Add(Flank);
		};

		float PrevEdge = -WallSpanHalfExtent;
		for (const FWallGapSpan& Gap : MergedGaps)
		{
			AddSegmentIfSolid(PrevEdge, Gap.Start);
			PrevEdge = Gap.End;
		}
		AddSegmentIfSolid(PrevEdge, WallSpanHalfExtent);
	}
}

float ARoomActor::ComputeAxisExitDistance(const FVector2D& HalfExtent, const FVector2D& Direction2D)
{
	const float ExitX = FMath::IsNearlyZero(Direction2D.X) ? TNumericLimits<float>::Max() : HalfExtent.X / FMath::Abs(Direction2D.X);
	const float ExitY = FMath::IsNearlyZero(Direction2D.Y) ? TNumericLimits<float>::Max() : HalfExtent.Y / FMath::Abs(Direction2D.Y);
	return FMath::Min(ExitX, ExitY);
}

// Mirrors ADoorConnectorActor::GateBlockingComponent's collision setup exactly
// (see that component's construction in ADoorConnectorActor's constructor) - the real player pawn presents ECC_WorldDynamic/
// BlockAllDynamic (issue #218's "attempt 3" root-cause finding), not ECC_Pawn or
// ECC_WorldStatic, so any new blocking volume must reuse this same response channel or
// it silently fails to stop the player exactly like #218's first two attempts did.
//
// The component's own CollisionObjectType is set to ECC_WorldStatic (issue #243
// Finding 3) - these are static level geometry (walls/flanks, corridor guard rails,
// the gate), never itself an actor's moving root, so WorldStatic is also the
// semantically correct channel. This matters beyond semantics: AEnemyBase::BeginPlay
// narrows every enemy's own root response to ECC_WorldDynamic down to Overlap (issue
// #211, so a regular enemy can physically enter an OverlapAllDynamic-profiled
// ATargetZone instead of being blocked by it) but leaves its response to
// ECC_WorldStatic at the unmodified Block default. A blocking volume left at
// WorldDynamic (the default UPrimitiveComponent object type, unset by the response
// calls above) is therefore a channel a fleeing enemy's own sweep silently classifies
// as an Overlap - never a blocking hit, regardless of bSweep - and passes straight
// through, confirmed empirically against this exact headless Automation harness.
// Declaring these volumes WorldStatic instead sidesteps that narrowing entirely and
// still blocks the player exactly as before (its own response was never narrowed).
void ARoomActor::ConfigureWorldDynamicBlockingCollision(UPrimitiveComponent* Component)
{
	Component->SetCollisionObjectType(ECC_WorldStatic);
	Component->SetCollisionResponseToAllChannels(ECR_Ignore);
	Component->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	Component->SetGenerateOverlapEvents(false);
	Component->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
}

ARoomActor::ARoomActor()
{
	// Ticks every frame to detect first entry into this room and advance its
	// activation countdown. Originally throttled to a 0.25s-interval poll
	// (mirroring ADoorConnectorActor's door-crossing idiom) - reverted, issue
	// #290 pass-1 E2E finding: that throttle left a window, up to 0.25s wide,
	// after the player physically entered the room but before this Tick() had
	// run for the first time. During that window AEnemyBase's own per-frame
	// Tick()/TickCheckDetection loop already saw the player resolve nearest to
	// this room via IsPlayerInOwningRoom(), while IsActivationPending() was
	// still false (bCountdownActive not yet set) - so the Idle->Alert gate
	// this issue exists to hold shut let enemies through immediately, live and
	// reproducibly, well before the on-screen countdown ever showed anything
	// but "3". No event exists for "the player entered this room", so this is
	// still a per-tick poll, just no longer throttled - it now runs at the
	// same per-frame rate as the detection loop it has to stay ahead of.
	PrimaryActorTick.bCanEverTick = true;

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

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	TArray<ARoomActor*> AllRooms;
	for (TActorIterator<ARoomActor> It(World); It; ++It)
	{
		AllRooms.Add(*It);
	}

	for (TActorIterator<AEnemyBase> It(World); It; ++It)
	{
		AEnemyBase* Enemy = *It;
		if (FindNearestRoom(Enemy, AllRooms) == this)
		{
			AddOwnedEnemy(Enemy);
		}
	}

	SealRoomPerimeter();
}

void ARoomActor::SealRoomPerimeter()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (TObjectPtr<UBoxComponent>& Flank : WallGapFlankComponents)
	{
		if (Flank)
		{
			Flank->DestroyComponent();
		}
	}
	WallGapFlankComponents.Reset();

	UStaticMeshComponent* WallBySide[4] = {
		WallNorthMeshComponent, WallSouthMeshComponent, WallEastMeshComponent, WallWestMeshComponent };
	TArray<FWallGapSpan> GapsBySide[4];

	for (TActorIterator<ADoorConnectorActor> It(World); It; ++It)
	{
		ADoorConnectorActor* Door = *It;
		if (!Door->ConnectsValidRooms() || (Door->RoomA != this && Door->RoomB != this))
		{
			continue;
		}
		ARoomActor* OtherRoom = (Door->RoomA == this) ? ToRawPtr(Door->RoomB) : ToRawPtr(Door->RoomA);

		const FVector Delta = OtherRoom->GetActorLocation() - GetActorLocation();
		const bool bEastWest = FMath::Abs(Delta.X) >= FMath::Abs(Delta.Y);
		const ERoomWallSide Side = bEastWest
			? (Delta.X >= 0.f ? ERoomWallSide::East : ERoomWallSide::West)
			: (Delta.Y >= 0.f ? ERoomWallSide::North : ERoomWallSide::South);

		// Where the line from this room's origin to OtherRoom's origin actually
		// crosses this room's wall plane - not the room-centres midpoint, which is
		// only correct when the wall sits exactly halfway between the two centres
		// (issue #243, PR #305 code-review Finding 2).
		const float FixedAxisPosition = bEastWest
			? (Side == ERoomWallSide::East ? RoomFloorExtent.X : -RoomFloorExtent.X)
			: (Side == ERoomWallSide::North ? RoomFloorExtent.Y : -RoomFloorExtent.Y);
		const float WallNormalDelta = bEastWest ? Delta.X : Delta.Y;
		if (FMath::IsNearlyZero(WallNormalDelta))
		{
			// Exactly-coincident room origins (issue #219's known co-located-rooms
			// authoring failure mode) would otherwise divide by zero here and poison
			// GapCenterOffset with NaN, which AddSegmentIfSolid's KINDA_SMALL_NUMBER
			// guard can't catch (NaN comparisons are always false, so a NaN segment
			// reads as "solid"). Fail open like this function's other degenerate cases
			// instead: skip this door's gap rather than construct a NaN-poisoned box.
			continue;
		}
		const float CrossingParam = FixedAxisPosition / WallNormalDelta;
		const float GapCenterOffset = CrossingParam * (bEastWest ? Delta.Y : Delta.X);
		const float GapHalfWidth = Door->ConnectorFloorWidth * 0.5f;

		GapsBySide[static_cast<uint8>(Side)].Add(FWallGapSpan{ GapCenterOffset - GapHalfWidth, GapCenterOffset + GapHalfWidth });
	}

	for (uint8 SideIndex = 0; SideIndex < 4; ++SideIndex)
	{
		if (!WallBySide[SideIndex])
		{
			continue;
		}
		if (GapsBySide[SideIndex].Num() > 0)
		{
			// The full-span wall mesh stays visual-only on a gapped side - the solid
			// segments built below (one per gap-free stretch) carry the real blocking
			// collision instead, so each doorway's width stays walkable.
			WallBySide[SideIndex]->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			BuildWallSideFlanks(this, static_cast<ERoomWallSide>(SideIndex), GapsBySide[SideIndex], WallGapFlankComponents);
		}
		else
		{
			ConfigureWorldDynamicBlockingCollision(WallBySide[SideIndex]);
		}
	}
}

void ARoomActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bRoomActivated)
	{
		return;
	}
	// else-if, not two sequential ifs: the tick that starts the countdown
	// (CheckFirstEntry -> StartCountdown, setting bCountdownActive) must not
	// also advance it in that same frame - that would silently consume one
	// tick's worth of DeltaSeconds off the enforced hold before the player-
	// facing countdown display ever reflected it (code-review follow-up,
	// issue #290 pass-1).
	if (bCountdownActive)
	{
		AdvanceCountdown(DeltaSeconds);
	}
	else if (const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0))
	{
		CheckFirstEntry(PlayerPawn->GetActorLocation());
	}
}

ARoomActor* ARoomActor::FindNearestRoom(const AActor* Actor, const TArray<ARoomActor*>& Rooms)
{
	return FindNearestRoom(Actor->GetActorLocation(), Rooms);
}

ARoomActor* ARoomActor::FindNearestRoom(const FVector& Location, const TArray<ARoomActor*>& Rooms)
{
	ARoomActor* Nearest = nullptr;
	float NearestDistSq = TNumericLimits<float>::Max();
	for (ARoomActor* Room : Rooms)
	{
		const float DistSq = FVector::DistSquared(Location, Room->GetActorLocation());
		if (DistSq < NearestDistSq)
		{
			NearestDistSq = DistSq;
			Nearest = Room;
		}
	}
	return Nearest;
}

const TArray<ARoomActor*>& ARoomActor::GetCachedRoomList(UWorld* World)
{
	static TWeakObjectPtr<UWorld> CachedWorld;
	static uint64 CachedFrameNumber = TNumericLimits<uint64>::Max();
	static TArray<ARoomActor*> CachedRooms;

	if (CachedWorld.Get() != World || CachedFrameNumber != GFrameCounter)
	{
		CachedRooms.Reset();
		for (TActorIterator<ARoomActor> It(World); It; ++It)
		{
			CachedRooms.Add(*It);
		}
		CachedWorld = World;
		CachedFrameNumber = GFrameCounter;
	}
	return CachedRooms;
}

void ARoomActor::ApplyChainColourToMarker(AActor* MarkerActor, const ATargetZone* Zone)
{
	if (!Zone || Zone->bAcceptAnyEnemyType)
	{
		return;
	}
	if (APlaceholderTargetZoneActor* Placeholder = Cast<APlaceholderTargetZoneActor>(MarkerActor))
	{
		Placeholder->ApplyChainColour(AbilityData::GetChainColourForEnemyType(Zone->ZoneEnemyType));
	}
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
			ApplyChainColourToMarker(Zone.MarkerActor, ExistingZone);
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
		// Colour stays as metadata (visuals/bonus); acceptance is type-keyed
		// (operator ruling 2026-08-22): this pen takes its own enemy type only.
		BankingZone->ZoneColourTag = ResolvedColourTag;
		BankingZone->bAcceptAnyEnemyType = false;
		BankingZone->ZoneEnemyType = Zone.EnemyType;
		BankingZone->OnActorBanked.AddUniqueDynamic(this, &ARoomActor::HandleZoneActorBanked);
		ApplyChainColourToMarker(Zone.MarkerActor, BankingZone);
	}
}

void ARoomActor::HandleZoneActorBanked(AActor* BankedActor)
{
	if (AEnemyBase* Enemy = Cast<AEnemyBase>(BankedActor))
	{
		Enemy->TransitionToBanked();
	}
}

int32 ARoomActor::GetRemainingEnemyCount() const
{
	int32 RemainingCount = 0;
	for (const TObjectPtr<AEnemyBase>& Enemy : OwnedEnemies)
	{
		// IsActorBeingDestroyed() matters here, not just IsValid(): AActor::OnDestroyed
		// broadcasts synchronously from inside UWorld::DestroyActor() *before* the actor
		// is marked garbage, so a HandleOwnedEnemyDestroyed()-triggered re-check would
		// otherwise still see this un-banked enemy as IsValid() and blocking, and the
		// door would never actually re-open.
		if (IsValid(Enemy) && !Enemy->IsActorBeingDestroyed() && Enemy->GetEnemyState() != EEnemyState::Banked)
		{
			++RemainingCount;
		}
	}
	return RemainingCount;
}

bool ARoomActor::IsRoomCleared() const
{
	return GetRemainingEnemyCount() == 0;
}

void ARoomActor::BindOwnedEnemyDelegate(AEnemyBase* Enemy)
{
	if (IsValid(Enemy))
	{
		Enemy->OnEnemyBanked.AddUniqueDynamic(this, &ARoomActor::HandleOwnedEnemyBanked);
		Enemy->OnDestroyed.AddUniqueDynamic(this, &ARoomActor::HandleOwnedEnemyDestroyed);
	}
}

void ARoomActor::AddOwnedEnemy(AEnemyBase* Enemy)
{
	if (!IsValid(Enemy))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("ARoomActor: AddOwnedEnemy() called on '%s' with an invalid Enemy - ignoring."),
			*GetNameSafe(this));
		return;
	}
	if (OwnedEnemies.Contains(Enemy))
	{
		return;
	}
	OwnedEnemies.Add(Enemy);
	Enemy->SetOwningRoom(this);
	// This room's Tick() (CheckFirstEntry -> StartCountdown) must run before the
	// enemy's Tick() -> TickCheckDetection() within any given frame, or an enemy
	// ticking first on the exact first-entry frame sees IsActivationPending() ==
	// false before the countdown has started and can alert one frame early
	// (pass-2 behavioral-validation finding on issue #245).
	Enemy->AddTickPrerequisiteActor(this);
	BindOwnedEnemyDelegate(Enemy);
	OnRoomClearedStateChanged.Broadcast();
}

void ARoomActor::HandleOwnedEnemyBanked()
{
	OnRoomClearedStateChanged.Broadcast();
}

void ARoomActor::HandleOwnedEnemyDestroyed(AActor* DestroyedActor)
{
	OnRoomClearedStateChanged.Broadcast();
}

void ARoomActor::CheckFirstEntry(const FVector& PlayerLocation)
{
	if (bRoomActivated || bCountdownActive || IsRoomCleared())
	{
		return;
	}
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	if (FindNearestRoom(PlayerLocation, GetCachedRoomList(World)) != this)
	{
		return;
	}
	StartCountdown();
}

void ARoomActor::StartCountdown()
{
	bCountdownActive = true;
	RemainingCountdownSeconds = RoomActivationCountdownSeconds;
	LastDisplayedCount = -1;
	UpdateCountdownPrompt();
}

void ARoomActor::AdvanceCountdown(float DeltaSeconds)
{
	if (!bCountdownActive)
	{
		return;
	}
	RemainingCountdownSeconds = FMath::Max(0.0f, RemainingCountdownSeconds - DeltaSeconds);
	if (RemainingCountdownSeconds <= 0.0f)
	{
		bCountdownActive = false;
		ActivateRoom();
		return;
	}
	UpdateCountdownPrompt();
}

void ARoomActor::ActivateRoom()
{
	bRoomActivated = true;
}

void ARoomActor::UpdateCountdownPrompt()
{
	const int32 DisplayCount = FMath::Max(0, FMath::CeilToInt(RemainingCountdownSeconds));
	if (DisplayCount <= 0 || DisplayCount == LastDisplayedCount)
	{
		return;
	}
	LastDisplayedCount = DisplayCount;
	if (UOnScreenPromptWidget* Widget = ResolvePromptWidget())
	{
		// 1.0s per digit, well under UOnScreenPromptWidget::MaxPromptDurationSeconds
		// (2.0f) - PRD 09 REQ-4's hard cap stays untouched.
		Widget->ShowPrompt(FText::AsNumber(DisplayCount), 1.0f);
	}
}

UOnScreenPromptWidget* ARoomActor::ResolvePromptWidget()
{
	if (CachedPromptWidget)
	{
		return CachedPromptWidget;
	}
	if (UWorld* World = GetWorld())
	{
		if (AKrowdKontrolPlayerController* Controller = Cast<AKrowdKontrolPlayerController>(World->GetFirstPlayerController()))
		{
			CachedPromptWidget = Controller->OnScreenPromptWidgetInstance;
		}
	}
	if (!CachedPromptWidget && !bHasWarnedMissingPromptWidget)
	{
		bHasWarnedMissingPromptWidget = true;
		UE_LOG(LogTemp, Warning,
			TEXT("ARoomActor: no OnScreenPromptWidget available on '%s' - the room-entry countdown cannot be shown."),
			*GetNameSafe(this));
	}
	return CachedPromptWidget;
}
