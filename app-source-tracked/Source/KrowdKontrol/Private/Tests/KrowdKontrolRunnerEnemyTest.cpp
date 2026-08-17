// Confirms ARunnerEnemy (issue #13, PRD 03) satisfies RU-NNR's acceptance criteria:
// a distinct elongated-cube "dart" silhouette, a Purple drain-glow that intensifies
// ONLY when Snare is the ability that triggered OnControlledEntry (no response for
// any other ability), a visible attack tell that precedes OnRunnerDrainFired (provably
// ordered), the tell fires exactly once per attack, and a fast
// GetMovementSpeedUnitsPerSecond() override genuinely wired into TickChaseMovement.
// Mirrors KrowdKontrolSniperEnemyTest.cpp's structure plus
// KrowdKontrolBomberEnemyTest.cpp's movement-speed-override cases (l2)/(l3).
//
// Uses NewObject rather than spawning into a UWorld for most cases: nothing exercised
// there calls GetWorld()/SpawnActor - AdvanceAttackTelegraph and TickCheckDetection/
// TickChaseMovement are all driven directly via friend access, never through a real
// Tick() loop. Case (m) is the one exception - it spawns into a real UWorld to prove
// the Tick() override itself is wired correctly.
//
// Case (j) is a structural proxy, same caveat as Sniper's/Bomber's own case (j).
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the other
// KrowdKontrol.Unit.* tests.

