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
#include "BomberEnemy.h"
#include "EnemyType.h"
#include "AbilitySlot.h"
#include "ReservedGameplayColours.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"
#include "Components/PrimitiveComponent.h"
#include "PlaceholderTargetZoneActor.h"
#include "AbilityTargetingIndicatorComponent.h"
#include "PlaceholderCubeActor.h"
#include "Components/PointLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "AbilityData.h"

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

	// Fourth marker (B0_0MR, countered by Fear/Orange) - issue #253's acceptance
	// criterion: a Fear-controlled enemy (the new flee-flavour Controlled state) still
	// banks when delivered to its own type-matched zone, proving the new movement
	// flavour doesn't disturb the herd/bank chain. Moved clear of every other marker
	// (default-location Marker/SecondMarker and TrooperMarker at (5000,5000,0)) for
	// the same co-location reason TrooperMarker's own comment documents above.
	AActor* FearMarker = Room->AddTargetZone(EEnemyType::B0_0MR);
	if (!TestNotNull(TEXT("Fear marker should spawn"), FearMarker))
	{
		return false;
	}
	FearMarker->SetActorLocation(FVector(5000.f, -5000.f, 0.f));

	// Fifth marker: simulates a hand-placed zone (RoomActor.cpp's own
	// EnsureBankingZonesWired() doc comment names this scenario explicitly) by
	// pre-attaching a real ATargetZone *before* FinishSpawning(), left at
	// TargetZone.h's own bAcceptAnyEnemyType default (true) rather than the
	// freshly-spawned branch's hardcoded false. EnemyType passed to AddTargetZone()
	// is irrelevant here - it only tags TargetZones, not the pre-attached zone.
	// Proves MISSION.md Hard Invariant 3's five-colour lock: an any-type zone's
	// marker must never receive one of the four locked types' reserved colours.
	AActor* AnyTypeMarker = Room->AddTargetZone(EEnemyType::RU_NNR);
	if (!TestNotNull(TEXT("Any-type marker should spawn"), AnyTypeMarker))
	{
		return false;
	}
	AnyTypeMarker->SetActorLocation(FVector(-5000.f, 0.f, 0.f));
	ATargetZone* PreAttachedAnyTypeZone = World->SpawnActor<ATargetZone>(
		AnyTypeMarker->GetActorLocation(), FRotator::ZeroRotator);
	if (!TestNotNull(TEXT("Pre-attached any-type zone should spawn"), PreAttachedAnyTypeZone))
	{
		return false;
	}
	PreAttachedAnyTypeZone->AttachToActor(AnyTypeMarker, FAttachmentTransformRules::KeepWorldTransform);

	// Sixth marker: a designer-supplied custom MarkerClass (APlaceholderCubeActor,
	// already established for this purpose in KrowdKontrolRoomActorTest.cpp) driven
	// through the real BeginPlay() -> EnsureBankingZonesWired() production path -
	// proves ApplyChainColourToMarker()'s Cast<APlaceholderTargetZoneActor> guard
	// leaves a non-placeholder marker alone instead of crashing or miscasting.
	AActor* CustomClassMarker = Room->AddTargetZone(EEnemyType::SN_1PR, APlaceholderCubeActor::StaticClass());
	if (!TestNotNull(TEXT("Custom-MarkerClass marker should spawn"), CustomClassMarker))
	{
		return false;
	}
	CustomClassMarker->SetActorLocation(FVector(-5000.f, 5000.f, 0.f));

	Room->FinishSpawning(FTransform::Identity);

	ATargetZone* BankingZone = FindAttachedZone(Marker);
	if (!TestNotNull(TEXT("Marker should have a self-healed ATargetZone attached via BeginPlay()"), BankingZone))
	{
		return false;
	}
	TestEqual(TEXT("Banking zone should be colour-tagged Purple for a RU-NNR marker"),
		BankingZone->ZoneColourTag, ReservedGameplayColours::GetPurpleTag());
	if (APlaceholderTargetZoneActor* PlaceholderMarker = Cast<APlaceholderTargetZoneActor>(Marker))
	{
		const FLinearColor ExpectedColour = AbilityData::GetChainColourForEnemyType(EEnemyType::RU_NNR);
		TestTrue(TEXT("Marker's reflected chain colour should be Purple for a RU-NNR zone"),
			PlaceholderMarker->CurrentChainColour.Equals(ExpectedColour, 0.01f));
		TestTrue(TEXT("Marker's beacon light should render Purple for a RU-NNR zone"),
			PlaceholderMarker->BeaconLightComponent->GetLightColor().Equals(ExpectedColour, 0.01f));
		// Asserts the actual assigned mesh resource, not just derived/reflected state
		// (CurrentChainColour/beacon light above) - a dropped or misindexed
		// SetMaterial() call would leave a mesh silently un-tinted in-game while those
		// other assertions still pass, since all three are set independently in
		// ApplyChainColour(). One marker is sufficient: all four exercise the identical
		// SetMaterial() code path, so this is a completeness check on the mechanism,
		// not a per-colour check.
		TestTrue(TEXT("Beacon disc mesh should have the chain-colour MID actually assigned"),
			Cast<UMaterialInstanceDynamic>(PlaceholderMarker->BeaconMeshComponent->GetMaterial(0)) ==
				PlaceholderMarker->ChainColourMaterialInstance.Get());
		TestTrue(TEXT("Beacon column mesh should have the chain-colour MID actually assigned"),
			Cast<UMaterialInstanceDynamic>(PlaceholderMarker->BeaconColumnMeshComponent->GetMaterial(0)) ==
				PlaceholderMarker->ChainColourMaterialInstance.Get());

		// Banking-radius ring honesty (issue #365 REQ-3): radius must equal the zone's
		// own live-read extent, not a hardcoded literal; colour must equal the same
		// chain colour driving the pole/beacon above.
		TestTrue(TEXT("Marker's banking-radius ring should be visible"),
			PlaceholderMarker->BankingRadiusIndicatorComponent->bIsVisible);
		TestEqual(TEXT("Marker's banking-radius ring radius should equal the zone's real GetBankingRadiusUnits()"),
			PlaceholderMarker->BankingRadiusIndicatorComponent->CurrentShapeSpec.RangeUnits,
			BankingZone->GetBankingRadiusUnits());
		TestTrue(TEXT("Marker's banking-radius ring colour should be Purple for a RU-NNR zone"),
			PlaceholderMarker->BankingRadiusIndicatorComponent->CurrentColour.Equals(
				AbilityData::GetChainColourForEnemyType(EEnemyType::RU_NNR), 0.01f));
	}

	ATargetZone* SecondBankingZone = FindAttachedZone(SecondMarker);
	if (!TestNotNull(TEXT("Second marker should have a self-healed ATargetZone attached via BeginPlay()"), SecondBankingZone))
	{
		return false;
	}
	TestEqual(TEXT("Second banking zone should be colour-tagged Blue for an SN-1PR marker"),
		SecondBankingZone->ZoneColourTag, ReservedGameplayColours::GetBlueTag());
	if (APlaceholderTargetZoneActor* PlaceholderMarker = Cast<APlaceholderTargetZoneActor>(SecondMarker))
	{
		const FLinearColor ExpectedColour = AbilityData::GetChainColourForEnemyType(EEnemyType::SN_1PR);
		TestTrue(TEXT("Marker's reflected chain colour should be Blue for an SN-1PR zone"),
			PlaceholderMarker->CurrentChainColour.Equals(ExpectedColour, 0.01f));
		TestTrue(TEXT("Marker's beacon light should render Blue for an SN-1PR zone"),
			PlaceholderMarker->BeaconLightComponent->GetLightColor().Equals(ExpectedColour, 0.01f));
	}

	ATargetZone* TrooperBankingZone = FindAttachedZone(TrooperMarker);
	if (!TestNotNull(TEXT("Trooper marker should have a self-healed ATargetZone attached via BeginPlay()"), TrooperBankingZone))
	{
		return false;
	}
	TestEqual(TEXT("Trooper banking zone should be colour-tagged Teal for a TR_UPR marker"),
		TrooperBankingZone->ZoneColourTag, ReservedGameplayColours::GetTealTag());
	if (APlaceholderTargetZoneActor* PlaceholderMarker = Cast<APlaceholderTargetZoneActor>(TrooperMarker))
	{
		const FLinearColor ExpectedColour = AbilityData::GetChainColourForEnemyType(EEnemyType::TR_UPR);
		TestTrue(TEXT("Marker's reflected chain colour should be Teal for a TR_UPR zone"),
			PlaceholderMarker->CurrentChainColour.Equals(ExpectedColour, 0.01f));
		TestTrue(TEXT("Marker's beacon light should render Teal for a TR_UPR zone"),
			PlaceholderMarker->BeaconLightComponent->GetLightColor().Equals(ExpectedColour, 0.01f));
	}

	ATargetZone* FearBankingZone = FindAttachedZone(FearMarker);
	if (!TestNotNull(TEXT("Fear marker should have a self-healed ATargetZone attached via BeginPlay()"), FearBankingZone))
	{
		return false;
	}
	TestEqual(TEXT("Fear banking zone should be colour-tagged Orange for a B0_0MR marker"),
		FearBankingZone->ZoneColourTag, ReservedGameplayColours::GetOrangeTag());
	if (APlaceholderTargetZoneActor* PlaceholderMarker = Cast<APlaceholderTargetZoneActor>(FearMarker))
	{
		const FLinearColor ExpectedColour = AbilityData::GetChainColourForEnemyType(EEnemyType::B0_0MR);
		TestTrue(TEXT("Marker's reflected chain colour should be Orange for a B0_0MR zone"),
			PlaceholderMarker->CurrentChainColour.Equals(ExpectedColour, 0.01f));
		TestTrue(TEXT("Marker's beacon light should render Orange for a B0_0MR zone"),
			PlaceholderMarker->BeaconLightComponent->GetLightColor().Equals(ExpectedColour, 0.01f));
	}

	// Hard Invariant 3 guard: the pre-attached any-type zone's marker must never
	// receive one of the four locked types' reserved chain colours - proves
	// ApplyChainColourToMarker()'s bAcceptAnyEnemyType no-op guard actually reaches
	// this marker via the ExistingZone branch of EnsureBankingZonesWired(), the only
	// branch a hand-placed any-type zone can arrive through (the freshly-spawned
	// branch always hardcodes bAcceptAnyEnemyType = false).
	if (APlaceholderTargetZoneActor* AnyTypePlaceholder = Cast<APlaceholderTargetZoneActor>(AnyTypeMarker))
	{
		TestTrue(TEXT("An any-type zone's marker must never receive a reserved chain colour (Hard Invariant 3)"),
			AnyTypePlaceholder->CurrentChainColour.Equals(FLinearColor::Black, 0.01f));

		// The ring itself is still required for an any-type zone (issue #365) - only
		// its colour differs, using the neutral-chrome accessor instead of a reserved
		// chain colour (Hard Invariant 3). Proves the ring reaches this marker via the
		// ExistingZone branch too, the only branch a hand-placed any-type zone can
		// arrive through.
		TestTrue(TEXT("An any-type zone's marker must still get a visible banking-radius ring"),
			AnyTypePlaceholder->BankingRadiusIndicatorComponent->bIsVisible);
		TestEqual(TEXT("An any-type zone's ring radius should equal the zone's real GetBankingRadiusUnits()"),
			AnyTypePlaceholder->BankingRadiusIndicatorComponent->CurrentShapeSpec.RangeUnits,
			PreAttachedAnyTypeZone->GetBankingRadiusUnits());
		TestTrue(TEXT("An any-type zone's ring colour should be the neutral chrome, never a reserved colour (Hard Invariant 3)"),
			AnyTypePlaceholder->BankingRadiusIndicatorComponent->CurrentColour.Equals(
				ReservedGameplayColours::GetNeutralChrome(), 0.01f));
	}

	// Custom-MarkerClass guard: a designer-supplied non-placeholder marker must
	// survive EnsureBankingZonesWired() untouched (no crash/miscast) and still get
	// its own self-healed, correctly colour-tagged ATargetZone - proving
	// ApplyChainColourToMarker()'s Cast<APlaceholderTargetZoneActor> guard is a
	// pure no-op for this marker rather than an unchecked cast.
	TestFalse(TEXT("A non-APlaceholderTargetZoneActor marker must not be miscast by ApplyChainColourToMarker"),
		CustomClassMarker->IsA(APlaceholderTargetZoneActor::StaticClass()));
	ATargetZone* CustomClassZone = FindAttachedZone(CustomClassMarker);
	if (TestNotNull(TEXT("Custom-class marker should still get its self-healed ATargetZone"), CustomClassZone))
	{
		TestEqual(TEXT("Custom-class marker's zone should still be colour-tagged correctly (colour metadata is independent of visual application)"),
			CustomClassZone->ZoneColourTag, ReservedGameplayColours::GetBlueTag());
	}

	// Calling EnsureBankingZonesWired() a second time must not double-spawn, for
	// either marker.
	Room->EnsureBankingZonesWired();
	TestEqual(TEXT("EnsureBankingZonesWired should be idempotent - no duplicate zone"),
		CountAttachedZones(Marker), 1);
	TestEqual(TEXT("EnsureBankingZonesWired should be idempotent for the second marker too"),
		CountAttachedZones(SecondMarker), 1);
	if (APlaceholderTargetZoneActor* PlaceholderMarker = Cast<APlaceholderTargetZoneActor>(Marker))
	{
		TestTrue(TEXT("A repeated EnsureBankingZonesWired pass should not corrupt the marker's chain colour"),
			PlaceholderMarker->CurrentChainColour.Equals(
				AbilityData::GetChainColourForEnemyType(EEnemyType::RU_NNR), 0.01f));
	}

	// Type-keyed acceptance (operator ruling 2026-08-22): the zone is a pen for the
	// marker's EEnemyType, so the enemy's own type is what matters now - ARunnerEnemy
	// (RU-NNR) matches the first marker's zone. Controlled with STUN deliberately:
	// the colour-neutral starter ability must bank at a type-matched pen (MISSION
	// "each ability viable solo at base effectiveness"), the exact case the old
	// colour gate made impossible.
	// Offset kept well inside Room's own +-1000uu wall bounds (issue #243: the room's
	// perimeter is now real blocking geometry, not just visual - a +1000 offset used to
	// be harmless but now lands the enemy behind/inside the East wall itself, blocking
	// the sweep below before it ever reaches the zone).
	ARunnerEnemy* Enemy = World->SpawnActor<ARunnerEnemy>(
		BankingZone->GetActorLocation() + FVector(500.f, 0.f, 0.f), FRotator::ZeroRotator);
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
	// Offset kept inside Room's own wall bounds - same reasoning as Enemy's spawn
	// above; at the old +1000 offset this assertion passed for the wrong reason (the
	// enemy never even reached the zone, blocked by the wall instead of by type).
	ATrooperEnemy* MismatchedEnemy = World->SpawnActor<ATrooperEnemy>(
		BankingZone->GetActorLocation() + FVector(500.f, 500.f, 0.f), FRotator::ZeroRotator);
	if (TestNotNull(TEXT("Mismatched enemy should spawn"), MismatchedEnemy))
	{
		MismatchedEnemy->TickCheckDetection(MismatchedEnemy->GetActorLocation());
		MismatchedEnemy->ReceiveControl(EAbilitySlot::Root);
		MismatchedEnemy->SetActorLocation(BankingZone->GetActorLocation(), /*bSweep=*/true);
		TestEqual(TEXT("A wrong-type controlled enemy overlapping the zone should not bank (type-keyed)"),
			static_cast<uint8>(MismatchedEnemy->GetEnemyState()), static_cast<uint8>(EEnemyState::Controlled));
	}

	// An uncontrolled enemy physically overlapping the zone should not bank.
	// Offset kept inside Room's own wall bounds - same reasoning as Enemy's spawn above.
	ATrooperEnemy* UncontrolledEnemy = World->SpawnActor<ATrooperEnemy>(
		BankingZone->GetActorLocation() + FVector(500.f, -500.f, 0.f), FRotator::ZeroRotator);
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

	// Issue #253 acceptance criterion: a Fear-controlled enemy (the new flee-flavour
	// Controlled state, AEnemyBase::TickFleeMovement) still banks when physically
	// delivered onto its own B0_0MR-typed zone, proving the new movement flavour
	// doesn't disturb the herd/bank chain - same "physically deliver, don't simulate
	// herding" shape every other case in this file already uses (no herding primitive
	// exists yet).
	ABomberEnemy* BankableBomber = World->SpawnActor<ABomberEnemy>(
		FearBankingZone->GetActorLocation() + FVector(1000.f, 0.f, 0.f), FRotator::ZeroRotator);
	if (!TestNotNull(TEXT("Bankable bomber should spawn"), BankableBomber))
	{
		return false;
	}
	BankableBomber->TickCheckDetection(BankableBomber->GetActorLocation());
	BankableBomber->ReceiveControl(EAbilitySlot::Fear);
	TestEqual(TEXT("Bomber should be Controlled before overlapping its zone"),
		static_cast<uint8>(BankableBomber->GetEnemyState()), static_cast<uint8>(EEnemyState::Controlled));

	BankableBomber->SetActorLocation(FearBankingZone->GetActorLocation(), /*bSweep=*/true);

	TestEqual(TEXT("A Fear-controlled Bomber overlapping its own B0_0MR zone should reach Banked (issue #253)"),
		static_cast<uint8>(BankableBomber->GetEnemyState()), static_cast<uint8>(EEnemyState::Banked));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
