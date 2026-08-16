// Confirms ASniperEnemy (issue #17, PRD 03) satisfies SN-1PR's acceptance criteria:
// a distinct cone silhouette, a Blue eye-glow that intensifies ONLY when Sleep is the
// ability that triggered OnControlledEntry (no response for any other ability), a
// visible attack tell that precedes OnSniperShotFired (provably ordered), the tell
// fires exactly once per attack, and a long GetAttackRangeUnits() override relative
// to the base class default.
//
// Uses NewObject rather than spawning into a UWorld: nothing exercised here calls
// GetWorld()/SpawnActor - AdvanceAttackTelegraph and TickCheckDetection are both
// driven directly via friend access, never through a real Tick() loop, same rationale
// KrowdKontrolAbilityCooldownTest.cpp documents. Case (m) is the one exception - it
// spawns into a real UWorld to prove the Tick() override itself is wired correctly.
//
// Case (j) below is a structural proxy, not a direct numeric assertion against
// another enemy's range, because no sibling concrete enemy type exists in the
// codebase yet to compare against (issues #13/#14/#15 are unimplemented) - strengthen
// it once a second concrete enemy type lands.
//
// #if-guarded so this compiles out of Shipping/packaged builds, same as the other
// KrowdKontrol.Unit.* tests.

