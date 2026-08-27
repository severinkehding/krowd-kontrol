#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EnemyType.h"
#include "RoomActor.generated.h"

class APlaceholderTargetZoneActor;
class ATargetZone;
class UStaticMeshComponent;
class UBoxComponent;
class UPrimitiveComponent;
class AEnemyBase;
class ADoorConnectorActor;
class UOnScreenPromptWidget;
class AKrowdKontrolPlayerController;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRoomClearedStateChanged);

// One tagged target-zone marker child of an ARoomActor: which EEnemyType it serves,
// and the spawned marker actor itself (see APlaceholderTargetZoneActor - no real
// ATargetZone class exists yet, per RoomEnemyBudgetController.h's reserved
// OnActorBanked integration point).
USTRUCT(BlueprintType)
struct FRoomTargetZone
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Target Zone")
	EEnemyType EnemyType = EEnemyType::RU_NNR;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Target Zone")
	TObjectPtr<AActor> MarkerActor;
};

// Placeable, hand-authoring-era building block for a level's room topology (PRD 05
// REQ-1/REQ-2, issue #39): holds an arbitrary number of tagged target-zone marker
// children, added via AddTargetZone(). No ability or HUD logic. Since issue #218,
// also tracks which enemies it owns and whether they've all reached Banked, purely to
// drive door gating (see OwnedEnemies/IsRoomCleared) - no enemy AI decision-making
// lives here. Room-pool/connector-shuffler generation (PRD 05 REQ-4/REQ-5/REQ-6) is a
// separate future P1 issue, not built here.
UCLASS()
class KROWDKONTROL_API ARoomActor : public AActor
{
	GENERATED_BODY()

	// Grants the Automation Framework test direct access to CheckFirstEntry/
	// AdvanceCountdown/StartCountdown/ActivateRoom below (issue #245), so a headless
	// test can drive the first-entry countdown deterministically without a real
	// per-frame Tick() loop - same rationale AEnemyBase's own friend-class grants
	// document for TickCheckDetection.
	friend class FKrowdKontrolRoomActivationCountdownTest;

public:
	ARoomActor();

	// Spawns MarkerClass (or APlaceholderTargetZoneActor if unset), attaches it as a
	// child of this room, tags it with EnemyType, and records the pair. Returns the
	// spawned marker actor, or nullptr if spawning failed.
	UFUNCTION(BlueprintCallable, Category = "Room")
	AActor* AddTargetZone(EEnemyType EnemyType, TSubclassOf<AActor> MarkerClass = nullptr);

	const TArray<FRoomTargetZone>& GetTargetZones() const { return TargetZones; }

	// Self-heals a colour-tagged ATargetZone attached to each already-placed marker
	// in TargetZones that doesn't have one yet (issue #211) - the code-only path for
	// rooms placed/serialized before this class carried banking behaviour, mirroring
	// APlaceholderTargetZoneActor::EnsureBeaconHierarchy()'s "unconditionally re-check
	// and fix, safe to call more than once" shape. Called automatically from
	// BeginPlay(); exposed publicly (and made idempotent) so callers - including the
	// Automation Framework test - can trigger it deterministically without needing to
	// drive the engine's full actor BeginPlay lifecycle, same rationale
	// URoomEnemyBudgetController::InitializeRoom() documents for its own public
	// idempotent entry point.
	UFUNCTION(BlueprintCallable, Category = "Room")
	void EnsureBankingZonesWired();

	// Issue #243 / PRD Room Encounter Flow REQ-1: seals this room's 4-wall perimeter so
	// the only walkable connection to an adjacent room is through a door actually
	// connected to this room. A side with no connecting ADoorConnectorActor gets
	// blocking collision on its existing wall mesh; a side with one gets a
	// matching-width gap (flanked by two invisible blocking segments) so the doorway
	// itself stays open for ADoorConnectorActor's own GateBlockingComponent to gate.
	// Called from BeginPlay, on the same "every placed actor already exists in the
	// World by the time any BeginPlay fires" assumption AddOwnedEnemy's own
	// auto-discovery already relies on. Exposed publicly and idempotent (destroys and
	// rebuilds its own flank components each call) so a test using the
	// SpawnActor-after-play pattern - where BeginPlay dispatches immediately per-actor,
	// before a later-spawned door exists - can call it again once the connecting
	// door(s) are actually in the World, mirroring EnsureBankingZonesWired()'s own
	// "safe to call more than once" contract.
	UFUNCTION(BlueprintCallable, Category = "Room")
	void SealRoomPerimeter();

