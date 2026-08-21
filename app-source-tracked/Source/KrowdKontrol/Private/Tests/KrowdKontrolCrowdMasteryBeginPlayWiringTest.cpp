// Confirms the production BeginPlay wiring for Crowd Mastery (issue #174 AC1,
// docs/prd-run-lifecycle.md REQ-5) actually binds when actors go through the real
// BeginPlay path: UAbilityCastComponent::BeginPlay must AddDynamic
// UCrowdMasterySubsystem::HandleAbilityCastApplied onto OnAbilityCastApplied, and
// AEnemyBase::BeginPlay must AddDynamic HandleEnemyControlledExpired onto
// OnEnemyControlledExpired. KrowdKontrolCrowdMasterySubsystemTest.cpp exercises the
// subsystem's handlers directly (bypassing BeginPlay by design, for synchronous
// determinism); this test covers the complementary gap its pass-2 code review
// flagged — nothing exercised the bindings through DispatchBeginPlay(), so a
// regression that deleted either AddDynamic call would have kept the whole suite
// green. DispatchBeginPlay() is required here because CreateNewMap() worlds never
// begin play on their own — see KrowdKontrolWaveSpawnerComponentTest.cpp's case (7)
// comment for the incident that established that pattern.
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the other
// KrowdKontrol.Unit.* tests.

#include "Misc/AutomationTest.h"
#include "CrowdMasterySubsystem.h"
#include "AbilityCastComponent.h"
#include "EnemyBaseTestActor.h"
#include "AbilitySlot.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolCrowdMasteryBeginPlayWiringTest,
	"KrowdKontrol.Unit.CrowdMasteryBeginPlayWiring",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolCrowdMasteryBeginPlayWiringTest::RunTest(const FString& Parameters)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	if (!TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
	{
		return false;
	}

	UCrowdMasterySubsystem* Subsystem = World->GetSubsystem<UCrowdMasterySubsystem>();
	if (!TestNotNull(TEXT("UWorld should auto-instantiate UCrowdMasterySubsystem"), Subsystem))
	{
		return false;
	}

	// (a) UAbilityCastComponent::BeginPlay binds HandleAbilityCastApplied. The
	// component is registered on a plain actor and driven through the real
	// DispatchBeginPlay() path, exactly as a possessed pawn's component would be.
	AActor* CasterActor = World->SpawnActor<AActor>();
	if (!TestNotNull(TEXT("Plain AActor should spawn into the test World"), CasterActor))
	{
		return false;
	}
	UAbilityCastComponent* CastComponent =
		NewObject<UAbilityCastComponent>(CasterActor, TEXT("WiringTestCastComponent"));
	CastComponent->RegisterComponent();
	CasterActor->DispatchBeginPlay();

	TestTrue(TEXT("BeginPlay should bind UCrowdMasterySubsystem::HandleAbilityCastApplied to OnAbilityCastApplied (issue #174 AC1)"),
		CastComponent->OnAbilityCastApplied.Contains(Subsystem,
			GET_FUNCTION_NAME_CHECKED(UCrowdMasterySubsystem, HandleAbilityCastApplied)));

	// (b) The binding transmits: a broadcast through the wired delegate — with one
	// genuinely Controlled enemy alive — must move the subsystem's running max to 1,
	// proving the AddDynamic produced a live connection, not just a table entry.
	AEnemyBaseTestActor* Enemy = World->SpawnActor<AEnemyBaseTestActor>();
	if (!TestNotNull(TEXT("AEnemyBaseTestActor should spawn into the test World"), Enemy))
	{
		return false;
	}
	const FVector ZeroDistanceLocation(0.0f, 0.0f, 0.0f);
	Enemy->TickCheckDetection(ZeroDistanceLocation); // Idle -> Alert
	Enemy->ReceiveControl(EAbilitySlot::Stun);        // Alert -> Controlled
	CastComponent->OnAbilityCastApplied.Broadcast(EAbilitySlot::Stun, Enemy);

	TestEqual(TEXT("Broadcast through the BeginPlay-wired delegate should sample the running max to 1"),
		Subsystem->GetRunningMaxControlledCount(), 1);

	// (c) AEnemyBase::BeginPlay binds HandleEnemyControlledExpired, through the same
	// real DispatchBeginPlay() path.
	AEnemyBaseTestActor* BegunEnemy = World->SpawnActor<AEnemyBaseTestActor>();
	if (!TestNotNull(TEXT("AEnemyBaseTestActor should spawn into the test World"), BegunEnemy))
	{
		return false;
	}
	BegunEnemy->DispatchBeginPlay();

	TestTrue(TEXT("BeginPlay should bind UCrowdMasterySubsystem::HandleEnemyControlledExpired to OnEnemyControlledExpired (issue #174 AC1)"),
		BegunEnemy->OnEnemyControlledExpired.Contains(Subsystem,
			GET_FUNCTION_NAME_CHECKED(UCrowdMasterySubsystem, HandleEnemyControlledExpired)));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
