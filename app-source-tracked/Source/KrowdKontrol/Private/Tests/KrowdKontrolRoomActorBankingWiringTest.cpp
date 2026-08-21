// Confirms ARoomActor::EnsureBankingZonesWired() (issue #211) self-heals a correctly
// colour-tagged ATargetZone attached to each already-placed TargetZones marker, is
// idempotent across repeated calls, and that a real physical overlap by a controlled,
// correctly-CC'd AEnemyBase reaches Banked end-to-end through
// ATargetZone::OnActorBanked -> ARoomActor::HandleZoneActorBanked ->
// AEnemyBase::TransitionToBanked() - the wiring this issue adds, not new detection or
// broadcast logic (both already proven independently by KrowdKontrolTargetZoneTest.cpp
// and KrowdKontrolHerdableTest.cpp).
//
// Requires a real physics overlap to fire OnComponentBeginOverlap, so this test needs
// the same World->InitializeActorsForPlay()/World->SetBegunPlay(true) pair
// KrowdKontrolTargetZoneTest.cpp's file comment documents and explains in detail -
// CreateNewMap()'s bare editor world otherwise silently drops the overlap event.
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the other
// KrowdKontrol.Unit.* tests.

#include "Misc/AutomationTest.h"
#include "RoomActor.h"
#include "TargetZone.h"
#include "TrooperEnemy.h"
#include "EnemyType.h"
#include "AbilitySlot.h"
#include "ReservedGameplayColours.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"

