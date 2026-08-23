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
#include "EnemyTypeIndicatorComponent.h"
#include "EnemyType.h"
#include "Sound/SoundWave.h"
#include "Components/AudioComponent.h"
#include "SniperEnemy.h"
#include "BomberEnemy.h"
#include "TrooperEnemy.h"
#include "AbilityData.h"

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

	// (b2) EnemyTypeIndicatorComponent is wired to RU_NNR - PRD 13 REQ-7's colourblind-
	// safe marker. Not implied by the component's own field default (which happens to
	// also be RU_NNR) - this proves the constructor's explicit assignment, not the
	// default, is what's in effect.
	TestEqual(TEXT("EnemyTypeIndicatorComponent should report RU_NNR"),
		static_cast<uint8>(Runner->EnemyTypeIndicatorComponent->EnemyType),
		static_cast<uint8>(EEnemyType::RU_NNR));

	// (b3) Elite trim light (issue #19): exists, attached to MeshComponent, colour
	// non-reserved. Checked here (early, on the original un-GC'd Runner) rather than
	// at the end of this test after dozens more NewObject<>() instances and several
	// CreateNewMap() calls have run - a NewObject<>()-constructed actor held only by
	// a local pointer has no GC roots, and asserting on it this late risked it having
	// already been collected by an incidental GC pass triggered by the later, heavier
	// cases (reproduced empirically: intermittent null failures when checked late).
	if (TestNotNull(TEXT("ARunnerEnemy should have an EliteTrimLightComponent"), Runner->EliteTrimLightComponent.Get()))
	{
		TestTrue(TEXT("EliteTrimLightComponent should be attached to MeshComponent"),
			Runner->EliteTrimLightComponent->GetAttachParent() == Mesh);
		TestFalse(TEXT("EliteTrimLightComponent colour should not collide with a reserved gameplay-information colour"),
			ReservedGameplayColours::GetAll().ContainsByPredicate(
				[Runner](const FLinearColor& Reserved) { return Reserved.Equals(Runner->EliteTrimLightComponent->GetLightColor(), 0.01f); }));
	}

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
	TestEqual(TEXT("Attack tell should reach configured intensity once Attack is entered"),
		TellLight->Intensity, TellRunner->AttackTellIntensity);

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

	// (l-root) issue #255: unlike Snare above, Root-triggered Controlled does NOT
	// clear the attack tell or stop the telegraph - the drain-ray still fires once
	// (bDrainFiredForCurrentAttack still latches), but firing itself is not silenced
	// by entering Controlled, since AbilityData::Get(Root).bAllowsAttackWhileControlled.
	ARunnerEnemy* RootedRunner = NewObject<ARunnerEnemy>();
	AdvanceToAttack(RootedRunner, ZeroDistanceLocation);
	TestTrue(TEXT("(l-root) Attack tell should be visibly on before Root interrupts"),
		RootedRunner->AttackTellLightComponent->Intensity > 0.0f);
	RootedRunner->ReceiveControl(EAbilitySlot::Root);
	TestEqual(TEXT("(l-root) Runner should be Controlled after Root"),
		static_cast<uint8>(RootedRunner->GetEnemyState()), static_cast<uint8>(EEnemyState::Controlled));
	TestTrue(TEXT("(l-root) Attack tell should stay on - Root does not clear it"),
		RootedRunner->AttackTellLightComponent->Intensity > 0.0f);
	UDrainRayFiredTestListener* RootedListener = NewObject<UDrainRayFiredTestListener>();
	RootedRunner->OnRunnerDrainFired.AddDynamic(RootedListener, &UDrainRayFiredTestListener::HandleDrainRayFired);
	RootedRunner->AdvanceAttackTelegraph(RootedRunner->AttackTelegraphSeconds);
	TestEqual(TEXT("(l-root) The drain-ray should still fire once while Controlled by Root, unlike Snare"),
		RootedListener->CallCount, 1);
	RootedRunner->AdvanceAttackTelegraph(RootedRunner->AttackTelegraphSeconds);
	TestEqual(TEXT("(l-root) The one-shot guard should still prevent a second drain-ray while Rooted"),
		RootedListener->CallCount, 1);

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

		// (n) OnAttackEntry spawns the attack-tell audio cue when AttackTellSound is
		// configured. NewObject<USoundWave>() (no .uasset) is sufficient, per
		// KrowdKontrolSniperEnemyTest.cpp case (n)'s precedent. Reuses World, the
		// same one case (m) already created above.
		ARunnerEnemy* AudioRunner = World->SpawnActor<ARunnerEnemy>();
		if (TestNotNull(TEXT("ARunnerEnemy should spawn into the audio test World"), AudioRunner))
		{
			USoundWave* ConfiguredSound = NewObject<USoundWave>();
			AudioRunner->AttackTellSound = ConfiguredSound;
			AdvanceToAttack(AudioRunner, ZeroDistanceLocation);
			if (TestNotNull(TEXT("Entering Attack with a configured AttackTellSound should spawn an audio cue"),
				AudioRunner->AttackTellAudioComponent.Get()))
			{
				TestEqual(TEXT("The spawned audio cue should play the configured AttackTellSound, not some other sound"),
					static_cast<USoundBase*>(AudioRunner->AttackTellAudioComponent->Sound.Get()),
					static_cast<USoundBase*>(ConfiguredSound));
			}
		}

		// (o) AttackTellSound defaults to a real placeholder asset (constructor's
		// AttackTellSoundFinder, issue #28 AC: "a distinct sound effect plays" out of
		// the box) rather than being left unset, so a freshly spawned, unconfigured
		// ARunnerEnemy must actually spawn an audio cue on Attack entry, and that
		// default must differ from all 3 siblings' own live defaults - compared
		// against NewObject<>() instances, not hardcoded path strings, per PR #145's
		// review-fix precedent.
		ARunnerEnemy* DefaultSoundRunner = World->SpawnActor<ARunnerEnemy>();
		if (TestNotNull(TEXT("ARunnerEnemy should spawn into the default-audio test World"), DefaultSoundRunner))
		{
			TestFalse(TEXT("AttackTellSound should default to a configured placeholder asset, not be left unset"),
				DefaultSoundRunner->AttackTellSound.IsNull());
			TestEqual(TEXT("AttackTellSound should default to the CompileFailed placeholder, not some other asset"),
				DefaultSoundRunner->AttackTellSound.ToSoftObjectPath().ToString(),
				FString(TEXT("/Engine/EditorSounds/Notifications/CompileFailed.CompileFailed")));

			ASniperEnemy* SiblingSniper = NewObject<ASniperEnemy>();
			ABomberEnemy* SiblingBomber = NewObject<ABomberEnemy>();
			ATrooperEnemy* SiblingTrooper = NewObject<ATrooperEnemy>();
			TestNotEqual(TEXT("Runner's default tell must differ from Sniper's live default, so the two enemies are audibly distinct"),
				DefaultSoundRunner->AttackTellSound.ToSoftObjectPath().ToString(),
				SiblingSniper->AttackTellSound.ToSoftObjectPath().ToString());
			TestNotEqual(TEXT("Runner's default tell must differ from Bomber's live default, so the two enemies are audibly distinct"),
				DefaultSoundRunner->AttackTellSound.ToSoftObjectPath().ToString(),
				SiblingBomber->AttackTellSound.ToSoftObjectPath().ToString());
			TestNotEqual(TEXT("Runner's default tell must differ from Trooper's live default, so the two enemies are audibly distinct"),
				DefaultSoundRunner->AttackTellSound.ToSoftObjectPath().ToString(),
				SiblingTrooper->AttackTellSound.ToSoftObjectPath().ToString());

			AdvanceToAttack(DefaultSoundRunner, ZeroDistanceLocation);
			TestNotNull(TEXT("Entering Attack with the default AttackTellSound should spawn an audio cue"),
				DefaultSoundRunner->AttackTellAudioComponent.Get());
		}

		// (p) the graceful, no-crash fallback is still exercised for the defensive
		// case an explicit override (Blueprint/Details panel) clears AttackTellSound
		// back to unset. No assertion on the warning log itself (no existing test in
		// this module asserts UE_LOG output).
		ARunnerEnemy* SilentRunner = World->SpawnActor<ARunnerEnemy>();
		if (TestNotNull(TEXT("ARunnerEnemy should spawn into the silent-audio test World"), SilentRunner))
		{
			SilentRunner->AttackTellSound = nullptr;
			AdvanceToAttack(SilentRunner, ZeroDistanceLocation);
			TestNull(TEXT("Entering Attack with AttackTellSound explicitly cleared should not spawn an audio cue"),
				SilentRunner->AttackTellAudioComponent.Get());
		}

		// (q) Issue #28 AC: the attack-tell audio "is not replayed if the attack is
		// interrupted or the enemy is controlled mid-telegraph". AEnemyBase.cpp's
		// state machine is strictly linear (Idle->Alert->Attack->Controlled->Banked,
		// no edges back) and AdvanceToAttack() itself guards on CurrentState ==
		// Alert, so once ReceiveControl() has moved an actor to Controlled
		// mid-telegraph, no further TickCheckDetection() call can ever drive it back
		// into Attack and re-invoke OnAttackEntry() - structurally, not just "in
		// practice". This proves the audio cue captured on first entry is never
		// replaced/re-spawned.
		ARunnerEnemy* ReplayGuardRunner = World->SpawnActor<ARunnerEnemy>();
		if (TestNotNull(TEXT("ARunnerEnemy should spawn into the replay-guard test World"), ReplayGuardRunner))
		{
			AdvanceToAttack(ReplayGuardRunner, ZeroDistanceLocation);
			UAudioComponent* FirstAudioComponent = ReplayGuardRunner->AttackTellAudioComponent.Get();
			if (TestNotNull(TEXT("Entering Attack should spawn the attack-tell audio cue"), FirstAudioComponent))
			{
				ReplayGuardRunner->ReceiveControl(EAbilitySlot::Snare); // interrupts mid-telegraph
				TestEqual(TEXT("interrupted enemy is Controlled"),
					static_cast<uint8>(ReplayGuardRunner->GetEnemyState()), static_cast<uint8>(EEnemyState::Controlled));

				// Further detection checks (e.g. player still in range post-interrupt)
				// must not drive the state machine back into Attack.
				ReplayGuardRunner->TickCheckDetection(ZeroDistanceLocation);
				ReplayGuardRunner->TickCheckDetection(ZeroDistanceLocation);
				TestEqual(TEXT("state stays Controlled - no edge back to Attack exists"),
					static_cast<uint8>(ReplayGuardRunner->GetEnemyState()), static_cast<uint8>(EEnemyState::Controlled));
				TestEqual(TEXT("audio cue is not replaced/re-spawned after the interrupt"),
					ReplayGuardRunner->AttackTellAudioComponent.Get(), FirstAudioComponent);
			}
		}
	}

	// (r) OnAttackEntry's sound-spawn call must degrade gracefully for actors without a
	// real UWorld (SpawnSoundAtLocation needs a world context) - a combination this PR
	// makes reachable for the first time via every NewObject-only case above (a)-(l).
	ARunnerEnemy* WorldlessRunner = NewObject<ARunnerEnemy>();
	if (TestNotNull(TEXT("ARunnerEnemy should construct without a UWorld"), WorldlessRunner))
	{
		AdvanceToAttack(WorldlessRunner, ZeroDistanceLocation);
		TestNull(TEXT("SpawnSoundAtLocation should no-op (not crash) for an actor with no real UWorld"),
			WorldlessRunner->AttackTellAudioComponent.Get());
	}

	// (s) issue #138: expiry-reversion via Snare (Runner's own OnControlledEntry
	// ability, case (c) above), using AbilityData::Get(Snare).BaseDurationSeconds
	// directly rather than a hardcoded magic number - no per-enemy override exists for
	// Runner, so the base duration governs.
	ARunnerEnemy* ExpiryRunner = NewObject<ARunnerEnemy>();
	AdvanceToAttack(ExpiryRunner, ZeroDistanceLocation);
	ExpiryRunner->ReceiveControl(EAbilitySlot::Snare); // Attack -> Controlled
	const float SnareDurationSeconds = AbilityData::Get(EAbilitySlot::Snare).BaseDurationSeconds;
	ExpiryRunner->TickControlledDuration(SnareDurationSeconds - 1.0f);
	TestEqual(TEXT("Runner should still be Controlled before the Snare duration elapses"),
		static_cast<uint8>(ExpiryRunner->GetEnemyState()), static_cast<uint8>(EEnemyState::Controlled));
	ExpiryRunner->TickControlledDuration(1.5f);
	TestEqual(TEXT("Runner should revert to Alert once the Snare duration elapses"),
		static_cast<uint8>(ExpiryRunner->GetEnemyState()), static_cast<uint8>(EEnemyState::Alert));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
