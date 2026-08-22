// Confirms ARoomActor/ADoorConnectorActor's door-gating mechanism (issue #218,
// attempt 2): a door bound to a GatingRoom stays blocked while any of that room's
// OwnedEnemies is un-Banked, opens once the last one banks, and re-gates when a
// wave-spawned addition arrives - and, per the CRITICAL finding that rejected attempt
// 1 (PR #228), the blocking collision must actually stop the real player pawn, which
// presents as ECC_WorldStatic (its MeshComponent never sets an explicit collision
// profile/object type - see FlatCamera3DPrototypePawn.cpp), not ECC_Pawn.
//
// Requires World->InitializeActorsForPlay()/World->SetBegunPlay(true) - same rationale
// KrowdKontrolRoomActorBankingWiringTest.cpp's file comment documents: a bare
// CreateNewMap() world silently drops dynamic-multicast-delegate broadcasts between
// spawned actors (proven empirically while building this test - Enemy->OnEnemyBanked
// never reached ARoomActor's bound handler without it). Once the world has begun
// play, SpawnActor() auto-dispatches BeginPlay() immediately - so any door whose
// GatingRoom must be set before its own BeginPlay runs uses SpawnActorDeferred() +
// FinishSpawning(), the same pattern KrowdKontrolRoomActorBankingWiringTest.cpp uses
// for the analogous ARoomActor/TargetZones ordering problem. Each case also adds a
// room's owned enemies *before* deferred-finishing its door, so the door's one-time
// BeginPlay-time RefreshGateState() call observes the intended starting state.
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the other
// KrowdKontrol.Unit.* tests.