	// Enemies this room must clear before its gated door(s) open. Auto-discovered in
	// BeginPlay via nearest-room-by-distance over every AEnemyBase in the world (issue
	// #218) - the same rule KrowdKontrolLevelTestUtils::FindNearestRoom already uses and
	// the real levels' structural regression tests already trust, so real placed enemies
	// wire up correctly with zero .umap authoring. AddOwnedEnemy() extends this at
	// runtime for enemies added later (e.g. a future wave spawn).
	const TArray<TObjectPtr<AEnemyBase>>& GetOwnedEnemies() const { return OwnedEnemies; }

	// Count of OwnedEnemies still blocking IsRoomCleared() - same predicate (valid, not
	// being destroyed, not yet Banked). Exposed separately so callers that need "how many
	// are left" (not just "is it done") - e.g. the quest tracker's room-state HUD line,
	// issue #248 - don't have to re-walk OwnedEnemies themselves. IsRoomCleared() is
	// implemented in terms of this so the two can never drift.
	UFUNCTION(BlueprintCallable, Category = "Room|Enemies")
	int32 GetRemainingEnemyCount() const;

	// True once every currently-owned enemy has reached Banked, or is already being
	// destroyed (see HandleOwnedEnemyDestroyed) - vacuously true for an empty list, so a
	// room with nothing to clear never gates its own doors.
	UFUNCTION(BlueprintCallable, Category = "Room|Enemies")
	bool IsRoomCleared() const;

	// Adds Enemy to this room's ownership (no-op if already owned or invalid) and binds
	// its OnEnemyBanked/OnDestroyed so the room's cleared-state is re-evaluated when it
	// banks or is otherwise removed. Broadcasts OnRoomClearedStateChanged immediately -
	// if Enemy isn't already Banked, this turns a previously-cleared room un-cleared,
	// re-gating any door bound to it. This is the hook a future wave-spawner
	// integration calls; this issue's own tests call it directly to simulate that
	// without depending on UWaveSpawnerComponent (unwired to ARoomActor today).
	UFUNCTION(BlueprintCallable, Category = "Room|Enemies")
	void AddOwnedEnemy(AEnemyBase* Enemy);

	// Issue #245 / PRD Room Encounter Flow REQ-3: seconds the first-entry
	// countdown runs before this room activates. Configurable per placed
	// instance; default matches the operator's locked 3.0s decision.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Room|Activation")
	float RoomActivationCountdownSeconds = 3.0f;

	// True once this room's first-entry countdown has expired - a permanent
	// one-shot latch, never reset, so re-entry can never re-trigger it.
	UFUNCTION(BlueprintPure, Category = "Room|Activation")
	bool IsRoomActivated() const { return bRoomActivated; }

	// Raw countdown-running flag, for callers (tests, HUD/debug) that want the literal
	// state rather than the gate-specific framing IsActivationPending() below uses -
	// same underlying field, kept in sync by construction (both just read
	// bCountdownActive directly).
	UFUNCTION(BlueprintPure, Category = "Room|Activation")
	bool IsCountdownActive() const { return bCountdownActive; }

	UFUNCTION(BlueprintPure, Category = "Room|Activation")
	float GetRemainingCountdownSeconds() const { return RemainingCountdownSeconds; }

	// True exactly while this room's first-entry countdown is running -
	// AEnemyBase::IsPlayerInOwningRoom()'s Idle->Alert gate (issue #244) checks
	// this to stay closed for the full countdown duration, per this issue's own
	// AC wording ("While counting: the room's owned enemies hold Idle") and its
	// Notes section ("holds that gate closed... rather than building its own
	// separate hold mechanism"). Deliberately keyed on bCountdownActive, not on
	// "not yet activated" more broadly - a room whose countdown was never
	// started (CheckFirstEntry never called, e.g. every pre-#245 Automation
	// test that drives TickCheckDetection directly without a real Tick loop)
	// must keep the unmodified #244 gate behavior, not a new permanent hold.
	bool IsActivationPending() const { return bCountdownActive; }

