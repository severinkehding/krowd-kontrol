// Confirms UFirstStunBeaconComponent (issue #29, PRD 09 REQ-2): the first time
// UAbilityCastComponent::OnAbilityCastApplied fires with EAbilitySlot::Stun, the
// nearest APlaceholderTargetZoneActor's beacon is intensified exactly once -
// subsequent Stun casts (second and later) must not re-trigger it, and a non-Stun
// ability cast must never trigger it at all.
//
// The no-zone-in-world case (a) runs against its own CreateNewMap() World, before the
// shared World used by every later case is created - FAutomationEditorCommonUtils::
// CreateNewMap() tears down whatever World preceded it (confirmed via
// UWorld::CleanupWorld in the log when this test originally called it a second time
// mid-run), so any actor pointer captured before a later CreateNewMap() call would be
// left dangling. Only ever call CreateNewMap() once more after this point.
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the other
// KrowdKontrol.Unit.* tests.

#include "Misc/AutomationTest.h"
#include "FirstStunBeaconComponent.h"
#include "PlaceholderTargetZoneActor.h"
#include "Components/PointLightComponent.h"
#include "AbilityCastComponent.h"
#include "AbilityUnlockComponent.h"
#include "AbilityCooldownComponent.h"
#include "EnemyBaseTestActor.h"
#include "FlatCamera3DPrototypePawn.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolFirstStunBeaconComponentTest,
	"KrowdKontrol.Unit.FirstStunBeaconComponent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolFirstStunBeaconComponentTest::RunTest(const FString& Parameters)
{
	// (a) No APlaceholderTargetZoneActor in the world: HandleAbilityCastApplied must
	// not crash - reaching the assertion below is itself the proof. Run first, against
	// its own empty World, before any target zone exists anywhere in this test.
	{
		UWorld* EmptyWorld = FAutomationEditorCommonUtils::CreateNewMap();
		if (!TestNotNull(TEXT("CreateNewMap should return a valid empty World"), EmptyWorld))
		{
			return false;
		}
		APawn* EmptyWorldOwner = EmptyWorld->SpawnActor<APawn>();
		if (!TestNotNull(TEXT("APawn should spawn into the empty test World"), EmptyWorldOwner))
		{
			return false;
		}
		UFirstStunBeaconComponent* EmptyWorldBeaconComponent = NewObject<UFirstStunBeaconComponent>(EmptyWorldOwner);
		EmptyWorldBeaconComponent->RegisterComponent();
		EmptyWorldBeaconComponent->HandleAbilityCastApplied(EAbilitySlot::Stun, nullptr);
		TestTrue(TEXT("HandleAbilityCastApplied must not crash when no APlaceholderTargetZoneActor exists"), true);

		// A no-zone miss on the first successful Stun must burn the one-shot guard
		// permanently, not leave it retryable - see FirstStunBeaconComponent.cpp's
		// "Set before attempting the zone lookup" comment. A zone appearing afterward
		// (e.g. late level streaming) must never retroactively intensify.
		APlaceholderTargetZoneActor* LateZone = EmptyWorld->SpawnActor<APlaceholderTargetZoneActor>();
		if (!TestNotNull(TEXT("A late-spawned APlaceholderTargetZoneActor should spawn into the empty test World"), LateZone))
		{
			return false;
		}
		EmptyWorldBeaconComponent->HandleAbilityCastApplied(EAbilitySlot::Stun, nullptr);
		TestEqual(TEXT("A zone spawned after a no-zone miss must not be retroactively intensified"),
			LateZone->BeaconLightComponent->Intensity, LateZone->BeaconBaselineIntensity);
	}

	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
	{
		return false;
	}

	APawn* Owner = World->SpawnActor<APawn>();
	if (!TestNotNull(TEXT("APawn should spawn into the test World"), Owner))
	{
		return false;
	}
	Owner->SetActorLocation(FVector::ZeroVector);

	UAbilityUnlockComponent* UnlockComponent = NewObject<UAbilityUnlockComponent>(Owner);
	UnlockComponent->RegisterComponent();
	UAbilityCooldownComponent* CooldownComponent = NewObject<UAbilityCooldownComponent>(Owner);
	CooldownComponent->RegisterComponent();
	UAbilityCastComponent* CastComponent = NewObject<UAbilityCastComponent>(Owner);
	CastComponent->RegisterComponent();

	UFirstStunBeaconComponent* BeaconComponent = NewObject<UFirstStunBeaconComponent>(Owner);
	BeaconComponent->RegisterComponent();
	CastComponent->OnAbilityCastApplied.AddDynamic(BeaconComponent, &UFirstStunBeaconComponent::HandleAbilityCastApplied);

	// (b) Two target zones at different distances from Owner - the nearer one must be
	// the one intensified.
	APlaceholderTargetZoneActor* NearZone = World->SpawnActor<APlaceholderTargetZoneActor>();
	if (!TestNotNull(TEXT("Near APlaceholderTargetZoneActor should spawn into the test World"), NearZone))
	{
		return false;
	}
	NearZone->SetActorLocation(FVector(100.0f, 0.0f, 0.0f));

	APlaceholderTargetZoneActor* FarZone = World->SpawnActor<APlaceholderTargetZoneActor>();
	if (!TestNotNull(TEXT("Far APlaceholderTargetZoneActor should spawn into the test World"), FarZone))
	{
		return false;
	}
	FarZone->SetActorLocation(FVector(1000.0f, 0.0f, 0.0f));

	AEnemyBaseTestActor* Enemy = World->SpawnActor<AEnemyBaseTestActor>();
	if (!TestNotNull(TEXT("AEnemyBaseTestActor should spawn into the test World"), Enemy))
	{
		return false;
	}
	Enemy->TickCheckDetection(FVector::ZeroVector); // Idle -> Alert

	const bool bFirstCastResult = CastComponent->TryCastAbility(EAbilitySlot::Stun);
	TestTrue(TEXT("TryCastAbility(Stun) should succeed against an eligible in-range enemy"), bFirstCastResult);
	TestEqual(TEXT("The nearer zone's beacon should intensify on the first successful Stun cast"),
		NearZone->BeaconLightComponent->Intensity, NearZone->BeaconIntensifiedIntensity);
	TestEqual(TEXT("The farther zone's beacon should remain at baseline intensity"),
		FarZone->BeaconLightComponent->Intensity, FarZone->BeaconBaselineIntensity);

	// (c) Simulated further Stun casts must not re-trigger the beacon - same rationale
	// as KrowdKontrolGizmoFirstContactComponentTest.cpp's case (b): the cooldown gate
	// is already covered independently by KrowdKontrolAbilityCastComponentTest.cpp.
	BeaconComponent->HandleAbilityCastApplied(EAbilitySlot::Stun, Enemy);
	TestEqual(TEXT("A second simulated Stun cast must not change the near zone's intensity"),
		NearZone->BeaconLightComponent->Intensity, NearZone->BeaconIntensifiedIntensity);
	TestEqual(TEXT("A second simulated Stun cast must not intensify the far zone"),
		FarZone->BeaconLightComponent->Intensity, FarZone->BeaconBaselineIntensity);

	BeaconComponent->HandleAbilityCastApplied(EAbilitySlot::Stun, Enemy);
	TestEqual(TEXT("A third simulated Stun cast must not intensify the far zone"),
		FarZone->BeaconLightComponent->Intensity, FarZone->BeaconBaselineIntensity);

	// (d) A non-Stun ability cast must never trigger the beacon, even before any Stun
	// cast has happened.
	APawn* SecondOwner = World->SpawnActor<APawn>();
	if (!TestNotNull(TEXT("A second APawn should spawn into the test World"), SecondOwner))
	{
		return false;
	}
	SecondOwner->SetActorLocation(FVector::ZeroVector);
	UFirstStunBeaconComponent* SecondBeaconComponent = NewObject<UFirstStunBeaconComponent>(SecondOwner);
	SecondBeaconComponent->RegisterComponent();

	APlaceholderTargetZoneActor* NonStunZone = World->SpawnActor<APlaceholderTargetZoneActor>();
	if (!TestNotNull(TEXT("A third APlaceholderTargetZoneActor should spawn into the test World"), NonStunZone))
	{
		return false;
	}
	NonStunZone->SetActorLocation(FVector(50.0f, 0.0f, 0.0f));

	SecondBeaconComponent->HandleAbilityCastApplied(EAbilitySlot::Sleep, Enemy);
	TestEqual(TEXT("A non-Stun ability cast must never intensify any beacon"),
		NonStunZone->BeaconLightComponent->Intensity, NonStunZone->BeaconBaselineIntensity);

	// (e) Real pawn-constructor wiring: AFlatCamera3DPrototypePawn's constructor binds
	// AbilityCastComponent->OnAbilityCastApplied to its own FirstStunBeaconComponent -
	// a copy-paste slip there would compile cleanly and every other case above would
	// still pass, since none of them go through the real pawn.
	AFlatCamera3DPrototypePawn* WiringPawn = World->SpawnActor<AFlatCamera3DPrototypePawn>();
	if (!TestNotNull(TEXT("AFlatCamera3DPrototypePawn should spawn into the test World"), WiringPawn))
	{
		return false;
	}
	if (!TestNotNull(TEXT("The real pawn's FirstStunBeaconComponent should be constructed"),
		ToRawPtr(WiringPawn->FirstStunBeaconComponent)))
	{
		return false;
	}
	WiringPawn->SetActorLocation(FVector(2000.0f, 0.0f, 0.0f));

	APlaceholderTargetZoneActor* WiringZone = World->SpawnActor<APlaceholderTargetZoneActor>();
	if (!TestNotNull(TEXT("A wiring-test APlaceholderTargetZoneActor should spawn into the test World"), WiringZone))
	{
		return false;
	}
	WiringZone->SetActorLocation(FVector(2100.0f, 0.0f, 0.0f));

	AEnemyBaseTestActor* WiringEnemy = World->SpawnActor<AEnemyBaseTestActor>();
	if (!TestNotNull(TEXT("A wiring-test AEnemyBaseTestActor should spawn into the test World"), WiringEnemy))
	{
		return false;
	}
	WiringEnemy->SetActorLocation(FVector(2000.0f, 0.0f, 0.0f));
	WiringEnemy->TickCheckDetection(WiringPawn->GetActorLocation()); // Idle -> Alert

	const bool bWiringCastResult = WiringPawn->AbilityCastComponent->TryCastAbility(EAbilitySlot::Stun);
	TestTrue(TEXT("TryCastAbility(Stun) should succeed against an eligible in-range enemy via the real pawn"),
		bWiringCastResult);
	TestEqual(TEXT("The pawn's real constructor-time AddDynamic binding must reach FirstStunBeaconComponent"),
		WiringZone->BeaconLightComponent->Intensity, WiringZone->BeaconIntensifiedIntensity);

	// (f) Two target zones at an exact distance tie from Owner: FindNearestTargetZone()
	// uses strict less-than, so exactly one of the tied zones wins deterministically -
	// but which one depends on TActorIterator's iteration order, which is not
	// documented to match spawn order (confirmed empirically: it does not always).
	// Pin the actual contract - exactly one zone wins, never both and never neither -
	// rather than asserting a specific zone that iteration order doesn't guarantee.
	// TieOwner is a bare APawn with no RootComponent, same as Owner/SecondOwner above,
	// so SetActorLocation() on it is a no-op and it stays at the World origin - the
	// tied zones are placed closer to the origin than every other zone spawned above
	// (nearest of those, NonStunZone, is 50 units out) so this case's tie is the only
	// one FindNearestTargetZone() can find.
	APawn* TieOwner = World->SpawnActor<APawn>();
	if (!TestNotNull(TEXT("A tie-test APawn should spawn into the test World"), TieOwner))
	{
		return false;
	}
	TieOwner->SetActorLocation(FVector::ZeroVector);
	UFirstStunBeaconComponent* TieBeaconComponent = NewObject<UFirstStunBeaconComponent>(TieOwner);
	TieBeaconComponent->RegisterComponent();

	APlaceholderTargetZoneActor* TiedZoneA = World->SpawnActor<APlaceholderTargetZoneActor>();
	if (!TestNotNull(TEXT("First tied APlaceholderTargetZoneActor should spawn into the test World"), TiedZoneA))
	{
		return false;
	}
	TiedZoneA->SetActorLocation(FVector(10.0f, 0.0f, 0.0f));

	APlaceholderTargetZoneActor* TiedZoneB = World->SpawnActor<APlaceholderTargetZoneActor>();
	if (!TestNotNull(TEXT("Second tied APlaceholderTargetZoneActor should spawn into the test World"), TiedZoneB))
	{
		return false;
	}
	TiedZoneB->SetActorLocation(FVector(0.0f, 10.0f, 0.0f)); // same distance from TieOwner as TiedZoneA

	TieBeaconComponent->HandleAbilityCastApplied(EAbilitySlot::Stun, nullptr);
	const bool bTiedZoneAIntensified = FMath::IsNearlyEqual(
		TiedZoneA->BeaconLightComponent->Intensity, TiedZoneA->BeaconIntensifiedIntensity);
	const bool bTiedZoneBIntensified = FMath::IsNearlyEqual(
		TiedZoneB->BeaconLightComponent->Intensity, TiedZoneB->BeaconIntensifiedIntensity);
	TestTrue(TEXT("On an exact distance tie, exactly one zone should be intensified, never both or neither"),
		bTiedZoneAIntensified != bTiedZoneBIntensified);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
