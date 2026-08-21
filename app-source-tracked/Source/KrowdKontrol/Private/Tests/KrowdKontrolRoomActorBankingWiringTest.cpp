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

	ARoomActor* Room = World->SpawnActor<ARoomActor>();
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

	// EnsureBankingZonesWired() is public+idempotent specifically so a test can call
	// it directly without needing to drive the full BeginPlay lifecycle.
	Room->EnsureBankingZonesWired();

	TArray<AActor*> Attached;
	Marker->GetAttachedActors(Attached);
	ATargetZone* BankingZone = nullptr;
	for (AActor* A : Attached)
	{
		if (ATargetZone* Zone = Cast<ATargetZone>(A))
		{
			BankingZone = Zone;
			break;
		}
	}
	if (!TestNotNull(TEXT("Marker should have a self-healed ATargetZone attached"), BankingZone))
	{
		return false;
	}
	TestEqual(TEXT("Banking zone should be colour-tagged Purple for a RU-NNR marker"),
		BankingZone->ZoneColourTag, ReservedGameplayColours::GetPurpleTag());

	// Calling EnsureBankingZonesWired() a second time must not double-spawn.
	Room->EnsureBankingZonesWired();
	TArray<AActor*> AttachedAfterSecondCall;
	Marker->GetAttachedActors(AttachedAfterSecondCall);
	int32 ZoneCount = 0;
	for (AActor* A : AttachedAfterSecondCall)
	{
		if (A && A->IsA<ATargetZone>()) { ++ZoneCount; }
	}
	TestEqual(TEXT("EnsureBankingZonesWired should be idempotent - no duplicate zone"), ZoneCount, 1);

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

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
