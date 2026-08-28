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
#include "SniperEnemy.h"
#include "BomberEnemy.h"
#include "ReservedGameplayColours.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Tests/AutomationEditorCommon.h"
#include "TrooperRayFiredTestListener.h"
#include "Sound/SoundWave.h"
#include "Components/AudioComponent.h"
#include "AbilityData.h"
#include "EnemyTypeIndicatorComponent.h"
#include "EnemyType.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Components/SceneComponent.h"

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

	// (a2) EnemyTypeIndicatorComponent is wired to TR_UPR - PRD 13 REQ-7's colourblind-
	// safe marker, and (issue #242) the component ATargetZone's type-keyed acceptance
	// resolves via FindComponentByClass. Not implied by the component's own field
	// default (which happens to also be TR_UPR) - this proves the constructor's
	// explicit assignment, not the default, is what's in effect.
	UEnemyTypeIndicatorComponent* TrooperIndicator = Trooper->EnemyTypeIndicatorComponent;
	if (TestNotNull(TEXT("ATrooperEnemy should have an EnemyTypeIndicatorComponent"), TrooperIndicator))
	{
		TestEqual(TEXT("EnemyTypeIndicatorComponent should report TR_UPR"),
			static_cast<uint8>(TrooperIndicator->EnemyType),
			static_cast<uint8>(EEnemyType::TR_UPR));
	}

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

	// (b2) Elite trim light (issue #19): exists, attached to MeshComponent, colour
	// non-reserved. Checked here (early, on the original un-GC'd Trooper) rather than
	// at the end of this test after dozens more NewObject<>() instances and several
	// CreateNewMap() calls have run - a NewObject<>()-constructed actor held only by
	// a local pointer has no GC roots, and asserting on it this late risked it having
	// already been collected by an incidental GC pass triggered by the later, heavier
	// cases (reproduced empirically: intermittent null failures when checked late).
	if (TestNotNull(TEXT("ATrooperEnemy should have an EliteTrimLightComponent"), Trooper->EliteTrimLightComponent.Get()))
	{
		TestTrue(TEXT("EliteTrimLightComponent should be attached to MeshComponent"),
			Trooper->EliteTrimLightComponent->GetAttachParent() == Mesh);
		TestFalse(TEXT("EliteTrimLightComponent colour should not collide with a reserved gameplay-information colour"),
			ReservedGameplayColours::GetAll().ContainsByPredicate(
				[Trooper](const FLinearColor& Reserved) { return Reserved.Equals(Trooper->EliteTrimLightComponent->GetLightColor(), 0.01f); }));
	}

	// (b3) Body chain-colour tint (issue #316): TR-UPR's chain colour is Root's colour
	// (AbilityData::GetChainColourForEnemyType), applied to MeshComponent via a lazily-
	// created MID, and ApplyBodyChainColourTint() is idempotent (same MID instance,
	// re-applying the same colour, on a second call).
	Trooper->ApplyBodyChainColourTint();
	const FLinearColor ExpectedBodyColour = AbilityData::GetChainColourForEnemyType(EEnemyType::TR_UPR);
	TestTrue(TEXT("body chain colour matches AbilityData::GetChainColourForEnemyType(TR_UPR)"),
		Trooper->CurrentBodyChainColour.Equals(ExpectedBodyColour, 0.01f));
	UMaterialInstanceDynamic* FirstBodyMID = Trooper->BodyChainColourMaterialInstance;
	if (TestNotNull(TEXT("BodyChainColourMaterialInstance should be created"), FirstBodyMID))
	{
		TestTrue(TEXT("MeshComponent's material 0 is the BodyChainColourMaterialInstance"),
			Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(0)) == FirstBodyMID);
	}
	Trooper->ApplyBodyChainColourTint();
	TestTrue(TEXT("second call reuses the same MID instance"),
		Trooper->BodyChainColourMaterialInstance.Get() == FirstBodyMID);
	TestEqual(TEXT("EnemyTypeIndicatorComponent's own EnemyType is unchanged by the body tint"),
		static_cast<uint8>(Trooper->EnemyTypeIndicatorComponent->EnemyType), static_cast<uint8>(EEnemyType::TR_UPR));

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
	// cleared too and that no further ray fires afterward. Uses Stun (a full-immobilize
	// ability, bAllowsAttackWhileControlled == false) - issue #255 makes this no longer
	// true for Root specifically, see case (l2) below.
	ATrooperEnemy* InterruptedTrooper = NewObject<ATrooperEnemy>();
	AdvanceToAttack(InterruptedTrooper, ZeroDistanceLocation);
	TestTrue(TEXT("Attack tell should be visibly on before the interrupt"),
		InterruptedTrooper->AttackTellLightComponent->Intensity > 0.0f);
	InterruptedTrooper->ReceiveControl(EAbilitySlot::Stun);
	TestEqual(TEXT("Attack tell should be cleared once Controlled interrupts the attack"),
		InterruptedTrooper->AttackTellLightComponent->Intensity, 0.0f);
	UTrooperRayFiredTestListener* InterruptedListener = NewObject<UTrooperRayFiredTestListener>();
	InterruptedTrooper->OnTrooperRayFired.AddDynamic(InterruptedListener, &UTrooperRayFiredTestListener::HandleTrooperRayFired);
	InterruptedTrooper->AdvanceAttackTelegraph(InterruptedTrooper->AttackTelegraphSeconds);
	TestEqual(TEXT("No further ray should fire once interrupted"), InterruptedListener->CallCount, 0);

	// (l2) issue #255: unlike Stun above, Root-triggered Controlled does NOT clear
	// the attack tell or stop the telegraph - the ray keeps firing exactly as it
	// would in Attack, since AbilityData::Get(Root).bAllowsAttackWhileControlled.
	ATrooperEnemy* RootedTrooper = NewObject<ATrooperEnemy>();
	AdvanceToAttack(RootedTrooper, ZeroDistanceLocation);
	TestTrue(TEXT("(l2) Attack tell should be visibly on before Root interrupts"),
		RootedTrooper->AttackTellLightComponent->Intensity > 0.0f);
	RootedTrooper->ReceiveControl(EAbilitySlot::Root);
	TestEqual(TEXT("(l2) Trooper should be Controlled after Root"),
		static_cast<uint8>(RootedTrooper->GetEnemyState()), static_cast<uint8>(EEnemyState::Controlled));
	TestTrue(TEXT("(l2) Attack tell should stay on - Root does not clear it"),
		RootedTrooper->AttackTellLightComponent->Intensity > 0.0f);
	UTrooperRayFiredTestListener* RootedListener = NewObject<UTrooperRayFiredTestListener>();
	RootedTrooper->OnTrooperRayFired.AddDynamic(RootedListener, &UTrooperRayFiredTestListener::HandleTrooperRayFired);
	RootedTrooper->AdvanceAttackTelegraph(RootedTrooper->AttackTelegraphSeconds);
	TestEqual(TEXT("(l2) A ray should still fire while Controlled by Root, unlike Stun"),
		RootedListener->CallCount, 1);
	RootedTrooper->AdvanceAttackTelegraph(RootedTrooper->AttackTelegraphSeconds);
	TestEqual(TEXT("(l2) The telegraph should keep re-arming and firing while Rooted, matching normal Attack behaviour"),
		RootedListener->CallCount, 2);

	// (l-attack-expired) issue #313 pass-1 review follow-up (HIGH): OnAttackExpired
	// must clear the tell light once the Attack-duration timeout reverts Attack ->
	// Alert unconditionally, mid-telegraph - the same bug class the Controlled ->
	// Alert edge's OnControlledExpired override exists to prevent, but for Attack.
	ATrooperEnemy* ExpiredAttackTrooper = NewObject<ATrooperEnemy>();
	AdvanceToAttack(ExpiredAttackTrooper, ZeroDistanceLocation);
	TestTrue(TEXT("(l-attack-expired) Attack tell should be visibly on before the Attack-duration timeout"),
		ExpiredAttackTrooper->AttackTellLightComponent->Intensity > 0.0f);
	ExpiredAttackTrooper->TickAttackDuration(ExpiredAttackTrooper->GetAttackDurationSeconds());
	TestEqual(TEXT("(l-attack-expired) Trooper should be back to Alert once the Attack-duration timeout elapses"),
		static_cast<uint8>(ExpiredAttackTrooper->GetEnemyState()), static_cast<uint8>(EEnemyState::Alert));
	TestEqual(TEXT("(l-attack-expired) OnAttackExpired should clear the tell light once the Attack-duration timeout elapses"),
		ExpiredAttackTrooper->AttackTellLightComponent->Intensity, 0.0f);

	// (m-snare) issue #254: unlike Root above (which runs its attack unmodified), Snare
	// scales the attack telegraph's elapsed time by ControlledSpeedMultiplier (0.5f) -
	// a full AttackTelegraphSeconds' worth of ticks only advances the telegraph 50% of
	// the way, so the ray has NOT fired yet; a second identical tick brings the
	// cumulative elapsed time up to AttackTelegraphSeconds and the ray fires exactly
	// once. This is Trooper's own independent AdvanceAttackTelegraph copy, mirroring
	// KrowdKontrolBomberEnemyTest.cpp's (m-snare) case for the same behaviour.
	ATrooperEnemy* SnaredTrooper = NewObject<ATrooperEnemy>();
	AdvanceToAttack(SnaredTrooper, ZeroDistanceLocation);
	SnaredTrooper->ReceiveControl(EAbilitySlot::Snare);
	TestEqual(TEXT("(m-snare) Trooper should be Controlled after Snare"),
		static_cast<uint8>(SnaredTrooper->GetEnemyState()), static_cast<uint8>(EEnemyState::Controlled));
	UTrooperRayFiredTestListener* SnaredListener = NewObject<UTrooperRayFiredTestListener>();
	SnaredTrooper->OnTrooperRayFired.AddDynamic(SnaredListener, &UTrooperRayFiredTestListener::HandleTrooperRayFired);
	SnaredTrooper->AdvanceAttackTelegraph(SnaredTrooper->AttackTelegraphSeconds);
	TestEqual(TEXT("(m-snare) The ray should NOT have fired after only one telegraph's worth of half-speed ticks"),
		SnaredListener->CallCount, 0);
	SnaredTrooper->AdvanceAttackTelegraph(SnaredTrooper->AttackTelegraphSeconds);
	TestEqual(TEXT("(m-snare) The ray should fire exactly once once cumulative elapsed time (at half speed) reaches AttackTelegraphSeconds"),
		SnaredListener->CallCount, 1);

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

	// (m2) issue #313 follow-up (test-coverage review, MEDIUM finding): pins the
	// disclosed cadence change - a Trooper's derived GetAttackDurationSeconds() is
	// max(2.5s base floor, 0.4s telegraph + margin) = the 2.5s floor, so during
	// sustained close-range engagement the timeout forces Attack -> Alert and, within
	// that same Tick() call, TickCheckDetection immediately re-detects the still-in-range
	// player back into Attack (replaying OnAttackEntry()'s attack tell) - instead of
	// firing continuously forever the way Trooper's rapid re-arm did pre-#313. This is
	// operator-ratified behaviour (2026-08-27 ruling on PR #336: repeating attacks are
	// the intended enemy model) - pinning it here so a future change to this
	// behaviour, deliberate or not, shows up as a named test result instead of only
	// ever being caught by a live playtest.
	UWorld* CadenceWorld = FAutomationEditorCommonUtils::CreateNewMap();
	if (TestNotNull(TEXT("CreateNewMap should return a valid World"), CadenceWorld))
	{
		ATrooperEnemy* CadenceTrooper = CadenceWorld->SpawnActor<ATrooperEnemy>();
		APawn* CadencePlayerPawn = CadenceWorld->SpawnActor<APawn>();
		if (TestNotNull(TEXT("ATrooperEnemy should spawn into the test World"), CadenceTrooper)
			&& TestNotNull(TEXT("APawn should spawn into the test World"), CadencePlayerPawn))
		{
			APlayerController* CadenceController = CadenceWorld->SpawnActor<APlayerController>();
			if (!TestNotNull(TEXT("Should be able to spawn a controller to possess the pawn"), CadenceController))
			{
				return false;
			}
			CadenceController->Possess(CadencePlayerPawn);
			CadenceWorld->AddController(CadenceController);

			USceneComponent* CadencePlayerPawnRoot = NewObject<USceneComponent>(CadencePlayerPawn);
			CadencePlayerPawnRoot->RegisterComponent();
			CadencePlayerPawn->SetRootComponent(CadencePlayerPawnRoot);
			CadencePlayerPawn->SetActorLocation(ZeroDistanceLocation); // stays within attack range throughout

			AdvanceToAttack(CadenceTrooper, ZeroDistanceLocation);
			UTrooperRayFiredTestListener* CadenceListener = NewObject<UTrooperRayFiredTestListener>();
			CadenceTrooper->OnTrooperRayFired.AddDynamic(CadenceListener, &UTrooperRayFiredTestListener::HandleTrooperRayFired);

			// Tick through exactly one full Attack window, snapshot the fire count at
			// the expiry boundary, then keep ticking - only a post-boundary INCREASE
			// proves the ray survives the Attack -> Alert -> Attack cycle. (The old
			// CallCount > 1 assertion was vacuous: the 0.4s telegraph fires ~5 times
			// inside the first window alone, so it passed even if the cycle went
			// permanently silent - PR #336 pass-2 escalation, MEDIUM finding.)
			const float CadenceAttackDurationSeconds = CadenceTrooper->GetAttackDurationSeconds();
			for (float Elapsed = 0.0f; Elapsed < CadenceAttackDurationSeconds + 0.25f; Elapsed += 0.5f)
			{
				CadenceTrooper->Tick(0.5f);
			}
			const int32 CallCountAtExpiryBoundary = CadenceListener->CallCount;

			for (int32 PostBoundaryTick = 0; PostBoundaryTick < 4; ++PostBoundaryTick)
			{
				CadenceTrooper->Tick(0.5f);
			}

			TestEqual(TEXT("A Trooper kept in range should be back in Attack after the base timeout cycles it through Alert and back, same real Tick() path"),
				static_cast<uint8>(CadenceTrooper->GetEnemyState()), static_cast<uint8>(EEnemyState::Attack));
			TestTrue(TEXT("The attack-tell ray must fire again AFTER the expiry boundary, not go silent across the Attack -> Alert -> Attack cycle"),
				CadenceListener->CallCount > CallCountAtExpiryBoundary);
		}
	}

	// (n)/(o)/(p)/(q) OnAttackEntry's audio-cue spawning - needs a real UWorld
	// (SpawnSoundAtLocation resolves it via the actor's outer), same shape as case
	// (m). NewObject<USoundWave>() (no .uasset) is sufficient, per
	// KrowdKontrolSniperEnemyTest.cpp case (n)'s precedent.
	UWorld* AudioWorld = FAutomationEditorCommonUtils::CreateNewMap();
	if (TestNotNull(TEXT("CreateNewMap should return a valid World for the audio tests"), AudioWorld))
	{
		// (n) OnAttackEntry spawns the attack-tell audio cue when AttackTellSound is
		// explicitly configured.
		ATrooperEnemy* AudioTrooper = AudioWorld->SpawnActor<ATrooperEnemy>();
		if (TestNotNull(TEXT("ATrooperEnemy should spawn into the audio test World"), AudioTrooper))
		{
			USoundWave* ConfiguredSound = NewObject<USoundWave>();
			AudioTrooper->AttackTellSound = ConfiguredSound;
			AdvanceToAttack(AudioTrooper, ZeroDistanceLocation);
			if (TestNotNull(TEXT("Entering Attack with a configured AttackTellSound should spawn an audio cue"),
				AudioTrooper->AttackTellAudioComponent.Get()))
			{
				TestEqual(TEXT("The spawned audio cue should play the configured AttackTellSound, not some other sound"),
					static_cast<USoundBase*>(AudioTrooper->AttackTellAudioComponent->Sound.Get()),
					static_cast<USoundBase*>(ConfiguredSound));
			}
		}

		// (o) AttackTellSound now defaults to a real placeholder asset (constructor's
		// AttackTellSoundFinder, issue #30 AC: "a distinct sound effect plays" out of
		// the box) - not left unset.
		ATrooperEnemy* DefaultSoundTrooper = AudioWorld->SpawnActor<ATrooperEnemy>();
		if (TestNotNull(TEXT("ATrooperEnemy should spawn into the default-audio test World"), DefaultSoundTrooper))
		{
			TestFalse(TEXT("AttackTellSound should default to a configured placeholder asset, not be left unset"),
				DefaultSoundTrooper->AttackTellSound.IsNull());
			// Compared against live sibling instances, not hardcoded path literals, so
			// this stays correct if ASniperEnemy's/ABomberEnemy's own defaults ever change.
			ASniperEnemy* SniperRef = NewObject<ASniperEnemy>();
			ABomberEnemy* BomberRef = NewObject<ABomberEnemy>();
			TestNotEqual(TEXT("Trooper's default tell must differ from Sniper's actual current default, so all three enemies are audibly distinct"),
				DefaultSoundTrooper->AttackTellSound.ToSoftObjectPath().ToString(),
				SniperRef->AttackTellSound.ToSoftObjectPath().ToString());
			TestNotEqual(TEXT("Trooper's default tell must differ from Bomber's actual current default, so all three enemies are audibly distinct"),
				DefaultSoundTrooper->AttackTellSound.ToSoftObjectPath().ToString(),
				BomberRef->AttackTellSound.ToSoftObjectPath().ToString());
			AdvanceToAttack(DefaultSoundTrooper, ZeroDistanceLocation);
			TestNotNull(TEXT("Entering Attack with the default AttackTellSound should spawn an audio cue"),
				DefaultSoundTrooper->AttackTellAudioComponent.Get());
		}

		// (p) the graceful, no-crash fallback is still exercised for the defensive
		// case an explicit override (Blueprint/Details panel) clears AttackTellSound
		// back to unset. No assertion on the warning log itself (no existing test in
		// this module asserts UE_LOG output).
		ATrooperEnemy* SilentTrooper = AudioWorld->SpawnActor<ATrooperEnemy>();
		if (TestNotNull(TEXT("ATrooperEnemy should spawn into the silent-audio test World"), SilentTrooper))
		{
			SilentTrooper->AttackTellSound = nullptr;
			AdvanceToAttack(SilentTrooper, ZeroDistanceLocation);
			TestNull(TEXT("Entering Attack with AttackTellSound explicitly cleared should not spawn an audio cue"),
				SilentTrooper->AttackTellAudioComponent.Get());
		}

		// (q) Issue #30 AC: the attack-tell audio "is not replayed if the attack is
		// interrupted or the enemy is controlled mid-telegraph". EnemyBase.cpp's
		// state machine only calls OnAttackEntry() once per Alert->Attack
		// transition, so this is currently unreachable via the live FSM - kept as a
		// regression backstop, mirroring KrowdKontrolBomberEnemyTest.cpp case (s).
		// Also proves TR-UPR's rapid-refire trait (AdvanceAttackTelegraph re-arming)
		// doesn't somehow trigger a second audio spawn either, by asserting
		// OnTrooperRayFired's listener count stays 0 after the interrupt. Uses Stun
		// (full-immobilize, bAllowsAttackWhileControlled == false) - Root no longer
		// stops the ray/telegraph as of issue #255, see case (l2) above.
		ATrooperEnemy* ReplayGuardTrooper = AudioWorld->SpawnActor<ATrooperEnemy>();
		if (TestNotNull(TEXT("ATrooperEnemy should spawn into the replay-guard test World"), ReplayGuardTrooper))
		{
			AdvanceToAttack(ReplayGuardTrooper, ZeroDistanceLocation);
			UAudioComponent* FirstAudioComponent = ReplayGuardTrooper->AttackTellAudioComponent.Get();
			if (TestNotNull(TEXT("Entering Attack should spawn the attack-tell audio cue"), FirstAudioComponent))
			{
				UTrooperRayFiredTestListener* ReplayGuardListener = NewObject<UTrooperRayFiredTestListener>();
				ReplayGuardTrooper->OnTrooperRayFired.AddDynamic(ReplayGuardListener, &UTrooperRayFiredTestListener::HandleTrooperRayFired);

				ReplayGuardTrooper->ReceiveControl(EAbilitySlot::Stun); // interrupts mid-telegraph
				TestEqual(TEXT("interrupted enemy is Controlled"),
					static_cast<uint8>(ReplayGuardTrooper->GetEnemyState()), static_cast<uint8>(EEnemyState::Controlled));

				// Further detection checks (e.g. player still in range post-interrupt)
				// must not drive the state machine back into Attack.
				ReplayGuardTrooper->TickCheckDetection(ZeroDistanceLocation);
				ReplayGuardTrooper->TickCheckDetection(ZeroDistanceLocation);
				TestEqual(TEXT("state stays Controlled - no edge back to Attack exists"),
					static_cast<uint8>(ReplayGuardTrooper->GetEnemyState()), static_cast<uint8>(EEnemyState::Controlled));
				TestEqual(TEXT("audio cue is not replaced/re-spawned after the interrupt"),
					ReplayGuardTrooper->AttackTellAudioComponent.Get(), FirstAudioComponent);
				TestEqual(TEXT("no further ray should fire once interrupted, even though AdvanceAttackTelegraph re-arms"),
					ReplayGuardListener->CallCount, 0);
			}
		}
	}

	// (r) OnAttackEntry's sound-spawn call must degrade gracefully for actors without a
	// real UWorld (SpawnSoundAtLocation needs a world context) - a combination this PR
	// makes reachable for the first time via every NewObject-only case above (a)-(l).
	ATrooperEnemy* WorldlessTrooper = NewObject<ATrooperEnemy>();
	if (TestNotNull(TEXT("ATrooperEnemy should construct without a UWorld"), WorldlessTrooper))
	{
		AdvanceToAttack(WorldlessTrooper, ZeroDistanceLocation);
		TestNull(TEXT("SpawnSoundAtLocation should no-op (not crash) for an actor with no real UWorld"),
			WorldlessTrooper->AttackTellAudioComponent.Get());
	}

	// (r) issue #138/#65: expiry-reversion via Root (Trooper's own OnControlledEntry
	// ability, case (c) above). TR-UPR's GetControlledDurationOverrideSeconds now
	// returns an 8s colour-match bonus for Root (issue #65), not the 5s
	// AbilityData::Get(Root).BaseDurationSeconds baseline - read the actually-applied
	// duration off the enemy itself rather than assuming the base duration governs.
	ATrooperEnemy* ExpiryTrooper = NewObject<ATrooperEnemy>();
	AdvanceToAttack(ExpiryTrooper, ZeroDistanceLocation);
	ExpiryTrooper->ReceiveControl(EAbilitySlot::Root); // Attack -> Controlled, 8.0f override
	const float RootDurationSeconds = ExpiryTrooper->GetTotalControlledSeconds();
	TestEqual(TEXT("Root's colour-match override (8.0f) should govern expiry, not the 5s base duration"),
		RootDurationSeconds, 8.0f);
	ExpiryTrooper->TickControlledDuration(RootDurationSeconds - 1.0f);
	TestEqual(TEXT("Trooper should still be Controlled before the Root duration elapses"),
		static_cast<uint8>(ExpiryTrooper->GetEnemyState()), static_cast<uint8>(EEnemyState::Controlled));
	ExpiryTrooper->TickControlledDuration(1.5f);
	TestEqual(TEXT("Trooper should revert to Alert once the Root duration elapses"),
		static_cast<uint8>(ExpiryTrooper->GetEnemyState()), static_cast<uint8>(EEnemyState::Alert));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
