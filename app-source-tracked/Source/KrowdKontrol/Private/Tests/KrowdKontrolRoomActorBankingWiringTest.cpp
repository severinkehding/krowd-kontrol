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
#include "RunnerEnemy.h"
#include "EnemyType.h"
#include "AbilitySlot.h"
#include "ReservedGameplayColours.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"
#include "Components/PrimitiveComponent.h"

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

static int32 CountAttachedZones(AActor* Marker)
{
	TArray<AActor*> Attached;
	Marker->GetAttachedActors(Attached);
	int32 Count = 0;
	for (AActor* A : Attached)
	{
		if (A && A->IsA<ATargetZone>()) { ++Count; }
	}
	return Count;
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

	// Third marker (TR_UPR, countered by Root/Teal) - issue #242's acceptance
	// criterion: a Controlled Trooper delivered onto its own type-matched zone must
	// reach Banked, proving the fix (ATrooperEnemy now owns an
	// EnemyTypeIndicatorComponent) closes the real gap, not just the synthetic one.
	AActor* TrooperMarker = Room->AddTargetZone(EEnemyType::TR_UPR);
	if (!TestNotNull(TEXT("Trooper marker should spawn"), TrooperMarker))
	{
		return false;
	}
	// AddTargetZone() snaps every marker to the room's own location (no per-marker
	// placement param), so Marker/SecondMarker/TrooperMarker - and therefore their
	// self-healed ATargetZones below - would otherwise all sit exactly on top of one
	// another. That's harmless for the first two (their enemy types never match each
	// other), but the existing wrong-type fixture below deliberately overlaps a real
	// ATrooperEnemy onto `BankingZone` (RU_NNR) to prove type-keyed rejection - if
	// TrooperBankingZone stayed co-located with it, that same physical overlap would
	// also satisfy TrooperBankingZone's TR_UPR match and incorrectly bank there.
	// Moving this marker well clear of the others keeps the two scenarios physically
	// independent.
	TrooperMarker->SetActorLocation(FVector(5000.f, 5000.f, 0.f));

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

	ATargetZone* TrooperBankingZone = FindAttachedZone(TrooperMarker);
	if (!TestNotNull(TEXT("Trooper marker should have a self-healed ATargetZone attached via BeginPlay()"), TrooperBankingZone))
	{
		return false;
	}
	TestEqual(TEXT("Trooper banking zone should be colour-tagged Teal for a TR_UPR marker"),
		TrooperBankingZone->ZoneColourTag, ReservedGameplayColours::GetTealTag());

	// Calling EnsureBankingZonesWired() a second time must not double-spawn, for
	// either marker.
	Room->EnsureBankingZonesWired();
	TestEqual(TEXT("EnsureBankingZonesWired should be idempotent - no duplicate zone"),
		CountAttachedZones(Marker), 1);
	TestEqual(TEXT("EnsureBankingZonesWired should be idempotent for the second marker too"),
		CountAttachedZones(SecondMarker), 1);

	// Type-keyed acceptance (operator ruling 2026-08-22): the zone is a pen for the
	// marker's EEnemyType, so the enemy's own type is what matters now - ARunnerEnemy
	// (RU-NNR) matches the first marker's zone. Controlled with STUN deliberately:
	// the colour-neutral starter ability must bank at a type-matched pen (MISSION
	// "each ability viable solo at base effectiveness"), the exact case the old
	// colour gate made impossible.
	ARunnerEnemy* Enemy = World->SpawnActor<ARunnerEnemy>(
		BankingZone->GetActorLocation() + FVector(1000.f, 0.f, 0.f), FRotator::ZeroRotator);
	if (!TestNotNull(TEXT("Enemy should spawn"), Enemy))
	{
		return false;
	}

	// Pass-1 review follow-up (low severity, code_quality): pins down the exact
	// collision-response value EnemyBase::BeginPlay() sets, channel-wide and
	// actor-wide (see that method's own comment on the residual risk), as a tripwire -
	// if this regresses or someone narrows it away without updating this test, this
	// assertion (not just the physical-overlap assertions below) is what catches it.
	if (UPrimitiveComponent* EnemyRootPrimitive = Cast<UPrimitiveComponent>(Enemy->GetRootComponent()))
	{
		TestEqual(TEXT("Enemy root's response to the WorldDynamic channel should be Overlap (EnemyBase::BeginPlay's channel-wide fix, issue #211) - re-check this assertion before scoping the fix to ATargetZone specifically"),
			EnemyRootPrimitive->GetCollisionResponseToChannel(ECC_WorldDynamic), ECR_Overlap);
	}

	// Drive Idle -> Alert via the friend-granted private TickCheckDetection (distance
	// 0 from itself is always within DetectionRangeUnits), then ReceiveControl (public)
	// into Controlled with Stun - colour-neutral, proving acceptance is type-keyed.
	Enemy->TickCheckDetection(Enemy->GetActorLocation());
	Enemy->ReceiveControl(EAbilitySlot::Stun);
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

	// A controlled enemy of the *wrong type* physically overlapping the zone should
	// not bank - the pen takes Runners, a Trooper stays Controlled (type-keyed
	// rejection; the ability used is irrelevant either way).
	ATrooperEnemy* MismatchedEnemy = World->SpawnActor<ATrooperEnemy>(
		BankingZone->GetActorLocation() + FVector(1000.f, 500.f, 0.f), FRotator::ZeroRotator);
	if (TestNotNull(TEXT("Mismatched enemy should spawn"), MismatchedEnemy))
	{
		MismatchedEnemy->TickCheckDetection(MismatchedEnemy->GetActorLocation());
		MismatchedEnemy->ReceiveControl(EAbilitySlot::Root);
		MismatchedEnemy->SetActorLocation(BankingZone->GetActorLocation(), /*bSweep=*/true);
		TestEqual(TEXT("A wrong-type controlled enemy overlapping the zone should not bank (type-keyed)"),
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

	// Issue #242 acceptance criterion: a Controlled Trooper delivered onto its own
	// TR_UPR-typed zone reaches Banked. Controlled with Root deliberately - Root is
	// TR_UPR's own countering ability (AbilityData.cpp), though type-keyed acceptance
	// means any controlling ability would bank here; Stun is already covered by the
	// RU-NNR happy path above, so this exercises a second ability for coverage.
	ATrooperEnemy* BankableTrooper = World->SpawnActor<ATrooperEnemy>(
		TrooperBankingZone->GetActorLocation() + FVector(1000.f, 0.f, 0.f), FRotator::ZeroRotator);
	if (!TestNotNull(TEXT("Bankable trooper should spawn"), BankableTrooper))
	{
		return false;
	}
	BankableTrooper->TickCheckDetection(BankableTrooper->GetActorLocation());
	BankableTrooper->ReceiveControl(EAbilitySlot::Root);
	TestEqual(TEXT("Trooper should be Controlled before overlapping its zone"),
		static_cast<uint8>(BankableTrooper->GetEnemyState()), static_cast<uint8>(EEnemyState::Controlled));

	BankableTrooper->SetActorLocation(TrooperBankingZone->GetActorLocation(), /*bSweep=*/true);

	TestEqual(TEXT("A controlled Trooper overlapping its own TR_UPR zone should reach Banked (issue #242)"),
		static_cast<uint8>(BankableTrooper->GetEnemyState()), static_cast<uint8>(EEnemyState::Banked));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
