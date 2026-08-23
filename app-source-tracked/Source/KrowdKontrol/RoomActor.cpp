#include "RoomActor.h"
#include "PlaceholderTargetZoneActor.h"
#include "TargetZone.h"
#include "EnemyBase.h"
#include "AbilityData.h"
#include "OnScreenPromptWidget.h"
#include "KrowdKontrolPlayerController.h"
#include "Engine/World.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
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
		// Colour stays as metadata (visuals/bonus); acceptance is type-keyed
		// (operator ruling 2026-08-22): this pen takes its own enemy type only.
		BankingZone->ZoneColourTag = ResolvedColourTag;
		BankingZone->bAcceptAnyEnemyType = false;
		BankingZone->ZoneEnemyType = Zone.EnemyType;
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
