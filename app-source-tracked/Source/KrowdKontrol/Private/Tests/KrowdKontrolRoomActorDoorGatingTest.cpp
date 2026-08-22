// Confirms ADoorConnectorActor's gate (issue #218) stays closed (blocking collision on
// GateBlockingComponent) while its GatingRoom still has un-banked OwnedEnemies, opens
// once every owned enemy reaches Banked, and re-closes if a new enemy is added to an
// already-cleared room (e.g. a wave spawn) - driven directly via
// ARoomActor::AddOwnedEnemy() since UWaveSpawnerComponent isn't wired to ARoomActor
// today (out of scope for this issue, see its own header comment). Also covers: a door
// with no GatingRoom stays always open (Test 4); OwnedEnemies hand-placed before
// BeginPlay(), not added via AddOwnedEnemy() (Test 5); and an owned enemy destroyed
// (not banked) doesn't permanently soft-lock the door (Test 6).
//
// Needs a real BeginPlay() pass for ARoomActor::OnRoomClearedStateChanged /
// ADoorConnectorActor::GatingRoom's delegate binding to be live, so this mirrors
// KrowdKontrolRoomActorBankingWiringTest.cpp's World->InitializeActorsForPlay()/
// World->SetBegunPlay(true) pair and SpawnActorDeferred()+FinishSpawning() scaffold
// (populating OwnedEnemies/GatingRoom before FinishSpawning() fires BeginPlay()).
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the other
// KrowdKontrol.Unit.* tests.