	// Fires whenever IsRoomCleared() may have changed - after an owned enemy banks, or
	// after AddOwnedEnemy() adds a not-yet-banked enemy. ADoorConnectorActor::GatingRoom
	// binds to this rather than polling.
	UPROPERTY(BlueprintAssignable, Category = "Room|Enemies")
	FOnRoomClearedStateChanged OnRoomClearedStateChanged;

	// Nearest-room-by-distance: the same "which room owns this actor" rule
	// KrowdKontrolLevelTestUtils::FindNearestRoom documents and now delegates to (rather
	// than duplicating the comparison), so BeginPlay's auto-discovery below and the test
	// suite's expectations can never drift apart.
	static ARoomActor* FindNearestRoom(const AActor* Actor, const TArray<ARoomActor*>& Rooms);

	// Same nearest-room-by-distance rule as the AActor overload above, operating
	// directly on a world location - lets a caller resolve "which room is nearest"
	// without a resolvable AActor for the query point (issue #244: AEnemyBase's
	// room-detection gate only ever has a plain FVector player location, not the
	// player pawn itself, and changing TickCheckDetection's signature to carry the
	// pawn would ripple through ~150 existing Automation test call sites).
	static ARoomActor* FindNearestRoom(const FVector& Location, const TArray<ARoomActor*>& Rooms);

	// Distance from an axis-aligned box's centre, along Direction2D (need not be
	// normalized), to where a ray in that exact direction exits the box - the correct
	// primitive for "where does the corridor to my neighbor cross my own wall", not the
	// box's support function (which measures maximum projection width, not ray-exit
	// distance, and only agrees with it when Direction2D is axis-aligned). Shared by
	// SealRoomPerimeter()'s own gap-centering math and
	// ADoorConnectorActor::RecomputeConnectorGeometry()'s guard-rail span, both of which
	// need the same "where does this room's actual wall sit along this line" answer.
	static float ComputeAxisExitDistance(const FVector2D& HalfExtent, const FVector2D& Direction2D);

	// Shared #218-channel recipe (Block-only-ECC_WorldDynamic response, matching the
	// real player pawn's presented channel; ECC_WorldStatic object type, matching
	// static level geometry and, per issue #243 Finding 3, the one channel a fleeing
	// enemy's own narrowed-for-banking response can still be blocked by - see the .cpp
	// definition's comment) for every blocking volume this issue's fix creates or
	// reuses: room wall-gap flanks, corridor guard rails, and the door's own
	// GateBlockingComponent. A single place to change if #218/#243's channel findings
	// are ever revisited.
	static void ConfigureWorldDynamicBlockingCollision(UPrimitiveComponent* Component);

	// Issue #274 code-review follow-up, promoted here from EnemyBase.cpp's own
	// anonymous namespace by issue #245: without this cache, a per-frame caller
	// (AEnemyBase::IsPlayerInOwningRoom, and now ARoomActor::CheckFirstEntry's own
	// per-frame poll) would rerun a full TActorIterator<ARoomActor> world scan + TArray
	// allocation from scratch on every call. Collapses that down to one scan per
	// frame shared by every caller that needs it this frame, mirroring the existing
	// "scan TActorIterator once, reuse across N comparisons" shape at this class's
	// own BeginPlay. Rooms are static, hand-placed level geometry that never changes
	// at runtime in this codebase today (this class's own comment above), so a lazy
	// once-per-frame refresh - rather than invalidating on room spawn/destroy - is
	// safe. Single-entry (not world-keyed): this codebase never ticks more than one
	// World at a time, and a mismatched World pointer alone forces a rescan, so
	// switching worlds (e.g. between Automation tests) can't return stale data.
	static const TArray<ARoomActor*>& GetCachedRoomList(UWorld* World);