#include "Misc/AutomationTest.h"
#include "RoomActor.h"
#include "DoorConnectorActor.h"
#include "EnemyBaseTestActor.h"
#include "AbilitySlot.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"
#include "Components/BoxComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolRoomActorDoorGatingTest,
	"KrowdKontrol.Unit.RoomActorDoorGating",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolRoomActorDoorGatingTest::RunTest(const FString& Parameters)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
	{
		return false;
	}
	World->InitializeActorsForPlay(FURL());
	World->SetBegunPlay(true);

	const FVector ZeroDistanceLocation(0.0f, 0.0f, 0.0f);

	// (1)/(2)/(6): door stays blocked while any owned enemy is un-Banked, opens once
	// the last one banks, and while closed blocks the real player's channel
	// (ECC_WorldStatic - the CRITICAL regression from attempt 1).
	ARoomActor* Room = World->SpawnActor<ARoomActor>();
	if (!TestNotNull(TEXT("ARoomActor should spawn into the test World"), Room))
	{
		return false;
	}

	AEnemyBaseTestActor* EnemyOne = World->SpawnActor<AEnemyBaseTestActor>();
	AEnemyBaseTestActor* EnemyTwo = World->SpawnActor<AEnemyBaseTestActor>();
	if (!TestNotNull(TEXT("First AEnemyBaseTestActor should spawn into the test World"), EnemyOne) ||
		!TestNotNull(TEXT("Second AEnemyBaseTestActor should spawn into the test World"), EnemyTwo))
	{
		return false;
	}
	Room->AddOwnedEnemy(EnemyOne);
	Room->AddOwnedEnemy(EnemyTwo);

	// GatingRoom must be set before FinishSpawning() triggers BeginPlay - BeginPlay
	// only ever runs once, and its one-time RefreshGateState() call is what needs to
	// see both the correct GatingRoom and Room's already-populated OwnedEnemies.
	ADoorConnectorActor* Door = World->SpawnActorDeferred<ADoorConnectorActor>(ADoorConnectorActor::StaticClass(), FTransform::Identity);
	if (!TestNotNull(TEXT("ADoorConnectorActor should spawn into the test World"), Door))
	{
		return false;
	}
	Door->GatingRoom = Room;
	Door->FinishSpawning(FTransform::Identity);

	TestEqual(TEXT("Door should be blocked (QueryOnly) while any owned enemy is un-Banked"),
		Door->GateBlockingComponent->GetCollisionEnabled(), ECollisionEnabled::QueryOnly);
	TestEqual(TEXT("While closed, the gate should Block ECC_WorldStatic - the channel the real player pawn actually presents (issue #218 regression)"),
		Door->GateBlockingComponent->GetCollisionResponseToChannel(ECC_WorldStatic), ECR_Block);

	EnemyOne->TickCheckDetection(ZeroDistanceLocation); // Idle -> Alert
	EnemyOne->ReceiveControl(EAbilitySlot::Stun);        // Alert -> Controlled
	EnemyOne->TransitionToBanked();                      // Controlled -> Banked

	TestEqual(TEXT("Door should remain blocked while a second owned enemy is still un-Banked"),
		Door->GateBlockingComponent->GetCollisionEnabled(), ECollisionEnabled::QueryOnly);

	EnemyTwo->TickCheckDetection(ZeroDistanceLocation);
	EnemyTwo->ReceiveControl(EAbilitySlot::Stun);
	EnemyTwo->TransitionToBanked();

	TestEqual(TEXT("Door should open (NoCollision) once the last owned enemy reaches Banked"),
		Door->GateBlockingComponent->GetCollisionEnabled(), ECollisionEnabled::NoCollision);

	// (3): a wave-spawned addition to an already-open room re-gates the door. Door is
	// already bound to Room's OnRoomClearedStateChanged from its earlier BeginPlay, so
	// no re-spawn/defer is needed here.
	AEnemyBaseTestActor* WaveEnemy = World->SpawnActor<AEnemyBaseTestActor>();
	if (!TestNotNull(TEXT("Wave-spawned AEnemyBaseTestActor should spawn into the test World"), WaveEnemy))
	{
		return false;
	}
	Room->AddOwnedEnemy(WaveEnemy);
	TestEqual(TEXT("A wave-spawned addition to an already-open room should re-gate the door until it too is Banked"),
		Door->GateBlockingComponent->GetCollisionEnabled(), ECollisionEnabled::QueryOnly);

	WaveEnemy->TickCheckDetection(ZeroDistanceLocation);
	WaveEnemy->ReceiveControl(EAbilitySlot::Stun);
	WaveEnemy->TransitionToBanked();
	TestEqual(TEXT("Door should re-open once the wave-spawned enemy also reaches Banked"),
		Door->GateBlockingComponent->GetCollisionEnabled(), ECollisionEnabled::NoCollision);

	// (4): a door with no resolvable GatingRoom defaults open. Nothing needs to be set
	// before BeginPlay here, so a plain SpawnActor (immediate BeginPlay) is fine.
	ADoorConnectorActor* UngatedDoor = World->SpawnActor<ADoorConnectorActor>();
	if (!TestNotNull(TEXT("Second ADoorConnectorActor should spawn into the test World"), UngatedDoor))
	{
		return false;
	}
	TestNull(TEXT("A door with no rooms assigned should not auto-derive a GatingRoom"), UngatedDoor->GatingRoom.Get());
	TestEqual(TEXT("A door with no resolvable GatingRoom should default open (NoCollision)"),
		UngatedDoor->GateBlockingComponent->GetCollisionEnabled(), ECollisionEnabled::NoCollision);

	// (5): an owned enemy destroyed (not banked) still re-opens the door.
	ARoomActor* DestroyRoom = World->SpawnActor<ARoomActor>();
	if (!TestNotNull(TEXT("Third ARoomActor should spawn into the test World"), DestroyRoom))
	{
		return false;
	}

	AEnemyBaseTestActor* DestroyEnemy = World->SpawnActor<AEnemyBaseTestActor>();
	if (!TestNotNull(TEXT("Fourth AEnemyBaseTestActor should spawn into the test World"), DestroyEnemy))
	{
		return false;
	}
	DestroyRoom->AddOwnedEnemy(DestroyEnemy);

	ADoorConnectorActor* DestroyDoor = World->SpawnActorDeferred<ADoorConnectorActor>(ADoorConnectorActor::StaticClass(), FTransform::Identity);
	if (!TestNotNull(TEXT("Third ADoorConnectorActor should spawn into the test World"), DestroyDoor))
	{
		return false;
	}
	DestroyDoor->GatingRoom = DestroyRoom;
	DestroyDoor->FinishSpawning(FTransform::Identity);

	TestEqual(TEXT("Destroy-case door should start blocked with one un-Banked owned enemy"),
		DestroyDoor->GateBlockingComponent->GetCollisionEnabled(), ECollisionEnabled::QueryOnly);

	World->DestroyActor(DestroyEnemy);
	TestEqual(TEXT("Door should re-open once its last un-Banked owned enemy is destroyed rather than banked"),
		DestroyDoor->GateBlockingComponent->GetCollisionEnabled(), ECollisionEnabled::NoCollision);

	// (7): GatingRoom auto-derives to the lower-X of RoomA/RoomB when left unset - the
	// mechanism this issue's "zero .umap authoring" claim rests on. RoomA/RoomB are
	// deliberately assigned in reversed order from the expected result so a swapped
	// comparison or operand would be caught.
	ARoomActor* NearRoom = World->SpawnActor<ARoomActor>(ARoomActor::StaticClass(), FTransform(FVector(0.f, 0.f, 0.f)));
	ARoomActor* FarRoom = World->SpawnActor<ARoomActor>(ARoomActor::StaticClass(), FTransform(FVector(3000.f, 0.f, 0.f)));
	if (!TestNotNull(TEXT("Near ARoomActor should spawn into the test World"), NearRoom) ||
		!TestNotNull(TEXT("Far ARoomActor should spawn into the test World"), FarRoom))
	{
		return false;
	}
	ADoorConnectorActor* DerivedDoor = World->SpawnActorDeferred<ADoorConnectorActor>(ADoorConnectorActor::StaticClass(), FTransform::Identity);
	if (!TestNotNull(TEXT("Fourth ADoorConnectorActor should spawn into the test World"), DerivedDoor))
	{
		return false;
	}
	DerivedDoor->RoomA = FarRoom;
	DerivedDoor->RoomB = NearRoom;
	DerivedDoor->FinishSpawning(FTransform::Identity);

	TestEqual(TEXT("GatingRoom should auto-derive to the lower-X room regardless of RoomA/RoomB order (issue #218)"),
		DerivedDoor->GatingRoom.Get(), NearRoom);

	// (8): a valid GatingRoom with zero owned enemies is vacuously cleared, so its door
	// defaults open - documents the intentional behaviour rather than leaving it
	// coverage-free.
	ARoomActor* EmptyRoom = World->SpawnActor<ARoomActor>();
	if (!TestNotNull(TEXT("Empty ARoomActor should spawn into the test World"), EmptyRoom))
	{
		return false;
	}
	ADoorConnectorActor* EmptyRoomDoor = World->SpawnActorDeferred<ADoorConnectorActor>(ADoorConnectorActor::StaticClass(), FTransform::Identity);
	if (!TestNotNull(TEXT("Fifth ADoorConnectorActor should spawn into the test World"), EmptyRoomDoor))
	{
		return false;
	}
	EmptyRoomDoor->GatingRoom = EmptyRoom;
	EmptyRoomDoor->FinishSpawning(FTransform::Identity);

	TestEqual(TEXT("A door gating a room with zero owned enemies should default open (vacuous clear, issue #218)"),
		EmptyRoomDoor->GateBlockingComponent->GetCollisionEnabled(), ECollisionEnabled::NoCollision);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