#include "Misc/AutomationTest.h"
#include "SniperEnemy.h"
#include "ReservedGameplayColours.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Tests/AutomationEditorCommon.h"
#include "SniperShotFiredTestListener.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolSniperEnemyTest,
	"KrowdKontrol.Unit.SniperEnemy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolSniperEnemyTest::RunTest(const FString& Parameters)
{
	ASniperEnemy* Sniper = NewObject<ASniperEnemy>();
	if (!TestNotNull(TEXT("ASniperEnemy should construct"), Sniper))
	{
		return false;
	}

	// (a) distinct cone mesh, not the shapes other placeholder actors already use.
	UStaticMeshComponent* Mesh = Sniper->MeshComponent;
	if (!TestNotNull(TEXT("ASniperEnemy should have a MeshComponent"), Mesh))
	{
		return false;
	}
	UStaticMesh* StaticMesh = Mesh->GetStaticMesh();
	if (!TestNotNull(TEXT("MeshComponent should have a static mesh assigned"), StaticMesh))
	{
		return false;
	}
	const FString MeshPath = StaticMesh->GetPathName();
	TestEqual(TEXT("MeshComponent should use the engine's cone mesh"),
		MeshPath, FString(TEXT("/Engine/BasicShapes/Cone.Cone")));
	TestNotEqual(TEXT("MeshComponent should not collide with the cube placeholder's mesh"),
		MeshPath, FString(TEXT("/Engine/BasicShapes/Cube.Cube")));
	TestNotEqual(TEXT("MeshComponent should not collide with the target zone's cylinder mesh"),
		MeshPath, FString(TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));
	// Disc-flattening-equivalent distinct silhouette scale (issue #72's precedent for
	// this class of "mesh + attached light" actor) - never asserted before this,
	// so an accidental revert to an unscaled (1,1,1) cone would silently pass.
	TestTrue(TEXT("MeshComponent should use the distinct cone silhouette scale"),
		Mesh->GetRelativeScale3D().Equals(FVector(1.0f, 1.0f, 1.8f), 0.01f));

	// (b) Blue eye-glow at baseline intensity, un-intensified at construction.
	UPointLightComponent* EyeGlow = Sniper->EyeGlowLightComponent;
	if (!TestNotNull(TEXT("ASniperEnemy should have an EyeGlowLightComponent"), EyeGlow))
	{
		return false;
	}
	TestTrue(TEXT("Eye glow colour should be the reserved Blue"),
		EyeGlow->GetLightColor().Equals(ReservedGameplayColours::GetBlue(), 0.01f));
	TestEqual(TEXT("Eye glow should start at baseline intensity"),
		EyeGlow->Intensity, Sniper->EyeGlowBaselineIntensity);
	// Attachment/attenuation, per the same PR #90 follow-up precedent
	// (app-changelog/issue-72.md) that required these assertions on
	// APlaceholderTargetZoneActor's near-identical mesh+light shape - an accidental
	// future edit attaching the glow to a different root, or dropping the
	// attenuation-radius literal, would otherwise pass silently.
	TestTrue(TEXT("EyeGlowLightComponent should be attached to MeshComponent"),
		EyeGlow->GetAttachParent() == Mesh);
	TestEqual(TEXT("EyeGlowLightComponent attenuation radius should match the placeholder value"),
		EyeGlow->AttenuationRadius, 300.0f);

	// (c) advance to Attack, then Sleep specifically intensifies the glow.
	const FVector ZeroDistanceLocation(0.0f, 0.0f, 0.0f);
	Sniper->TickCheckDetection(ZeroDistanceLocation); // Idle -> Alert
	Sniper->TickCheckDetection(ZeroDistanceLocation); // Alert -> Attack
	TestEqual(TEXT("Sniper should be in Attack after two zero-distance detection checks"),
		static_cast<uint8>(Sniper->GetEnemyState()), static_cast<uint8>(EEnemyState::Attack));
	Sniper->ReceiveControl(EAbilitySlot::Sleep);
	TestEqual(TEXT("Eye glow should intensify after Sleep-triggered OnControlledEntry"),
		EyeGlow->Intensity, Sniper->EyeGlowIntensifiedIntensity);

	// (d) every non-Sleep ability produces no glow response at all, each on its own
	// fresh actor - exhaustive over EAbilitySlot rather than a single representative
	// value, since the guard could someday become per-ability instead of a single
	// equality check.
	const EAbilitySlot NonSleepAbilities[] = {
		EAbilitySlot::Stun, EAbilitySlot::Root, EAbilitySlot::Fear, EAbilitySlot::Snare };
	for (EAbilitySlot NonSleepAbility : NonSleepAbilities)
	{
		ASniperEnemy* NonSleepSniper = NewObject<ASniperEnemy>();
		NonSleepSniper->TickCheckDetection(ZeroDistanceLocation); // Idle -> Alert
		NonSleepSniper->TickCheckDetection(ZeroDistanceLocation); // Alert -> Attack
		NonSleepSniper->ReceiveControl(NonSleepAbility);
		TestEqual(TEXT("Eye glow should stay at baseline intensity for a non-Sleep ability"),
			NonSleepSniper->EyeGlowLightComponent->Intensity, NonSleepSniper->EyeGlowBaselineIntensity);
	}

	// (e)/(f) the attack tell is off until Attack is entered, and visibly on
	// (before the shot fires) once it is - ordering proven explicitly below.
	ASniperEnemy* TellSniper = NewObject<ASniperEnemy>();
	UPointLightComponent* TellLight = TellSniper->AttackTellLightComponent;
	if (!TestNotNull(TEXT("ASniperEnemy should have an AttackTellLightComponent"), TellLight))
	{
		return false;
	}
	TestEqual(TEXT("Attack tell should be off before Attack is entered"), TellLight->Intensity, 0.0f);
	TestTrue(TEXT("AttackTellLightComponent should be attached to MeshComponent"),
		TellLight->GetAttachParent() == TellSniper->MeshComponent);
	TestEqual(TEXT("AttackTellLightComponent attenuation radius should match the placeholder value"),
		TellLight->AttenuationRadius, 300.0f);

	TellSniper->TickCheckDetection(ZeroDistanceLocation); // Idle -> Alert
	TellSniper->TickCheckDetection(ZeroDistanceLocation); // Alert -> Attack
	TestTrue(TEXT("Attack tell should be visibly on once Attack is entered"), TellLight->Intensity > 0.0f);

	USniperShotFiredTestListener* ShotListener = NewObject<USniperShotFiredTestListener>();
	TellSniper->OnSniperShotFired.AddDynamic(ShotListener, &USniperShotFiredTestListener::HandleSniperShotFired);
	TestEqual(TEXT("The shot should not have fired yet - tell precedes the shot"), ShotListener->CallCount, 0);

	// (g) the telegraph elapsing fires the shot exactly once.
	TellSniper->AdvanceAttackTelegraph(TellSniper->AttackTelegraphSeconds);
	TestEqual(TEXT("OnSniperShotFired should have fired exactly once"), ShotListener->CallCount, 1);

	// (h) advancing again after the shot fired does not re-fire it.
	TellSniper->AdvanceAttackTelegraph(1.0f);
	TestEqual(TEXT("OnSniperShotFired should not re-fire on a later telegraph advance"),
		ShotListener->CallCount, 1);

	// (g2) the telegraph accumulates across multiple partial advances, matching how
	// the real per-frame Tick() drives this - not just a single full-duration jump.
	// Distinguishes "decrements a running total" from "any call >= AttackTelegraphSeconds
	// fires", which case (g) alone cannot. The final partial advance overshoots
	// AttackTelegraphSeconds by a float32-rounding-safe margin (0.5 + 0.4 + 0.5 = 1.4,
	// not the boundary-exact 1.2) - an earlier version of this test landed the final
	// advance exactly on the 1.2 boundary and asserted
	// `1.2f - 0.5f - 0.4f - 0.3f == 0.0f`, which IEEE-754 float32 does not actually
	// produce (true residual: 2.9802322e-08, not 0.0f), so the shot never fired and
	// the assertion was wrong, not AdvanceAttackTelegraph's FMath::Max clamp (PR #117
	// review, issue #17).
	ASniperEnemy* AccumulatingSniper = NewObject<ASniperEnemy>();
	AccumulatingSniper->TickCheckDetection(ZeroDistanceLocation); // Idle -> Alert
	AccumulatingSniper->TickCheckDetection(ZeroDistanceLocation); // Alert -> Attack
	USniperShotFiredTestListener* AccumulatingListener = NewObject<USniperShotFiredTestListener>();
	AccumulatingSniper->OnSniperShotFired.AddDynamic(AccumulatingListener, &USniperShotFiredTestListener::HandleSniperShotFired);

	AccumulatingSniper->AdvanceAttackTelegraph(0.5f);
	AccumulatingSniper->AdvanceAttackTelegraph(0.4f);
	TestEqual(TEXT("Partial advances summing to below AttackTelegraphSeconds should not fire yet"),
		AccumulatingListener->CallCount, 0);

	AccumulatingSniper->AdvanceAttackTelegraph(0.5f); // 0.5 + 0.4 + 0.5 = 1.4, safely past 1.2
	TestEqual(TEXT("The telegraph should fire once the accumulated partial advances cross AttackTelegraphSeconds"),
		AccumulatingListener->CallCount, 1);

	// (i) advancing the telegraph while not in Attack is a no-op.
	ASniperEnemy* IdleSniper = NewObject<ASniperEnemy>();
	USniperShotFiredTestListener* IdleListener = NewObject<USniperShotFiredTestListener>();
	IdleSniper->OnSniperShotFired.AddDynamic(IdleListener, &USniperShotFiredTestListener::HandleSniperShotFired);
	IdleSniper->AdvanceAttackTelegraph(999.0f);
	TestEqual(TEXT("AdvanceAttackTelegraph should be a no-op while not in Attack"),
		IdleListener->CallCount, 0);

	// (j) GetAttackRangeUnits() is large relative to the base default - structural
	// proxy: a distance well beyond a hypothetical short-range enemy's reach (50.0f)
	// but still within the base DetectionRangeUnits default (1500.0f) still reaches
	// Attack after the same two-step detection walk used in (c).
	ASniperEnemy* LongRangeSniper = NewObject<ASniperEnemy>();
	TestEqual(TEXT("GetAttackRangeUnits() should return SN-1PR's long-range value"),
		LongRangeSniper->GetAttackRangeUnits(), 1400.0f);
	const FVector MidRangeLocation(800.0f, 0.0f, 0.0f);
	LongRangeSniper->TickCheckDetection(MidRangeLocation); // Idle -> Alert
	LongRangeSniper->TickCheckDetection(MidRangeLocation); // Alert -> Attack, since 800 <= 1400
	TestEqual(TEXT("Sniper's long attack range should reach Attack well beyond a short-range distance"),
		static_cast<uint8>(LongRangeSniper->GetEnemyState()), static_cast<uint8>(EEnemyState::Attack));

	// (k) the attack tell colour must not collide with any of MISSION.md Hard
	// Invariant 3's five reserved gameplay-information colours - the placeholder was
	// hand-verified against ReservedGameplayColours.cpp's current values, but this
	// assertion is the automated backstop against a future colour tweak silently
	// reusing one of them.
	TestFalse(TEXT("Attack tell colour should not collide with a reserved gameplay-information colour"),
		ReservedGameplayColours::GetAll().ContainsByPredicate(
			[TellLight](const FLinearColor& Reserved) { return Reserved.Equals(TellLight->GetLightColor(), 0.01f); }));

	// (l) ReceiveControl interrupting an in-progress attack telegraph clears the tell
	// light, so a sniper put to sleep mid-telegraph doesn't keep showing a shot that
	// will never fire (the state guard in AdvanceAttackTelegraph already stops the
	// shot itself - this proves the visual is cleared too).
	ASniperEnemy* InterruptedSniper = NewObject<ASniperEnemy>();
	InterruptedSniper->TickCheckDetection(ZeroDistanceLocation); // Idle -> Alert
	InterruptedSniper->TickCheckDetection(ZeroDistanceLocation); // Alert -> Attack
	TestTrue(TEXT("Attack tell should be visibly on before the interrupt"),
		InterruptedSniper->AttackTellLightComponent->Intensity > 0.0f);
	InterruptedSniper->ReceiveControl(EAbilitySlot::Sleep);
	TestEqual(TEXT("Attack tell should be cleared once Controlled interrupts the attack"),
		InterruptedSniper->AttackTellLightComponent->Intensity, 0.0f);
	USniperShotFiredTestListener* InterruptedListener = NewObject<USniperShotFiredTestListener>();
	InterruptedSniper->OnSniperShotFired.AddDynamic(InterruptedListener, &USniperShotFiredTestListener::HandleSniperShotFired);
	InterruptedSniper->AdvanceAttackTelegraph(InterruptedSniper->AttackTelegraphSeconds);
	TestEqual(TEXT("The interrupted shot should never fire"), InterruptedListener->CallCount, 0);

	// (m) the real Tick() override, not just the friend-called AdvanceAttackTelegraph
	// helper, must wire the telegraph into the per-frame loop - proves neither a
	// missing Super::Tick(DeltaTime) nor a wiring mistake in the override itself,
	// mirroring KrowdKontrolEnemyBaseTest.cpp case (k)'s real-UWorld Tick() coverage.
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	if (TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
	{
		ASniperEnemy* TickedSniper = World->SpawnActor<ASniperEnemy>();
		if (TestNotNull(TEXT("ASniperEnemy should spawn into the test World"), TickedSniper))
		{
			TickedSniper->TickCheckDetection(ZeroDistanceLocation); // Idle -> Alert
			TickedSniper->TickCheckDetection(ZeroDistanceLocation); // Alert -> Attack
			USniperShotFiredTestListener* TickedListener = NewObject<USniperShotFiredTestListener>();
			TickedSniper->OnSniperShotFired.AddDynamic(TickedListener, &USniperShotFiredTestListener::HandleSniperShotFired);
			TickedSniper->Tick(TickedSniper->AttackTelegraphSeconds);
			TestEqual(TEXT("Tick() should drive the telegraph through to firing the shot"),
				TickedListener->CallCount, 1);
		}
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