#include "Misc/AutomationTest.h"
#include "RoomActor.h"
#include "DoorConnectorActor.h"
#include "TrooperEnemy.h"
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

	ARoomActor* RoomOne = World->SpawnActor<ARoomActor>(FVector::ZeroVector, FRotator::ZeroRotator);
	ARoomActor* RoomTwo = World->SpawnActor<ARoomActor>(FVector(3000.f, 0.f, 0.f), FRotator::ZeroRotator);
	if (!TestNotNull(TEXT("RoomOne should spawn"), RoomOne)) { return false; }
	if (!TestNotNull(TEXT("RoomTwo should spawn"), RoomTwo)) { return false; }

	ATrooperEnemy* EnemyA = World->SpawnActor<ATrooperEnemy>(FVector(500.f, 0.f, 0.f), FRotator::ZeroRotator);
	ATrooperEnemy* EnemyB = World->SpawnActor<ATrooperEnemy>(FVector(500.f, 500.f, 0.f), FRotator::ZeroRotator);
	if (!TestNotNull(TEXT("EnemyA should spawn"), EnemyA)) { return false; }
	if (!TestNotNull(TEXT("EnemyB should spawn"), EnemyB)) { return false; }

	ADoorConnectorActor* Door = World->SpawnActorDeferred<ADoorConnectorActor>(
		ADoorConnectorActor::StaticClass(), FTransform::Identity);
	if (!TestNotNull(TEXT("Door should spawn"), Door)) { return false; }
	Door->RoomA = RoomOne;
	Door->RoomB = RoomTwo;
	Door->GatingRoom = RoomOne;
	// AddOwnedEnemy(), not a raw OwnedEnemies.Add() - RoomOne was spawned via plain
	// SpawnActor() while the world had already begun play, so its BeginPlay() (and the
	// OwnedEnemies-binding loop it runs) already fired before this point with an empty
	// list. AddOwnedEnemy() binds each enemy's OnEnemyBanked delegate immediately,
	// independent of BeginPlay timing - the same call Test 3 below uses for WaveEnemy.
	RoomOne->AddOwnedEnemy(EnemyA);
	RoomOne->AddOwnedEnemy(EnemyB);
	Door->FinishSpawning(FTransform::Identity);

	// --- Test 1: door blocked while >=1 owned enemy un-banked ---
	TestFalse(TEXT("Door should start closed while RoomOne's owned enemies are un-banked"),
		Door->bIsGateOpen);
	TestEqual(TEXT("Gate blocking component should have collision enabled while closed"),
		Door->GateBlockingComponent->GetCollisionEnabled(), ECollisionEnabled::QueryOnly);
	TestNotEqual(TEXT("Gate blocking component should not unintentionally block camera traces"),
		Door->GateBlockingComponent->GetCollisionResponseToChannel(ECC_Camera), ECR_Block);
	TestNotEqual(TEXT("Gate blocking component should not unintentionally block WorldDynamic traces"),
		Door->GateBlockingComponent->GetCollisionResponseToChannel(ECC_WorldDynamic), ECR_Block);

	EnemyA->TickCheckDetection(EnemyA->GetActorLocation());
	EnemyA->ReceiveControl(EAbilitySlot::Snare);
	EnemyA->TransitionToBanked();
	TestFalse(TEXT("Door should stay closed while EnemyB is still un-banked"),
		Door->bIsGateOpen);

	// --- Test 2: door opens on the last owned enemy reaching Banked ---
	EnemyB->TickCheckDetection(EnemyB->GetActorLocation());
	EnemyB->ReceiveControl(EAbilitySlot::Snare);
	EnemyB->TransitionToBanked();
	TestTrue(TEXT("Door should open once every owned enemy has reached Banked"),
		Door->bIsGateOpen);
	TestEqual(TEXT("Gate blocking component should have no collision once open"),
		Door->GateBlockingComponent->GetCollisionEnabled(), ECollisionEnabled::NoCollision);

	// --- Test 3: wave-spawned addition to an already-open room re-gates the door ---
	ATrooperEnemy* WaveEnemy = World->SpawnActor<ATrooperEnemy>(FVector(500.f, -500.f, 0.f), FRotator::ZeroRotator);
	if (!TestNotNull(TEXT("Wave-spawned enemy should spawn"), WaveEnemy)) { return false; }
	RoomOne->AddOwnedEnemy(WaveEnemy);
	TestFalse(TEXT("Door should re-close once a new owned enemy is added to an already-cleared room"),
		Door->bIsGateOpen);
	TestEqual(TEXT("Gate blocking component should re-enable collision on re-close"),
		Door->GateBlockingComponent->GetCollisionEnabled(), ECollisionEnabled::QueryOnly);

	WaveEnemy->TickCheckDetection(WaveEnemy->GetActorLocation());
	WaveEnemy->ReceiveControl(EAbilitySlot::Snare);
	WaveEnemy->TransitionToBanked();
	TestTrue(TEXT("Door should re-open once the wave-spawned enemy also reaches Banked"),
		Door->bIsGateOpen);
	TestEqual(TEXT("Gate blocking component should have no collision once re-opened"),
		Door->GateBlockingComponent->GetCollisionEnabled(), ECollisionEnabled::NoCollision);

	// --- Test 4: a door with no GatingRoom configured stays always open ---
	ADoorConnectorActor* UngatedDoor = World->SpawnActorDeferred<ADoorConnectorActor>(
		ADoorConnectorActor::StaticClass(), FTransform::Identity);
	if (!TestNotNull(TEXT("UngatedDoor should spawn"), UngatedDoor)) { return false; }
	UngatedDoor->RoomA = RoomOne;
	UngatedDoor->RoomB = RoomTwo;
	// GatingRoom left unset (nullptr) - the level's first-door default.
	UngatedDoor->FinishSpawning(FTransform::Identity);

	TestTrue(TEXT("A door with no GatingRoom should stay open even while RoomOne is uncleared"),
		UngatedDoor->bIsGateOpen);
	TestEqual(TEXT("An ungated door's blocking component should have no collision"),
		UngatedDoor->GateBlockingComponent->GetCollisionEnabled(), ECollisionEnabled::NoCollision);

	// --- Test 5: hand-placed OwnedEnemies (bound in BeginPlay(), not via AddOwnedEnemy()) ---
	// Simulates a real level room: OwnedEnemies is populated before FinishSpawning() fires
	// BeginPlay(), exercising the load-time binding loop instead of the runtime
	// AddOwnedEnemy() path every other test above uses.
	ARoomActor* PrePopulatedRoom = World->SpawnActorDeferred<ARoomActor>(
		ARoomActor::StaticClass(), FTransform::Identity);
	if (!TestNotNull(TEXT("PrePopulatedRoom should spawn"), PrePopulatedRoom)) { return false; }
	ATrooperEnemy* PrePlacedEnemy = World->SpawnActor<ATrooperEnemy>(FVector(1000.f, 1000.f, 0.f), FRotator::ZeroRotator);
	if (!TestNotNull(TEXT("PrePlacedEnemy should spawn"), PrePlacedEnemy)) { return false; }
	PrePopulatedRoom->OwnedEnemies.Add(PrePlacedEnemy);
	PrePopulatedRoom->FinishSpawning(FTransform::Identity);

	TestFalse(TEXT("A hand-placed room with an un-banked owned enemy should not report cleared"),
		PrePopulatedRoom->IsRoomCleared());

	PrePlacedEnemy->TickCheckDetection(PrePlacedEnemy->GetActorLocation());
	PrePlacedEnemy->ReceiveControl(EAbilitySlot::Snare);
	PrePlacedEnemy->TransitionToBanked();
	TestTrue(TEXT("Room should report cleared once its BeginPlay-bound enemy banks"),
		PrePopulatedRoom->IsRoomCleared());

	// --- Test 6: a destroyed owned enemy doesn't permanently soft-lock the room's door ---
	ATrooperEnemy* DoomedEnemy = World->SpawnActor<ATrooperEnemy>(FVector(500.f, -1000.f, 0.f), FRotator::ZeroRotator);
	if (!TestNotNull(TEXT("DoomedEnemy should spawn"), DoomedEnemy)) { return false; }
	RoomOne->AddOwnedEnemy(DoomedEnemy);
	TestFalse(TEXT("Room should be un-cleared while DoomedEnemy is still owned and un-banked"),
		RoomOne->IsRoomCleared());
	TestFalse(TEXT("Door should re-close once DoomedEnemy is added to the already-cleared room"),
		Door->bIsGateOpen);

	DoomedEnemy->Destroy();
	TestTrue(TEXT("A destroyed owned enemy should not block IsRoomCleared()"),
		RoomOne->IsRoomCleared());
	TestTrue(TEXT("Door should re-open once its last un-banked owned enemy is destroyed"),
		Door->bIsGateOpen);
	TestEqual(TEXT("Gate blocking component should have no collision once re-opened by destruction"),
		Door->GateBlockingComponent->GetCollisionEnabled(), ECollisionEnabled::NoCollision);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
