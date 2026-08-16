// Confirms ASniperEnemy (issue #17, PRD 03) satisfies SN-1PR's acceptance criteria:
// a distinct cone silhouette, a Blue eye-glow that intensifies ONLY when Sleep is the
// ability that triggered OnControlledEntry (no response for any other ability), a
// visible attack tell that precedes OnSniperShotFired (provably ordered), the tell
// fires exactly once per attack, and a long GetAttackRangeUnits() override relative
// to the base class default.
//
// Uses NewObject rather than spawning into a UWorld: nothing exercised here calls
// GetWorld()/SpawnActor - AdvanceAttackTelegraph and ReceiveControl are both driven
// directly via friend access, never through a real Tick() loop, same rationale
// KrowdKontrolAbilityCooldownTest.cpp documents.
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

	// (c) advance to Attack, then Sleep specifically intensifies the glow.
	const FVector ZeroDistanceLocation(0.0f, 0.0f, 0.0f);
	Sniper->TickCheckDetection(ZeroDistanceLocation); // Idle -> Alert
	Sniper->TickCheckDetection(ZeroDistanceLocation); // Alert -> Attack
	TestEqual(TEXT("Sniper should be in Attack after two zero-distance detection checks"),
		static_cast<uint8>(Sniper->GetEnemyState()), static_cast<uint8>(EEnemyState::Attack));
	Sniper->ReceiveControl(EAbilitySlot::Sleep);
	TestEqual(TEXT("Eye glow should intensify after Sleep-triggered OnControlledEntry"),
		EyeGlow->Intensity, Sniper->EyeGlowIntensifiedIntensity);

	// (d) a non-Sleep ability produces no glow response at all, on a fresh actor.
	ASniperEnemy* NonSleepSniper = NewObject<ASniperEnemy>();
	NonSleepSniper->TickCheckDetection(ZeroDistanceLocation); // Idle -> Alert
	NonSleepSniper->TickCheckDetection(ZeroDistanceLocation); // Alert -> Attack
	NonSleepSniper->ReceiveControl(EAbilitySlot::Root);
	TestEqual(TEXT("Eye glow should stay at baseline intensity for a non-Sleep ability"),
		NonSleepSniper->EyeGlowLightComponent->Intensity, NonSleepSniper->EyeGlowBaselineIntensity);

	// (e)/(f) the attack tell is off until Attack is entered, and visibly on
	// (before the shot fires) once it is - ordering proven explicitly below.
	ASniperEnemy* TellSniper = NewObject<ASniperEnemy>();
	UPointLightComponent* TellLight = TellSniper->AttackTellLightComponent;
	if (!TestNotNull(TEXT("ASniperEnemy should have an AttackTellLightComponent"), TellLight))
	{
		return false;
	}
	TestEqual(TEXT("Attack tell should be off before Attack is entered"), TellLight->Intensity, 0.0f);

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
	const FVector MidRangeLocation(800.0f, 0.0f, 0.0f);
	LongRangeSniper->TickCheckDetection(MidRangeLocation); // Idle -> Alert
	LongRangeSniper->TickCheckDetection(MidRangeLocation); // Alert -> Attack, since 800 <= 1400
	TestEqual(TEXT("Sniper's long attack range should reach Attack well beyond a short-range distance"),
		static_cast<uint8>(LongRangeSniper->GetEnemyState()), static_cast<uint8>(EEnemyState::Attack));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
