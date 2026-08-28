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
#include "Sound/SoundWave.h"
#include "Components/AudioComponent.h"
#include "AbilityData.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "EnemyTypeIndicatorComponent.h"

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

	// (b2) Elite trim light (issue #19): exists, attached to MeshComponent, colour
	// non-reserved. Checked here (early, on the original un-GC'd Sniper) rather than
	// at the end of this test after dozens more NewObject<>() instances and several
	// CreateNewMap() calls have run - a NewObject<>()-constructed actor held only by
	// a local pointer has no GC roots, and asserting on it this late risked it having
	// already been collected by an incidental GC pass triggered by the later, heavier
	// cases (reproduced empirically: intermittent null failures when checked late).
	if (TestNotNull(TEXT("ASniperEnemy should have an EliteTrimLightComponent"), Sniper->EliteTrimLightComponent.Get()))
	{
		TestTrue(TEXT("EliteTrimLightComponent should be attached to MeshComponent"),
			Sniper->EliteTrimLightComponent->GetAttachParent() == Mesh);
		TestFalse(TEXT("EliteTrimLightComponent colour should not collide with a reserved gameplay-information colour"),
			ReservedGameplayColours::GetAll().ContainsByPredicate(
				[Sniper](const FLinearColor& Reserved) { return Reserved.Equals(Sniper->EliteTrimLightComponent->GetLightColor(), 0.01f); }));
	}

	// (b3) Body chain-colour tint (issue #316): SN-1PR's chain colour is Sleep's colour
	// (AbilityData::GetChainColourForEnemyType), applied to MeshComponent via a lazily-
	// created MID, and ApplyBodyChainColourTint() is idempotent (same MID instance,
	// re-applying the same colour, on a second call).
	Sniper->ApplyBodyChainColourTint();
	const FLinearColor ExpectedBodyColour = AbilityData::GetChainColourForEnemyType(EEnemyType::SN_1PR);
	TestTrue(TEXT("body chain colour matches AbilityData::GetChainColourForEnemyType(SN_1PR)"),
		Sniper->CurrentBodyChainColour.Equals(ExpectedBodyColour, 0.01f));
	UMaterialInstanceDynamic* FirstBodyMID = Sniper->BodyChainColourMaterialInstance;
	if (TestNotNull(TEXT("BodyChainColourMaterialInstance should be created"), FirstBodyMID))
	{
		TestTrue(TEXT("MeshComponent's material 0 is the BodyChainColourMaterialInstance"),
			Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(0)) == FirstBodyMID);
	}
	Sniper->ApplyBodyChainColourTint();
	TestTrue(TEXT("second call reuses the same MID instance"),
		Sniper->BodyChainColourMaterialInstance.Get() == FirstBodyMID);
	TestEqual(TEXT("EnemyTypeIndicatorComponent's own EnemyType is unchanged by the body tint"),
		static_cast<uint8>(Sniper->EnemyTypeIndicatorComponent->EnemyType), static_cast<uint8>(EEnemyType::SN_1PR));

	// Drives a sniper from Idle straight through to Attack via two zero/mid-distance
	// detection checks (Idle -> Alert -> Attack) - shared by every case below that
	// needs a sniper already in Attack.
	auto AdvanceToAttack = [](ASniperEnemy* TargetSniper, const FVector& PlayerLocation)
	{
		TargetSniper->TickCheckDetection(PlayerLocation); // Idle -> Alert
		TargetSniper->TickCheckDetection(PlayerLocation); // Alert -> Attack
	};

	// (c) advance to Attack, then Sleep specifically intensifies the glow.
	const FVector ZeroDistanceLocation(0.0f, 0.0f, 0.0f);
	AdvanceToAttack(Sniper, ZeroDistanceLocation);
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
		AdvanceToAttack(NonSleepSniper, ZeroDistanceLocation);
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

	AdvanceToAttack(TellSniper, ZeroDistanceLocation);
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
	AdvanceToAttack(AccumulatingSniper, ZeroDistanceLocation);
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
	AdvanceToAttack(LongRangeSniper, MidRangeLocation); // Attack reached since 800 <= 1400
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
	AdvanceToAttack(InterruptedSniper, ZeroDistanceLocation);
	TestTrue(TEXT("Attack tell should be visibly on before the interrupt"),
		InterruptedSniper->AttackTellLightComponent->Intensity > 0.0f);
	InterruptedSniper->ReceiveControl(EAbilitySlot::Sleep);
	TestEqual(TEXT("Attack tell should be cleared once Controlled interrupts the attack"),
		InterruptedSniper->AttackTellLightComponent->Intensity, 0.0f);
	USniperShotFiredTestListener* InterruptedListener = NewObject<USniperShotFiredTestListener>();
	InterruptedSniper->OnSniperShotFired.AddDynamic(InterruptedListener, &USniperShotFiredTestListener::HandleSniperShotFired);
	InterruptedSniper->AdvanceAttackTelegraph(InterruptedSniper->AttackTelegraphSeconds);
	TestEqual(TEXT("The interrupted shot should never fire"), InterruptedListener->CallCount, 0);

	// (l2) issue #255: unlike Sleep above, Root-triggered Controlled does NOT clear
	// the attack tell or stop the telegraph - the shot still fires once
	// (bShotFiredForCurrentAttack still latches), but firing itself is not silenced by
	// entering Controlled, since AbilityData::Get(Root).bAllowsAttackWhileControlled.
	ASniperEnemy* RootedSniper = NewObject<ASniperEnemy>();
	AdvanceToAttack(RootedSniper, ZeroDistanceLocation);
	TestTrue(TEXT("(l2) Attack tell should be visibly on before Root interrupts"),
		RootedSniper->AttackTellLightComponent->Intensity > 0.0f);
	RootedSniper->ReceiveControl(EAbilitySlot::Root);
	TestEqual(TEXT("(l2) Sniper should be Controlled after Root"),
		static_cast<uint8>(RootedSniper->GetEnemyState()), static_cast<uint8>(EEnemyState::Controlled));
	TestTrue(TEXT("(l2) Attack tell should stay on - Root does not clear it"),
		RootedSniper->AttackTellLightComponent->Intensity > 0.0f);
	USniperShotFiredTestListener* RootedListener = NewObject<USniperShotFiredTestListener>();
	RootedSniper->OnSniperShotFired.AddDynamic(RootedListener, &USniperShotFiredTestListener::HandleSniperShotFired);
	RootedSniper->AdvanceAttackTelegraph(RootedSniper->AttackTelegraphSeconds);
	TestEqual(TEXT("(l2) A shot should still fire while Controlled by Root, unlike Sleep"),
		RootedListener->CallCount, 1);
	RootedSniper->AdvanceAttackTelegraph(RootedSniper->AttackTelegraphSeconds);
	TestEqual(TEXT("(l2) The one-shot guard should still prevent a second shot while Rooted"),
		RootedListener->CallCount, 1);

	// (l-attack-expired) issue #313 pass-1 review follow-up (HIGH): OnAttackExpired
	// must clear the tell light once the Attack-duration timeout reverts Attack ->
	// Alert unconditionally, mid-telegraph - the same bug class the Controlled ->
	// Alert edge's OnControlledExpired override exists to prevent, but for Attack.
	ASniperEnemy* ExpiredAttackSniper = NewObject<ASniperEnemy>();
	AdvanceToAttack(ExpiredAttackSniper, ZeroDistanceLocation);
	TestTrue(TEXT("(l-attack-expired) Attack tell should be visibly on before the Attack-duration timeout"),
		ExpiredAttackSniper->AttackTellLightComponent->Intensity > 0.0f);
	ExpiredAttackSniper->TickAttackDuration(ExpiredAttackSniper->GetAttackDurationSeconds());
	TestEqual(TEXT("(l-attack-expired) Sniper should be back to Alert once the Attack-duration timeout elapses"),
		static_cast<uint8>(ExpiredAttackSniper->GetEnemyState()), static_cast<uint8>(EEnemyState::Alert));
	TestEqual(TEXT("(l-attack-expired) OnAttackExpired should clear the tell light once the Attack-duration timeout elapses"),
		ExpiredAttackSniper->AttackTellLightComponent->Intensity, 0.0f);

	// (m-snare) issue #254: unlike Root above (which runs its attack unmodified), Snare
	// scales the attack telegraph's elapsed time by ControlledSpeedMultiplier (0.5f) -
	// a full AttackTelegraphSeconds' worth of ticks only advances the telegraph 50% of
	// the way, so the shot has NOT fired yet; a second identical tick brings the
	// cumulative elapsed time up to AttackTelegraphSeconds and the shot fires exactly
	// once. This is Sniper's own independent AdvanceAttackTelegraph copy, mirroring
	// KrowdKontrolBomberEnemyTest.cpp's (m-snare) case for the same behaviour.
	ASniperEnemy* SnaredSniper = NewObject<ASniperEnemy>();
	AdvanceToAttack(SnaredSniper, ZeroDistanceLocation);
	SnaredSniper->ReceiveControl(EAbilitySlot::Snare);
	TestEqual(TEXT("(m-snare) Sniper should be Controlled after Snare"),
		static_cast<uint8>(SnaredSniper->GetEnemyState()), static_cast<uint8>(EEnemyState::Controlled));
	USniperShotFiredTestListener* SnaredListener = NewObject<USniperShotFiredTestListener>();
	SnaredSniper->OnSniperShotFired.AddDynamic(SnaredListener, &USniperShotFiredTestListener::HandleSniperShotFired);
	SnaredSniper->AdvanceAttackTelegraph(SnaredSniper->AttackTelegraphSeconds);
	TestEqual(TEXT("(m-snare) The shot should NOT have fired after only one telegraph's worth of half-speed ticks"),
		SnaredListener->CallCount, 0);
	SnaredSniper->AdvanceAttackTelegraph(SnaredSniper->AttackTelegraphSeconds);
	TestEqual(TEXT("(m-snare) The shot should fire exactly once once cumulative elapsed time (at half speed) reaches AttackTelegraphSeconds"),
		SnaredListener->CallCount, 1);

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
			AdvanceToAttack(TickedSniper, ZeroDistanceLocation);
			USniperShotFiredTestListener* TickedListener = NewObject<USniperShotFiredTestListener>();
			TickedSniper->OnSniperShotFired.AddDynamic(TickedListener, &USniperShotFiredTestListener::HandleSniperShotFired);
			TickedSniper->Tick(TickedSniper->AttackTelegraphSeconds);
			TestEqual(TEXT("Tick() should drive the telegraph through to firing the shot"),
				TickedListener->CallCount, 1);
		}
	}

	// (n) OnAttackEntry spawns the attack-tell audio cue when AttackTellSound is
	// configured - needs a real UWorld (SpawnSoundAtLocation resolves it via the actor's
	// outer), same shape as case (m). NewObject<USoundWave>() (no .uasset) is sufficient,
	// per KrowdKontrolMusicSubsystemTest.cpp case (j)'s precedent.
	UWorld* AudioWorld = FAutomationEditorCommonUtils::CreateNewMap();
	if (TestNotNull(TEXT("CreateNewMap should return a valid World for the audio test"), AudioWorld))
	{
		ASniperEnemy* AudioSniper = AudioWorld->SpawnActor<ASniperEnemy>();
		if (TestNotNull(TEXT("ASniperEnemy should spawn into the audio test World"), AudioSniper))
		{
			USoundWave* ConfiguredSound = NewObject<USoundWave>();
			AudioSniper->AttackTellSound = ConfiguredSound;
			AdvanceToAttack(AudioSniper, ZeroDistanceLocation);
			if (TestNotNull(TEXT("Entering Attack with a configured AttackTellSound should spawn an audio cue"),
				AudioSniper->AttackTellAudioComponent.Get()))
			{
				TestEqual(TEXT("The spawned audio cue should play the configured AttackTellSound, not some other sound"),
					static_cast<USoundBase*>(AudioSniper->AttackTellAudioComponent->Sound.Get()),
					static_cast<USoundBase*>(ConfiguredSound));
			}
		}
	}

	// (o) AttackTellSound now defaults to a real placeholder asset (constructor's
	// AttackTellSoundFinder, issue #36 AC: "a distinct sound effect plays" out of the
	// box) rather than being left unset, so a freshly spawned, unconfigured
	// ASniperEnemy must actually spawn an audio cue on Attack entry.
	UWorld* DefaultSoundWorld = FAutomationEditorCommonUtils::CreateNewMap();
	if (TestNotNull(TEXT("CreateNewMap should return a valid World for the default-audio test"), DefaultSoundWorld))
	{
		ASniperEnemy* DefaultSoundSniper = DefaultSoundWorld->SpawnActor<ASniperEnemy>();
		if (TestNotNull(TEXT("ASniperEnemy should spawn into the default-audio test World"), DefaultSoundSniper))
		{
			TestFalse(TEXT("AttackTellSound should default to a configured placeholder asset, not be left unset"),
				DefaultSoundSniper->AttackTellSound.IsNull());
			AdvanceToAttack(DefaultSoundSniper, ZeroDistanceLocation);
			TestNotNull(TEXT("Entering Attack with the default AttackTellSound should spawn an audio cue"),
				DefaultSoundSniper->AttackTellAudioComponent.Get());
		}
	}

	// (p) the graceful, no-crash fallback is still exercised for the defensive case an
	// explicit override (Blueprint/Details panel) clears AttackTellSound back to unset -
	// same placeholder-first shape UMusicSubsystem's CalmTrack/CombatTrack document. No
	// assertion on the warning log itself (no existing test in this module asserts
	// UE_LOG output).
	UWorld* SilentWorld = FAutomationEditorCommonUtils::CreateNewMap();
	if (TestNotNull(TEXT("CreateNewMap should return a valid World for the silent-audio test"), SilentWorld))
	{
		ASniperEnemy* SilentSniper = SilentWorld->SpawnActor<ASniperEnemy>();
		if (TestNotNull(TEXT("ASniperEnemy should spawn into the silent-audio test World"), SilentSniper))
		{
			SilentSniper->AttackTellSound = nullptr;
			AdvanceToAttack(SilentSniper, ZeroDistanceLocation);
			TestNull(TEXT("Entering Attack with AttackTellSound explicitly cleared should not spawn an audio cue"),
				SilentSniper->AttackTellAudioComponent.Get());
		}
	}

	// (s) issue #138/#121: expiry-reversion via Sleep, and the 7s override specifically
	// (not the 5s AbilityData::Get(Sleep).BaseDurationSeconds baseline) governs it.
	ASniperEnemy* ExpirySniper = NewObject<ASniperEnemy>();
	AdvanceToAttack(ExpirySniper, ZeroDistanceLocation);
	ExpirySniper->ReceiveControl(EAbilitySlot::Sleep); // Attack -> Controlled, 7.0f override
	TestEqual(TEXT("GetTotalControlledSeconds should reflect the 7s Sleep override, not the base duration"),
		ExpirySniper->GetTotalControlledSeconds(), 7.0f);
	TestNotEqual(TEXT("precondition: the Sleep override (7.0f) differs from the base Sleep duration"),
		7.0f, AbilityData::Get(EAbilitySlot::Sleep).BaseDurationSeconds);
	ExpirySniper->TickControlledDuration(6.9f);
	TestEqual(TEXT("Sniper should still be Controlled just under the 7s override"),
		static_cast<uint8>(ExpirySniper->GetEnemyState()), static_cast<uint8>(EEnemyState::Controlled));
	ExpirySniper->TickControlledDuration(0.2f); // total 7.1f, past the 7s override
	TestEqual(TEXT("Sniper should revert to Alert once the 7s Sleep override elapses"),
		static_cast<uint8>(ExpirySniper->GetEnemyState()), static_cast<uint8>(EEnemyState::Alert));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
