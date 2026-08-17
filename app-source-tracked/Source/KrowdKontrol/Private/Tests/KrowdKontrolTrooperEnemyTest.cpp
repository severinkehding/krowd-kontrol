// Confirms ATrooperEnemy (issue #14, PRD 03) satisfies TR-UPR's acceptance criteria:
// a distinct Plane silhouette, a Teal glow that intensifies ONLY when Root is the
// ability that triggered OnControlledEntry (no response for any other ability), a
// visible attack tell that precedes OnTrooperRayFired (provably ordered), a medium
// GetAttackRangeUnits() strictly between ABomberEnemy's short and ASniperEnemy's long
// values, and - the one genuinely new behavior in this class - AdvanceAttackTelegraph
// re-arming itself after each ray fires instead of latching a fire-once guard like
// both siblings, so it keeps firing repeatedly for as long as Attack persists.
//
// Mirrors KrowdKontrolSniperEnemyTest.cpp/KrowdKontrolBomberEnemyTest.cpp's structure
// (NewObject + friend access for most cases, a real UWorld only for (m)).

#include "Misc/AutomationTest.h"
#include "TrooperEnemy.h"
#include "ReservedGameplayColours.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Tests/AutomationEditorCommon.h"
#include "TrooperRayFiredTestListener.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolTrooperEnemyTest,
	"KrowdKontrol.Unit.TrooperEnemy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolTrooperEnemyTest::RunTest(const FString& Parameters)
{
	ATrooperEnemy* Trooper = NewObject<ATrooperEnemy>();
	if (!TestNotNull(TEXT("ATrooperEnemy should construct"), Trooper))
	{
		return false;
	}

	// (a) distinct Plane mesh, not the shapes other placeholder/enemy actors already use.
	UStaticMeshComponent* Mesh = Trooper->MeshComponent;
	if (!TestNotNull(TEXT("ATrooperEnemy should have a MeshComponent"), Mesh))
	{
		return false;
	}
	UStaticMesh* StaticMesh = Mesh->GetStaticMesh();
	if (!TestNotNull(TEXT("MeshComponent should have a static mesh assigned"), StaticMesh))
	{
		return false;
	}
	const FString MeshPath = StaticMesh->GetPathName();
	TestEqual(TEXT("MeshComponent should use the engine's plane mesh"),
		MeshPath, FString(TEXT("/Engine/BasicShapes/Plane.Plane")));
	TestNotEqual(TEXT("MeshComponent should not collide with the cube placeholder's mesh"),
		MeshPath, FString(TEXT("/Engine/BasicShapes/Cube.Cube")));
	TestNotEqual(TEXT("MeshComponent should not collide with the target zone's cylinder mesh"),
		MeshPath, FString(TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));
	TestNotEqual(TEXT("MeshComponent should not collide with the sniper's cone mesh"),
		MeshPath, FString(TEXT("/Engine/BasicShapes/Cone.Cone")));
	TestNotEqual(TEXT("MeshComponent should not collide with the bomber's sphere mesh"),
		MeshPath, FString(TEXT("/Engine/BasicShapes/Sphere.Sphere")));
	TestTrue(TEXT("MeshComponent should use the distinct standing-panel silhouette scale"),
		Mesh->GetRelativeScale3D().Equals(FVector(1.2f, 0.15f, 1.6f), 0.01f));

	// (b) Teal glow at baseline intensity, un-intensified at construction.
	UPointLightComponent* Glow = Trooper->GlowLightComponent;
	if (!TestNotNull(TEXT("ATrooperEnemy should have a GlowLightComponent"), Glow))
	{
		return false;
	}
	TestTrue(TEXT("Glow colour should be the reserved Teal"),
		Glow->GetLightColor().Equals(ReservedGameplayColours::GetTeal(), 0.01f));
	TestEqual(TEXT("Glow should start at baseline intensity"),
		Glow->Intensity, Trooper->GlowBaselineIntensity);
	TestTrue(TEXT("GlowLightComponent should be attached to MeshComponent"),
		Glow->GetAttachParent() == Mesh);
	TestEqual(TEXT("GlowLightComponent attenuation radius should match the placeholder value"),
		Glow->AttenuationRadius, 300.0f);

	// Drives a trooper from Idle straight through to Attack via two zero/mid-distance
	// detection checks (Idle -> Alert -> Attack) - shared by every case below that
	// needs a trooper already in Attack.
	auto AdvanceToAttack = [](ATrooperEnemy* TargetTrooper, const FVector& PlayerLocation)
	{
		TargetTrooper->TickCheckDetection(PlayerLocation); // Idle -> Alert
		TargetTrooper->TickCheckDetection(PlayerLocation); // Alert -> Attack
	};

	// (c) advance to Attack, then Root specifically intensifies the glow.
	const FVector ZeroDistanceLocation(0.0f, 0.0f, 0.0f);
	AdvanceToAttack(Trooper, ZeroDistanceLocation);
	TestEqual(TEXT("Trooper should be in Attack after two zero-distance detection checks"),
		static_cast<uint8>(Trooper->GetEnemyState()), static_cast<uint8>(EEnemyState::Attack));
	Trooper->ReceiveControl(EAbilitySlot::Root);
	TestEqual(TEXT("Glow should intensify after Root-triggered OnControlledEntry"),
		Glow->Intensity, Trooper->GlowIntensifiedIntensity);

	// (d) every non-Root ability produces no glow response at all, each on its own
	// fresh actor - exhaustive over EAbilitySlot rather than a single representative
	// value.
	const EAbilitySlot NonRootAbilities[] = {
		EAbilitySlot::Stun, EAbilitySlot::Sleep, EAbilitySlot::Fear, EAbilitySlot::Snare };
	for (EAbilitySlot NonRootAbility : NonRootAbilities)
	{
		ATrooperEnemy* NonRootTrooper = NewObject<ATrooperEnemy>();
		AdvanceToAttack(NonRootTrooper, ZeroDistanceLocation);
		NonRootTrooper->ReceiveControl(NonRootAbility);
		TestEqual(TEXT("Glow should stay at baseline intensity for a non-Root ability"),
			NonRootTrooper->GlowLightComponent->Intensity, NonRootTrooper->GlowBaselineIntensity);
	}

	// (e)/(f) the attack tell is off until Attack is entered, and visibly on
	// (before any ray fires) once it is - ordering proven explicitly below.
	ATrooperEnemy* TellTrooper = NewObject<ATrooperEnemy>();
	UPointLightComponent* TellLight = TellTrooper->AttackTellLightComponent;
	if (!TestNotNull(TEXT("ATrooperEnemy should have an AttackTellLightComponent"), TellLight))
	{
		return false;
	}
	TestEqual(TEXT("Attack tell should be off before Attack is entered"), TellLight->Intensity, 0.0f);
	TestTrue(TEXT("AttackTellLightComponent should be attached to MeshComponent"),
		TellLight->GetAttachParent() == TellTrooper->MeshComponent);
	TestEqual(TEXT("AttackTellLightComponent attenuation radius should match the placeholder value"),
		TellLight->AttenuationRadius, 300.0f);

	AdvanceToAttack(TellTrooper, ZeroDistanceLocation);
	TestEqual(TEXT("Attack tell should reach configured intensity once Attack is entered"),
		TellLight->Intensity, TellTrooper->AttackTellIntensity);

	UTrooperRayFiredTestListener* RayListener = NewObject<UTrooperRayFiredTestListener>();
	TellTrooper->OnTrooperRayFired.AddDynamic(RayListener, &UTrooperRayFiredTestListener::HandleTrooperRayFired);
	TestEqual(TEXT("No ray should have fired yet - tell precedes the ray"), RayListener->CallCount, 0);

	// (g) the key differentiating case: the telegraph elapsing fires a ray, and -
	// unlike ASniperEnemy/ABomberEnemy - re-arms itself so a second and third advance
	// each fire again. This is the assertion that would fail against a copy-pasted
	// Sniper/Bomber-style fire-once guard, proving the "rapid" repeat behavior
	// genuinely exists.
	TellTrooper->AdvanceAttackTelegraph(TellTrooper->AttackTelegraphSeconds);
	TestEqual(TEXT("OnTrooperRayFired should have fired exactly once"), RayListener->CallCount, 1);
	// A small delta right after a fire must NOT immediately re-fire - proves the
	// re-arm resets to the full cadence, not to <= 0 (the runaway-broadcast risk this
	// class's deliberately-omitted fire-once guard raises).
	TellTrooper->AdvanceAttackTelegraph(0.01f);
	TestEqual(TEXT("A small delta immediately after a fire should not re-fire"), RayListener->CallCount, 1);
	TellTrooper->AdvanceAttackTelegraph(TellTrooper->AttackTelegraphSeconds);
	TestEqual(TEXT("OnTrooperRayFired should re-fire on a second full-interval advance"), RayListener->CallCount, 2);
	TellTrooper->AdvanceAttackTelegraph(TellTrooper->AttackTelegraphSeconds);
	TestEqual(TEXT("OnTrooperRayFired should re-fire on a third full-interval advance"), RayListener->CallCount, 3);

	// (h) the telegraph accumulates across multiple partial advances, matching how the
	// real per-frame Tick() drives this - not just a single full-duration jump. The
	// final partial advance overshoots AttackTelegraphSeconds (0.4f) by a
	// float32-rounding-safe margin (0.15+0.1+0.2=0.45) rather than landing exactly on
	// the boundary (PR #117 review, issue #17's lesson).
	ATrooperEnemy* AccumulatingTrooper = NewObject<ATrooperEnemy>();
	AdvanceToAttack(AccumulatingTrooper, ZeroDistanceLocation);
	UTrooperRayFiredTestListener* AccumulatingListener = NewObject<UTrooperRayFiredTestListener>();
	AccumulatingTrooper->OnTrooperRayFired.AddDynamic(AccumulatingListener, &UTrooperRayFiredTestListener::HandleTrooperRayFired);

	AccumulatingTrooper->AdvanceAttackTelegraph(0.15f);
	AccumulatingTrooper->AdvanceAttackTelegraph(0.1f);
	TestEqual(TEXT("Partial advances summing to below AttackTelegraphSeconds should not fire yet"),
		AccumulatingListener->CallCount, 0);

	AccumulatingTrooper->AdvanceAttackTelegraph(0.2f); // 0.15 + 0.1 + 0.2 = 0.45, safely past 0.4
	TestEqual(TEXT("The telegraph should fire once the accumulated partial advances cross AttackTelegraphSeconds"),
		AccumulatingListener->CallCount, 1);

	// (h2) documents current behavior for a single oversized delta (e.g. a frame
	// hitch spanning multiple telegraph periods): it still fires exactly once per
	// call, and the overshoot past AttackTelegraphSeconds is discarded rather than
	// carried into the next cycle - AdvanceAttackTelegraph only ever re-arms to the
	// full AttackTelegraphSeconds, never AttackTelegraphSeconds - overshoot.
	ATrooperEnemy* HitchTrooper = NewObject<ATrooperEnemy>();
	AdvanceToAttack(HitchTrooper, ZeroDistanceLocation);
	UTrooperRayFiredTestListener* HitchListener = NewObject<UTrooperRayFiredTestListener>();
	HitchTrooper->OnTrooperRayFired.AddDynamic(HitchListener, &UTrooperRayFiredTestListener::HandleTrooperRayFired);
	HitchTrooper->AdvanceAttackTelegraph(HitchTrooper->AttackTelegraphSeconds * 3.0f);
	TestEqual(TEXT("A single oversized delta should still fire only once"), HitchListener->CallCount, 1);

	// (i) advancing the telegraph while not in Attack is a no-op.
	ATrooperEnemy* IdleTrooper = NewObject<ATrooperEnemy>();
	UTrooperRayFiredTestListener* IdleListener = NewObject<UTrooperRayFiredTestListener>();
	IdleTrooper->OnTrooperRayFired.AddDynamic(IdleListener, &UTrooperRayFiredTestListener::HandleTrooperRayFired);
	IdleTrooper->AdvanceAttackTelegraph(999.0f);
	TestEqual(TEXT("AdvanceAttackTelegraph should be a no-op while not in Attack"),
		IdleListener->CallCount, 0);

	// (j) GetAttackRangeUnits() is medium - strictly between ABomberEnemy's short
	// (150.0f, BomberEnemy.cpp) and ASniperEnemy's long (1400.0f, SniperEnemy.cpp)
	// values. GetAttackRangeUnits() is protected on both sibling classes and this
	// test isn't (and shouldn't be) friended to either, so the comparison is against
	// their own known return values rather than a live call on a foreign instance.
	ATrooperEnemy* RangeTrooper = NewObject<ATrooperEnemy>();
	TestEqual(TEXT("GetAttackRangeUnits() should return TR-UPR's medium-range value"),
		RangeTrooper->GetAttackRangeUnits(), 700.0f);
	TestTrue(TEXT("TR-UPR's attack range should be greater than B0-0MR's short range"),
		RangeTrooper->GetAttackRangeUnits() > 150.0f);
	TestTrue(TEXT("TR-UPR's attack range should be less than SN-1PR's long range"),
		RangeTrooper->GetAttackRangeUnits() < 1400.0f);

	// (k) the attack tell colour must not collide with any of MISSION.md Hard
	// Invariant 3's five reserved gameplay-information colours - hand-verified against
	// ReservedGameplayColours.cpp's current values, this assertion is the automated
	// backstop against a future colour tweak silently reusing one of them.
	TestFalse(TEXT("Attack tell colour should not collide with a reserved gameplay-information colour"),
		ReservedGameplayColours::GetAll().ContainsByPredicate(
			[TellLight](const FLinearColor& Reserved) { return Reserved.Equals(TellLight->GetLightColor(), 0.01f); }));

	// (l) ReceiveControl interrupting an in-progress attack telegraph clears the tell
	// light and stops further rays - the state guard in AdvanceAttackTelegraph already
	// stops the ray itself once Controlled is entered; this proves the visual is
	// cleared too and that no further ray fires afterward.
	ATrooperEnemy* InterruptedTrooper = NewObject<ATrooperEnemy>();
	AdvanceToAttack(InterruptedTrooper, ZeroDistanceLocation);
	TestTrue(TEXT("Attack tell should be visibly on before the interrupt"),
		InterruptedTrooper->AttackTellLightComponent->Intensity > 0.0f);
	InterruptedTrooper->ReceiveControl(EAbilitySlot::Root);
	TestEqual(TEXT("Attack tell should be cleared once Controlled interrupts the attack"),
		InterruptedTrooper->AttackTellLightComponent->Intensity, 0.0f);
	UTrooperRayFiredTestListener* InterruptedListener = NewObject<UTrooperRayFiredTestListener>();
	InterruptedTrooper->OnTrooperRayFired.AddDynamic(InterruptedListener, &UTrooperRayFiredTestListener::HandleTrooperRayFired);
	InterruptedTrooper->AdvanceAttackTelegraph(InterruptedTrooper->AttackTelegraphSeconds);
	TestEqual(TEXT("No further ray should fire once interrupted"), InterruptedListener->CallCount, 0);

	// (m) the real Tick() override, not just the friend-called AdvanceAttackTelegraph
	// helper, must wire the telegraph into the per-frame loop, and the rapid re-arm
	// must survive through that real path - two successive Tick() calls should each
	// fire a ray, not just the first.
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	if (TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
	{
		ATrooperEnemy* TickedTrooper = World->SpawnActor<ATrooperEnemy>();
		if (TestNotNull(TEXT("ATrooperEnemy should spawn into the test World"), TickedTrooper))
		{
			AdvanceToAttack(TickedTrooper, ZeroDistanceLocation);
			UTrooperRayFiredTestListener* TickedListener = NewObject<UTrooperRayFiredTestListener>();
			TickedTrooper->OnTrooperRayFired.AddDynamic(TickedListener, &UTrooperRayFiredTestListener::HandleTrooperRayFired);
			TickedTrooper->Tick(TickedTrooper->AttackTelegraphSeconds);
			TickedTrooper->Tick(TickedTrooper->AttackTelegraphSeconds);
			TestEqual(TEXT("Two successive Tick() calls should each fire a ray, proving the rapid re-arm survives the real per-frame path"),
				TickedListener->CallCount, 2);
		}
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