static ATargetZone* FindAttachedZone(AActor* Marker)
{
	TArray<AActor*> Attached;
	Marker->GetAttachedActors(Attached);
	for (AActor* A : Attached)
	{
		if (ATargetZone* Zone = Cast<ATargetZone>(A))
		{
			return Zone;
		}
	}
	return nullptr;
}

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolRoomActorBankingWiringTest,
	"KrowdKontrol.Unit.RoomActorBankingWiring",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolRoomActorBankingWiringTest::RunTest(const FString& Parameters)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
	{
		return false;
	}
	// Required for a real physics overlap to fire OnComponentBeginOverlap - see
	// KrowdKontrolTargetZoneTest.cpp's file comment for why both calls are needed.
	World->InitializeActorsForPlay(FURL());
	World->SetBegunPlay(true);

	// Spawned deferred so both markers can be added to TargetZones *before*
	// FinishSpawning() below fires BeginPlay() - this exercises the actual production
	// entry point (BeginPlay() -> EnsureBankingZonesWired()) against markers that
	// already exist at BeginPlay time, matching the "rooms placed/serialized before
	// this class carried banking behaviour" scenario the method's own doc comment
	// describes, rather than only ever calling EnsureBankingZonesWired() explicitly
	// against an empty-then-populated room.
	ARoomActor* Room = World->SpawnActorDeferred<ARoomActor>(ARoomActor::StaticClass(), FTransform::Identity);
	if (!TestNotNull(TEXT("Room should spawn"), Room))
	{
		return false;
	}

	// RU-NNR is countered by Snare/Purple (AbilityData.cpp).
	AActor* Marker = Room->AddTargetZone(EEnemyType::RU_NNR);
	if (!TestNotNull(TEXT("Marker should spawn"), Marker))
	{
		return false;
	}

	// Second marker (SN-1PR, countered by Sleep/Blue) so the per-marker loop in
	// EnsureBankingZonesWired() is exercised across more than one entry - a room with
	// a single target zone is the degenerate case, not the realistic one.
	AActor* SecondMarker = Room->AddTargetZone(EEnemyType::SN_1PR);
	if (!TestNotNull(TEXT("Second marker should spawn"), SecondMarker))
	{
		return false;
	}

	Room->FinishSpawning(FTransform::Identity);

	ATargetZone* BankingZone = FindAttachedZone(Marker);
	if (!TestNotNull(TEXT("Marker should have a self-healed ATargetZone attached via BeginPlay()"), BankingZone))
	{
		return false;
	}
	TestEqual(TEXT("Banking zone should be colour-tagged Purple for a RU-NNR marker"),
		BankingZone->ZoneColourTag, ReservedGameplayColours::GetPurpleTag());

	ATargetZone* SecondBankingZone = FindAttachedZone(SecondMarker);
	if (!TestNotNull(TEXT("Second marker should have a self-healed ATargetZone attached via BeginPlay()"), SecondBankingZone))
	{
		return false;
	}
	TestEqual(TEXT("Second banking zone should be colour-tagged Blue for an SN-1PR marker"),
		SecondBankingZone->ZoneColourTag, ReservedGameplayColours::GetBlueTag());

	// Calling EnsureBankingZonesWired() a second time must not double-spawn, for
	// either marker.
	Room->EnsureBankingZonesWired();
	TArray<AActor*> AttachedAfterSecondCall;
	Marker->GetAttachedActors(AttachedAfterSecondCall);
	int32 ZoneCount = 0;
	for (AActor* A : AttachedAfterSecondCall)
	{
		if (A && A->IsA<ATargetZone>()) { ++ZoneCount; }
	}
	TestEqual(TEXT("EnsureBankingZonesWired should be idempotent - no duplicate zone"), ZoneCount, 1);

	TArray<AActor*> SecondAttachedAfterSecondCall;
	SecondMarker->GetAttachedActors(SecondAttachedAfterSecondCall);
	int32 SecondZoneCount = 0;
	for (AActor* A : SecondAttachedAfterSecondCall)
	{
		if (A && A->IsA<ATargetZone>()) { ++SecondZoneCount; }
	}
	TestEqual(TEXT("EnsureBankingZonesWired should be idempotent for the second marker too"), SecondZoneCount, 1);

	// Any concrete AEnemyBase subclass works here - ATrooperEnemy (TR-UPR) is used
	// purely as a spawnable non-abstract instance. Its native EnemyType is irrelevant:
	// GetHerdColourTag() derives from whichever ability lands (ControllingAbility),
	// not from the enemy's own EnemyType (see EnemyBase.cpp's GetHerdColourTag()).
	ATrooperEnemy* Enemy = World->SpawnActor<ATrooperEnemy>(
		BankingZone->GetActorLocation() + FVector(1000.f, 0.f, 0.f), FRotator::ZeroRotator);
	if (!TestNotNull(TEXT("Enemy should spawn"), Enemy))
	{
		return false;
	}

	// Drive Idle -> Alert via the friend-granted private TickCheckDetection (distance
	// 0 from itself is always within DetectionRangeUnits), then ReceiveControl (public)
	// into Controlled with Snare - RU-NNR's counter, matching the zone's Purple tag.
	Enemy->TickCheckDetection(Enemy->GetActorLocation());
	Enemy->ReceiveControl(EAbilitySlot::Snare);
	TestEqual(TEXT("Enemy should be Controlled before overlapping the zone"),
		static_cast<uint8>(Enemy->GetEnemyState()), static_cast<uint8>(EEnemyState::Controlled));

	Enemy->SetActorLocation(BankingZone->GetActorLocation(), /*bSweep=*/true);

	TestEqual(TEXT("A controlled, correctly-CC'd enemy overlapping its room's banking zone should reach Banked"),
		static_cast<uint8>(Enemy->GetEnemyState()), static_cast<uint8>(EEnemyState::Banked));

	// Reject paths, driven against a real production enemy under the same physical
	// collision-response fix that makes the happy path above possible - neither of
	// these was previously exercised against anything other than the synthetic
	// ATargetZoneTestActor fixture in KrowdKontrolTargetZoneTest.cpp, which never had
	// the Block-vs-Overlap problem this PR's EnemyBase::BeginPlay() fix resolves.

	// A controlled enemy with the *wrong* colour physically overlapping the zone
	// should not bank.
	ATrooperEnemy* MismatchedEnemy = World->SpawnActor<ATrooperEnemy>(
		BankingZone->GetActorLocation() + FVector(1000.f, 500.f, 0.f), FRotator::ZeroRotator);
	if (TestNotNull(TEXT("Mismatched enemy should spawn"), MismatchedEnemy))
	{
		MismatchedEnemy->TickCheckDetection(MismatchedEnemy->GetActorLocation());
		MismatchedEnemy->ReceiveControl(EAbilitySlot::Root); // Teal - zone is Purple.
		MismatchedEnemy->SetActorLocation(BankingZone->GetActorLocation(), /*bSweep=*/true);
		TestEqual(TEXT("A colour-mismatched controlled enemy overlapping the zone should not bank"),
			static_cast<uint8>(MismatchedEnemy->GetEnemyState()), static_cast<uint8>(EEnemyState::Controlled));
	}

	// An uncontrolled enemy physically overlapping the zone should not bank.
	ATrooperEnemy* UncontrolledEnemy = World->SpawnActor<ATrooperEnemy>(
		BankingZone->GetActorLocation() + FVector(1000.f, -500.f, 0.f), FRotator::ZeroRotator);
	if (TestNotNull(TEXT("Uncontrolled enemy should spawn"), UncontrolledEnemy))
	{
		UncontrolledEnemy->SetActorLocation(BankingZone->GetActorLocation(), /*bSweep=*/true);
		TestNotEqual(TEXT("An uncontrolled enemy overlapping the zone should not bank"),
			static_cast<uint8>(UncontrolledEnemy->GetEnemyState()), static_cast<uint8>(EEnemyState::Banked));
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