	// Half-extents (cm) of the room's greybox floor slab - full floor is 2x this.
	// Rooms in both hand-authored levels are spaced 3000cm apart along the chain axis
	// (docs/prd-level-playability-presentation.md:31-33), so the 1000cm default leaves
	// room for a connector strip without overlapping the next room's floor.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Room")
	FVector2D RoomFloorExtent = FVector2D(1000.f, 1000.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Room")
	float RoomFloorThickness = 20.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Room")
	float RoomWallHeight = 300.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Room")
	float RoomWallThickness = 20.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Room")
	TObjectPtr<UStaticMeshComponent> FloorMeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Room")
	TObjectPtr<UStaticMeshComponent> WallNorthMeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Room")
	TObjectPtr<UStaticMeshComponent> WallSouthMeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Room")
	TObjectPtr<UStaticMeshComponent> WallEastMeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Room")
	TObjectPtr<UStaticMeshComponent> WallWestMeshComponent;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	// Routes a banked regular enemy into its own TransitionToBanked() - the "room-
	// scope owner" subscriber the issue's Ask #3 calls out as an acceptable
	// alternative to the zone subscribing to itself. Deliberately does NOT call
	// URoomEnemyBudgetController::NotifyEnemyBanked() - that integration is separate,
	// out-of-scope future work per RoomEnemyBudgetController.h's own comment.
	UFUNCTION()
	void HandleZoneActorBanked(AActor* BankedActor);

	UFUNCTION()
	void HandleOwnedEnemyBanked();

	// Bound to each owned enemy's AActor::OnDestroyed so a room whose last un-banked
	// enemy is destroyed by something other than banking (Hard Invariant #2 forbids
	// gameplay code from doing this today, but editor/engine-level destruction is still
	// possible) still re-evaluates and opens its gated door, instead of soft-locking.
	UFUNCTION()
	void HandleOwnedEnemyDestroyed(AActor* DestroyedActor);

	void BindOwnedEnemyDelegate(AEnemyBase* Enemy);

	// Checks whether PlayerLocation just resolved to this room for the first
	// time via FindNearestRoom, and if so starts the countdown. No-op if
	// already activated, already counting down, or the room has nothing to
	// hold (IsRoomCleared()). Friend-testable so the Automation test can drive
	// it without a real Tick loop or a live player pawn.
	void CheckFirstEntry(const FVector& PlayerLocation);

	// Advances the running countdown by DeltaSeconds, refreshing the
	// on-screen prompt digit at each integer boundary, and calling
	// ActivateRoom() once it reaches zero. No-op if no countdown is active.
	void AdvanceCountdown(float DeltaSeconds);

	void StartCountdown();
	void ActivateRoom();
	void UpdateCountdownPrompt();

	// Lazily resolves the world's AKrowdKontrolPlayerController-owned
	// OnScreenPromptWidgetInstance - mirrors
	// UAbilityMatchupNudgeComponent::ResolvePromptWidget()'s exact shape.
	UOnScreenPromptWidget* ResolvePromptWidget();

	UPROPERTY()
	TObjectPtr<UOnScreenPromptWidget> CachedPromptWidget;

	bool bHasWarnedMissingPromptWidget = false;

	bool bCountdownActive = false;
	bool bRoomActivated = false;
	float RemainingCountdownSeconds = 0.0f;

	// The last integer digit shown via ShowPrompt() during the current
	// countdown, so UpdateCountdownPrompt() only re-calls ShowPrompt() on an
	// actual 3->2->1 boundary crossing, not every tick. Reset to -1 by
	// StartCountdown().
	int32 LastDisplayedCount = -1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Room", meta = (AllowPrivateAccess = "true"))
	TArray<FRoomTargetZone> TargetZones;

	// Flank collision volumes SealRoomPerimeter() creates for wall sides that have a
	// connecting door - tracked so a repeat call can destroy and rebuild them instead
	// of leaking components.
	UPROPERTY()
	TArray<TObjectPtr<UBoxComponent>> WallGapFlankComponents;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Room|Enemies", meta = (AllowPrivateAccess = "true"))
	TArray<TObjectPtr<AEnemyBase>> OwnedEnemies;
};