#include "Misc/AutomationTest.h"
#include "RunnerEnemy.h"
#include "ReservedGameplayColours.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Tests/AutomationEditorCommon.h"
#include "DrainRayFiredTestListener.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolRunnerEnemyTest,
	"KrowdKontrol.Unit.RunnerEnemy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolRunnerEnemyTest::RunTest(const FString& Parameters)
{
	ARunnerEnemy* Runner = NewObject<ARunnerEnemy>();
	if (!TestNotNull(TEXT("ARunnerEnemy should construct"), Runner))
	{
		return false;
	}

	// (a) distinct elongated-cube "dart" mesh, not the other 3 core enemy types' shapes.
	UStaticMeshComponent* Mesh = Runner->MeshComponent;
	if (!TestNotNull(TEXT("ARunnerEnemy should have a MeshComponent"), Mesh))
	{
		return false;
	}
	UStaticMesh* StaticMesh = Mesh->GetStaticMesh();
	if (!TestNotNull(TEXT("MeshComponent should have a static mesh assigned"), StaticMesh))
	{
		return false;
	}
	const FString MeshPath = StaticMesh->GetPathName();
	TestEqual(TEXT("MeshComponent should use the engine's cube mesh"),
		MeshPath, FString(TEXT("/Engine/BasicShapes/Cube.Cube")));
	TestNotEqual(TEXT("MeshComponent should not collide with the sniper's cone mesh"),
		MeshPath, FString(TEXT("/Engine/BasicShapes/Cone.Cone")));
	TestNotEqual(TEXT("MeshComponent should not collide with the bomber's sphere mesh"),
		MeshPath, FString(TEXT("/Engine/BasicShapes/Sphere.Sphere")));
	TestNotEqual(TEXT("MeshComponent should not collide with the trooper's plane mesh"),
		MeshPath, FString(TEXT("/Engine/BasicShapes/Plane.Plane")));
	TestTrue(TEXT("MeshComponent should use the distinct dart silhouette scale"),
		Mesh->GetRelativeScale3D().Equals(FVector(1.8f, 0.6f, 0.6f), 0.01f));

	// (b) Purple drain-glow at baseline intensity, un-intensified at construction.
	UPointLightComponent* DrainGlow = Runner->DrainGlowLightComponent;
	if (!TestNotNull(TEXT("ARunnerEnemy should have a DrainGlowLightComponent"), DrainGlow))
	{
		return false;
	}
	TestTrue(TEXT("Drain glow colour should be the reserved Purple"),
		DrainGlow->GetLightColor().Equals(ReservedGameplayColours::GetPurple(), 0.01f));
	TestEqual(TEXT("Drain glow should start at baseline intensity"),
		DrainGlow->Intensity, Runner->DrainGlowBaselineIntensity);
	TestTrue(TEXT("DrainGlowLightComponent should be attached to MeshComponent"),
		DrainGlow->GetAttachParent() == Mesh);
	TestEqual(TEXT("DrainGlowLightComponent attenuation radius should match the placeholder value"),
		DrainGlow->AttenuationRadius, 300.0f);

	// Drives a runner from Idle straight through to Attack via two zero/mid-distance
	// detection checks (Idle -> Alert -> Attack) - shared by every case below that
	// needs a runner already in Attack.
	auto AdvanceToAttack = [](ARunnerEnemy* TargetRunner, const FVector& PlayerLocation)
	{
		TargetRunner->TickCheckDetection(PlayerLocation); // Idle -> Alert
		TargetRunner->TickCheckDetection(PlayerLocation); // Alert -> Attack
	};

	// (c) advance to Attack, then Snare specifically intensifies the glow.
	const FVector ZeroDistanceLocation(0.0f, 0.0f, 0.0f);
	AdvanceToAttack(Runner, ZeroDistanceLocation);
	TestEqual(TEXT("Runner should be in Attack after two zero-distance detection checks"),
		static_cast<uint8>(Runner->GetEnemyState()), static_cast<uint8>(EEnemyState::Attack));
	Runner->ReceiveControl(EAbilitySlot::Snare);
	TestEqual(TEXT("Drain glow should intensify after Snare-triggered OnControlledEntry"),
		DrainGlow->Intensity, Runner->DrainGlowIntensifiedIntensity);

	// (d) every non-Snare ability produces no glow response at all, each on its own
	// fresh actor - exhaustive over EAbilitySlot rather than a single representative
	// value, since the guard could someday become per-ability instead of a single
	// equality check.
	const EAbilitySlot NonSnareAbilities[] = {
		EAbilitySlot::Stun, EAbilitySlot::Sleep, EAbilitySlot::Root, EAbilitySlot::Fear };
	for (EAbilitySlot NonSnareAbility : NonSnareAbilities)
	{
		ARunnerEnemy* NonSnareRunner = NewObject<ARunnerEnemy>();
		AdvanceToAttack(NonSnareRunner, ZeroDistanceLocation);
		NonSnareRunner->ReceiveControl(NonSnareAbility);
		TestEqual(TEXT("Drain glow should stay at baseline intensity for a non-Snare ability"),
			NonSnareRunner->DrainGlowLightComponent->Intensity, NonSnareRunner->DrainGlowBaselineIntensity);
	}

	// (e)/(f) the attack tell is off until Attack is entered, and visibly on
	// (before the drain-ray fires) once it is - ordering proven explicitly below.
	ARunnerEnemy* TellRunner = NewObject<ARunnerEnemy>();
	UPointLightComponent* TellLight = TellRunner->AttackTellLightComponent;
	if (!TestNotNull(TEXT("ARunnerEnemy should have an AttackTellLightComponent"), TellLight))
	{
		return false;
	}
	TestEqual(TEXT("Attack tell should be off before Attack is entered"), TellLight->Intensity, 0.0f);
	TestTrue(TEXT("AttackTellLightComponent should be attached to MeshComponent"),
		TellLight->GetAttachParent() == TellRunner->MeshComponent);
	TestEqual(TEXT("AttackTellLightComponent attenuation radius should match the placeholder value"),
		TellLight->AttenuationRadius, 300.0f);

	AdvanceToAttack(TellRunner, ZeroDistanceLocation);
	TestTrue(TEXT("Attack tell should be visibly on once Attack is entered"), TellLight->Intensity > 0.0f);

	UDrainRayFiredTestListener* DrainListener = NewObject<UDrainRayFiredTestListener>();
	TellRunner->OnRunnerDrainFired.AddDynamic(DrainListener, &UDrainRayFiredTestListener::HandleDrainRayFired);
	TestEqual(TEXT("The drain-ray should not have fired yet - tell precedes the ray"), DrainListener->CallCount, 0);

	// (g) the telegraph elapsing fires the drain-ray exactly once.
	TellRunner->AdvanceAttackTelegraph(TellRunner->AttackTelegraphSeconds);
	TestEqual(TEXT("OnRunnerDrainFired should have fired exactly once"), DrainListener->CallCount, 1);

	// (h) advancing again after the ray fired does not re-fire it.
	TellRunner->AdvanceAttackTelegraph(1.0f);
	TestEqual(TEXT("OnRunnerDrainFired should not re-fire on a later telegraph advance"),
		DrainListener->CallCount, 1);

	// (g2) the telegraph accumulates across multiple partial advances, matching how
	// the real per-frame Tick() drives this - not just a single full-duration jump.
	// The final partial advance overshoots AttackTelegraphSeconds (0.6f) by a
	// float32-rounding-safe margin (0.25 + 0.2 + 0.25 = 0.7, not the boundary-exact
	// 0.6) - landing exactly on the boundary risks a nonzero IEEE-754 residual
	// (PR #117 review, issue #17).
	ARunnerEnemy* AccumulatingRunner = NewObject<ARunnerEnemy>();
	AdvanceToAttack(AccumulatingRunner, ZeroDistanceLocation);
	UDrainRayFiredTestListener* AccumulatingListener = NewObject<UDrainRayFiredTestListener>();
	AccumulatingRunner->OnRunnerDrainFired.AddDynamic(AccumulatingListener, &UDrainRayFiredTestListener::HandleDrainRayFired);

	AccumulatingRunner->AdvanceAttackTelegraph(0.25f);
	AccumulatingRunner->AdvanceAttackTelegraph(0.2f);
	TestEqual(TEXT("Partial advances summing to below AttackTelegraphSeconds should not fire yet"),
		AccumulatingListener->CallCount, 0);

	AccumulatingRunner->AdvanceAttackTelegraph(0.25f); // 0.25 + 0.2 + 0.25 = 0.7, safely past 0.6
	TestEqual(TEXT("The telegraph should fire once the accumulated partial advances cross AttackTelegraphSeconds"),
		AccumulatingListener->CallCount, 1);

	// (i) advancing the telegraph while not in Attack is a no-op.
	ARunnerEnemy* IdleRunner = NewObject<ARunnerEnemy>();
	UDrainRayFiredTestListener* IdleListener = NewObject<UDrainRayFiredTestListener>();
	IdleRunner->OnRunnerDrainFired.AddDynamic(IdleListener, &UDrainRayFiredTestListener::HandleDrainRayFired);
	IdleRunner->AdvanceAttackTelegraph(999.0f);
	TestEqual(TEXT("AdvanceAttackTelegraph should be a no-op while not in Attack"),
		IdleListener->CallCount, 0);

	// (j) short attack range: a mid-range distance (well within DetectionRangeUnits)
	// does NOT reach Attack - the same shape as ABomberEnemy's own structural-proxy case.
	ARunnerEnemy* ShortRangeRunner = NewObject<ARunnerEnemy>();
	TestEqual(TEXT("GetAttackRangeUnits() should return RU-NNR's short-range value"),
		ShortRangeRunner->GetAttackRangeUnits(), 220.0f);
	const FVector MidRangeLocation(800.0f, 0.0f, 0.0f);
	AdvanceToAttack(ShortRangeRunner, MidRangeLocation); // Alert reached (800 <= 1500), Attack not (800 > 220)
	TestEqual(TEXT("Runner's short attack range should not reach Attack at mid-range"),
		static_cast<uint8>(ShortRangeRunner->GetEnemyState()), static_cast<uint8>(EEnemyState::Alert));

	// (k) the attack tell colour must not collide with any of MISSION.md Hard
	// Invariant 3's five reserved gameplay-information colours - the placeholder was
	// hand-verified against ReservedGameplayColours.cpp's current values, but this
	// assertion is the automated backstop against a future colour tweak silently
	// reusing one of them.
	TestFalse(TEXT("Attack tell colour should not collide with a reserved gameplay-information colour"),
		ReservedGameplayColours::GetAll().ContainsByPredicate(
			[TellLight](const FLinearColor& Reserved) { return Reserved.Equals(TellLight->GetLightColor(), 0.01f); }));

	// (l) ReceiveControl interrupting an in-progress attack telegraph clears the tell
	// light, so a runner put to Snare mid-telegraph doesn't keep showing a drain-ray
	// that will never fire (the state guard in AdvanceAttackTelegraph already stops
	// the ray itself - this proves the visual is cleared too).
	ARunnerEnemy* InterruptedRunner = NewObject<ARunnerEnemy>();
	AdvanceToAttack(InterruptedRunner, ZeroDistanceLocation);
	TestTrue(TEXT("Attack tell should be visibly on before the interrupt"),
		InterruptedRunner->AttackTellLightComponent->Intensity > 0.0f);
	InterruptedRunner->ReceiveControl(EAbilitySlot::Snare);
	TestEqual(TEXT("Attack tell should be cleared once Controlled interrupts the attack"),
		InterruptedRunner->AttackTellLightComponent->Intensity, 0.0f);
	UDrainRayFiredTestListener* InterruptedListener = NewObject<UDrainRayFiredTestListener>();
	InterruptedRunner->OnRunnerDrainFired.AddDynamic(InterruptedListener, &UDrainRayFiredTestListener::HandleDrainRayFired);
	InterruptedRunner->AdvanceAttackTelegraph(InterruptedRunner->AttackTelegraphSeconds);
	TestEqual(TEXT("The interrupted drain-ray should never fire"), InterruptedListener->CallCount, 0);

	// (l2) GetMovementSpeedUnitsPerSecond() override returns the declared
	// MovementSpeed (950.0f), not AEnemyBase's own base default (600.0f) - the direct
	// proof that issue #122's "per-type speeds are actually read" holds for RU-NNR too.
	ARunnerEnemy* SpeedRunner = NewObject<ARunnerEnemy>();
	TestEqual(TEXT("Runner's movement speed override returns its declared MovementSpeed"),
		SpeedRunner->GetMovementSpeedUnitsPerSecond(), SpeedRunner->MovementSpeed);
	TestEqual(TEXT("Runner's declared MovementSpeed is 950.0f"), SpeedRunner->MovementSpeed, 950.0f);
	TestNotEqual(TEXT("Runner's movement speed differs from AEnemyBase's base default"),
		SpeedRunner->GetMovementSpeedUnitsPerSecond(), 600.0f);

	// (l3) driving TickChaseMovement (friend-accessible, same as TickCheckDetection)
	// during Alert advances the runner at exactly its own 950 u/s, not 600 u/s -
	// proves the override is genuinely consulted by the tick step, not just present.
	ARunnerEnemy* ChasingRunner = NewObject<ARunnerEnemy>();
	const FVector FarPlayerLocation(1000.0f, 0.0f, 0.0f);
	ChasingRunner->TickCheckDetection(FarPlayerLocation); // Idle -> Alert (1000 <= 1500)
	TestEqual(TEXT("precondition: runner is Alert"),
		static_cast<uint8>(ChasingRunner->GetEnemyState()), static_cast<uint8>(EEnemyState::Alert));
	const FVector BeforeChase = ChasingRunner->GetActorLocation();
	ChasingRunner->TickChaseMovement(FarPlayerLocation, 0.5f);
	const float DistanceMoved = FVector::Dist(ChasingRunner->GetActorLocation(), BeforeChase);
	TestEqual(TEXT("runner chase advances at its own MovementSpeed * DeltaSeconds"),
		DistanceMoved, ChasingRunner->MovementSpeed * 0.5f);

	// (m) the real Tick() override, not just the friend-called AdvanceAttackTelegraph
	// helper, must wire the telegraph into the per-frame loop - proves neither a
	// missing Super::Tick(DeltaTime) nor a wiring mistake in the override itself,
	// mirroring KrowdKontrolSniperEnemyTest.cpp case (m).
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	if (TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
	{
		ARunnerEnemy* TickedRunner = World->SpawnActor<ARunnerEnemy>();
		if (TestNotNull(TEXT("ARunnerEnemy should spawn into the test World"), TickedRunner))
		{
			AdvanceToAttack(TickedRunner, ZeroDistanceLocation);
			UDrainRayFiredTestListener* TickedListener = NewObject<UDrainRayFiredTestListener>();
			TickedRunner->OnRunnerDrainFired.AddDynamic(TickedListener, &UDrainRayFiredTestListener::HandleDrainRayFired);
			TickedRunner->Tick(TickedRunner->AttackTelegraphSeconds);
			TestEqual(TEXT("Tick() should drive the telegraph through to firing the drain-ray"),
				TickedListener->CallCount, 1);
		}
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
