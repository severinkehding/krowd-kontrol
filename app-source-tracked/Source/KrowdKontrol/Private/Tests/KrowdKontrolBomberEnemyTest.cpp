// Confirms ABomberEnemy (issue #15, PRD 03) satisfies B0-0MR's AC, mirroring
// KrowdKontrolSniperEnemyTest.cpp's structure (NewObject + friend access for most
// cases, a real UWorld for (m)/(o)/(p)/(q)/(r) - deliberately not for (n), which
// proves the no-UWorld path doesn't crash). New vs. Sniper: the
// explosion's player-damage hookup is clamped/non-lethal by construction. Case (j) is
// a structural proxy, same caveat as Sniper's own case (j). Cases (p)/(q)/(r) (issue
// #33) mirror Sniper's own (n)/(o)/(p): a configured AttackTellSound spawns an audio
// cue with the matching asset, the constructor's WhiteNoise default also spawns a cue
// out of the box, and an explicitly-cleared AttackTellSound spawns nothing without
// crashing.

#include "Misc/AutomationTest.h"
#include "BomberEnemy.h"
#include "ReservedGameplayColours.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Tests/AutomationEditorCommon.h"
#include "BomberExplodedTestListener.h"
#include "PlayerEnergyComponent.h"
#include "GameFramework/Pawn.h"
#include "Sound/SoundWave.h"
#include "Components/AudioComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKrowdKontrolBomberEnemyTest,
	"KrowdKontrol.Unit.BomberEnemy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKrowdKontrolBomberEnemyTest::RunTest(const FString& Parameters)
{
	ABomberEnemy* Bomber = NewObject<ABomberEnemy>();
	if (!TestNotNull(TEXT("ABomberEnemy should construct"), Bomber))
	{
		return false;
	}

	// (a) distinct sphere mesh, not the shapes other placeholder actors already use.
	UStaticMeshComponent* Mesh = Bomber->MeshComponent;
	if (!TestNotNull(TEXT("ABomberEnemy should have a MeshComponent"), Mesh))
	{
		return false;
	}
	UStaticMesh* StaticMesh = Mesh->GetStaticMesh();
	if (!TestNotNull(TEXT("MeshComponent should have a static mesh assigned"), StaticMesh))
	{
		return false;
	}
	const FString MeshPath = StaticMesh->GetPathName();
	TestEqual(TEXT("mesh is sphere"), MeshPath, FString(TEXT("/Engine/BasicShapes/Sphere.Sphere")));
	TestNotEqual(TEXT("mesh != cube"), MeshPath, FString(TEXT("/Engine/BasicShapes/Cube.Cube")));
	TestNotEqual(TEXT("mesh != cylinder"), MeshPath, FString(TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));
	TestNotEqual(TEXT("mesh != cone"), MeshPath, FString(TEXT("/Engine/BasicShapes/Cone.Cone")));
	TestTrue(TEXT("distinct sphere scale"), Mesh->GetRelativeScale3D().Equals(FVector(1.3f, 1.3f, 1.3f), 0.01f));

	// (b) Orange core-glow at baseline intensity, un-intensified at construction.
	UPointLightComponent* CoreGlow = Bomber->CoreGlowLightComponent;
	if (!TestNotNull(TEXT("ABomberEnemy should have a CoreGlowLightComponent"), CoreGlow))
	{
		return false;
	}
	TestTrue(TEXT("glow colour is reserved Orange"), CoreGlow->GetLightColor().Equals(ReservedGameplayColours::GetOrange(), 0.01f));
	TestEqual(TEXT("glow starts at baseline"), CoreGlow->Intensity, Bomber->CoreGlowBaselineIntensity);
	TestTrue(TEXT("glow attached to mesh"), CoreGlow->GetAttachParent() == Mesh);
	TestEqual(TEXT("glow attenuation radius"), CoreGlow->AttenuationRadius, 300.0f);

	// Drives Idle -> Alert -> Attack via two detection checks - shared below.
	auto AdvanceToAttack = [](ABomberEnemy* TargetBomber, const FVector& PlayerLocation)
	{
		TargetBomber->TickCheckDetection(PlayerLocation); // Idle -> Alert
		TargetBomber->TickCheckDetection(PlayerLocation); // Alert -> Attack
	};

	// (c) advance to Attack, then Fear specifically intensifies the glow.
	const FVector ZeroDistanceLocation(0.0f, 0.0f, 0.0f);
	AdvanceToAttack(Bomber, ZeroDistanceLocation);
	TestEqual(TEXT("in Attack after two zero-distance checks"), static_cast<uint8>(Bomber->GetEnemyState()), static_cast<uint8>(EEnemyState::Attack));
	Bomber->ReceiveControl(EAbilitySlot::Fear);
	TestEqual(TEXT("glow intensifies after Fear"), CoreGlow->Intensity, Bomber->CoreGlowIntensifiedIntensity);

	// (d) every non-Fear ability produces no glow response, exhaustive over EAbilitySlot.
	const EAbilitySlot NonFearAbilities[] = {
		EAbilitySlot::Stun, EAbilitySlot::Sleep, EAbilitySlot::Root, EAbilitySlot::Snare };
	for (EAbilitySlot NonFearAbility : NonFearAbilities)
	{
		ABomberEnemy* NonFearBomber = NewObject<ABomberEnemy>();
		AdvanceToAttack(NonFearBomber, ZeroDistanceLocation);
		NonFearBomber->ReceiveControl(NonFearAbility);
		TestEqual(TEXT("glow stays at baseline for non-Fear ability"), NonFearBomber->CoreGlowLightComponent->Intensity, NonFearBomber->CoreGlowBaselineIntensity);
	}

	// (e)/(f) the attack tell is off until Attack is entered, and visibly on
	// (before the explosion fires) once it is - ordering proven explicitly below.
	ABomberEnemy* TellBomber = NewObject<ABomberEnemy>();
	UPointLightComponent* TellLight = TellBomber->AttackTellLightComponent;
	if (!TestNotNull(TEXT("ABomberEnemy should have an AttackTellLightComponent"), TellLight))
	{
		return false;
	}
	TestEqual(TEXT("tell off before Attack"), TellLight->Intensity, 0.0f);
	TestTrue(TEXT("tell attached to mesh"), TellLight->GetAttachParent() == TellBomber->MeshComponent);
	TestEqual(TEXT("tell attenuation radius"), TellLight->AttenuationRadius, 300.0f);

	AdvanceToAttack(TellBomber, ZeroDistanceLocation);
	TestEqual(TEXT("tell reaches configured intensity once Attack entered"), TellLight->Intensity, TellBomber->AttackTellIntensity);
	UBomberExplodedTestListener* ExplodedListener = NewObject<UBomberExplodedTestListener>();
	TellBomber->OnBomberExploded.AddDynamic(ExplodedListener, &UBomberExplodedTestListener::HandleBomberExploded);
	TestEqual(TEXT("explosion not fired yet - tell precedes it"), ExplodedListener->CallCount, 0);

	// (g) the telegraph elapsing fires the explosion exactly once.
	TellBomber->AdvanceAttackTelegraph(TellBomber->AttackTelegraphSeconds);
	TestEqual(TEXT("explosion fired exactly once"), ExplodedListener->CallCount, 1);

	// (h) advancing again after exploding does not re-fire.
	TellBomber->AdvanceAttackTelegraph(1.0f);
	TestEqual(TEXT("no re-fire on later advance"), ExplodedListener->CallCount, 1);

	// (g2) the telegraph accumulates across partial advances; the final advance
	// overshoots AttackTelegraphSeconds (2.0f) by a float32-rounding-safe margin
	// (0.8+0.7+0.8=2.3) rather than landing exactly on the boundary - IEEE-754
	// accumulation leaves a nonzero residual there (PR #117 review, issue #17).
	ABomberEnemy* AccumulatingBomber = NewObject<ABomberEnemy>();
	AdvanceToAttack(AccumulatingBomber, ZeroDistanceLocation);
	UBomberExplodedTestListener* AccumulatingListener = NewObject<UBomberExplodedTestListener>();
	AccumulatingBomber->OnBomberExploded.AddDynamic(AccumulatingListener, &UBomberExplodedTestListener::HandleBomberExploded);
	AccumulatingBomber->AdvanceAttackTelegraph(0.8f);
	AccumulatingBomber->AdvanceAttackTelegraph(0.7f);
	TestEqual(TEXT("partial advances below threshold don't fire"), AccumulatingListener->CallCount, 0);
	AccumulatingBomber->AdvanceAttackTelegraph(0.8f); // 0.8 + 0.7 + 0.8 = 2.3, safely past 2.0
	TestEqual(TEXT("accumulated advances crossing threshold fire"), AccumulatingListener->CallCount, 1);

	// (i) advancing the telegraph while not in Attack is a no-op.
	ABomberEnemy* IdleBomber = NewObject<ABomberEnemy>();
	UBomberExplodedTestListener* IdleListener = NewObject<UBomberExplodedTestListener>();
	IdleBomber->OnBomberExploded.AddDynamic(IdleListener, &UBomberExplodedTestListener::HandleBomberExploded);
	IdleBomber->AdvanceAttackTelegraph(999.0f);
	TestEqual(TEXT("no-op while not in Attack"), IdleListener->CallCount, 0);

	// (j) short attack range: a mid-range distance (well within DetectionRangeUnits)
	// does NOT reach Attack - the inverse of ASniperEnemy's own structural-proxy case.
	ABomberEnemy* ShortRangeBomber = NewObject<ABomberEnemy>();
	TestEqual(TEXT("short-range value"), ShortRangeBomber->GetAttackRangeUnits(), 150.0f);
	const FVector MidRangeLocation(800.0f, 0.0f, 0.0f);
	AdvanceToAttack(ShortRangeBomber, MidRangeLocation); // Alert reached (800 <= 1500), Attack not (800 > 150)
	TestEqual(TEXT("short range doesn't reach Attack at mid-range"), static_cast<uint8>(ShortRangeBomber->GetEnemyState()), static_cast<uint8>(EEnemyState::Alert));

	// (k) the attack tell colour must not collide with any reserved gameplay colour.
	TestFalse(TEXT("tell colour doesn't collide with a reserved colour"),
		ReservedGameplayColours::GetAll().ContainsByPredicate(
			[TellLight](const FLinearColor& Reserved) { return Reserved.Equals(TellLight->GetLightColor(), 0.01f); }));

	// (l) ReceiveControl mid-telegraph clears the tell and the explosion never fires.
	ABomberEnemy* InterruptedBomber = NewObject<ABomberEnemy>();
	AdvanceToAttack(InterruptedBomber, ZeroDistanceLocation);
	TestTrue(TEXT("tell on before interrupt"), InterruptedBomber->AttackTellLightComponent->Intensity > 0.0f);
	InterruptedBomber->ReceiveControl(EAbilitySlot::Sleep);
	TestEqual(TEXT("tell cleared once interrupted"), InterruptedBomber->AttackTellLightComponent->Intensity, 0.0f);
	UBomberExplodedTestListener* InterruptedListener = NewObject<UBomberExplodedTestListener>();
	InterruptedBomber->OnBomberExploded.AddDynamic(InterruptedListener, &UBomberExplodedTestListener::HandleBomberExploded);
	InterruptedBomber->AdvanceAttackTelegraph(InterruptedBomber->AttackTelegraphSeconds);
	TestEqual(TEXT("interrupted explosion never fires"), InterruptedListener->CallCount, 0);

	// (l2) GetMovementSpeedUnitsPerSecond() override returns the declared
	// MovementSpeed (200.0f), not AEnemyBase's own base default (600.0f) - the
	// direct proof that issue #122's "per-type speeds are actually read" holds.
	ABomberEnemy* SpeedBomber = NewObject<ABomberEnemy>();
	TestEqual(TEXT("Bomber's movement speed override returns its declared MovementSpeed"),
		SpeedBomber->GetMovementSpeedUnitsPerSecond(), SpeedBomber->MovementSpeed);
	TestEqual(TEXT("Bomber's declared MovementSpeed is 200.0f"), SpeedBomber->MovementSpeed, 200.0f);
	TestNotEqual(TEXT("Bomber's movement speed differs from AEnemyBase's base default"),
		SpeedBomber->GetMovementSpeedUnitsPerSecond(), 600.0f);

	// (l3) driving TickChaseMovement (friend-accessible, same as TickCheckDetection)
	// during Alert advances the bomber at exactly its own 200 u/s, not 600 u/s -
	// proves the override is genuinely consulted by the tick step, not just present.
	ABomberEnemy* ChasingBomber = NewObject<ABomberEnemy>();
	const FVector FarPlayerLocation(1000.0f, 0.0f, 0.0f);
	ChasingBomber->TickCheckDetection(FarPlayerLocation); // Idle -> Alert (1000 <= 1500)
	TestEqual(TEXT("precondition: bomber is Alert"),
		static_cast<uint8>(ChasingBomber->GetEnemyState()), static_cast<uint8>(EEnemyState::Alert));
	const FVector BeforeChase = ChasingBomber->GetActorLocation();
	ChasingBomber->TickChaseMovement(FarPlayerLocation, 0.5f);
	const float DistanceMoved = FVector::Dist(ChasingBomber->GetActorLocation(), BeforeChase);
	TestEqual(TEXT("bomber chase advances at its own MovementSpeed * DeltaSeconds"),
		DistanceMoved, ChasingBomber->MovementSpeed * 0.5f);

	// (m) the real Tick() override wires the telegraph into the per-frame loop.
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	if (TestNotNull(TEXT("CreateNewMap should return a valid World"), World))
	{
		ABomberEnemy* TickedBomber = World->SpawnActor<ABomberEnemy>();
		if (TestNotNull(TEXT("ABomberEnemy should spawn into the test World"), TickedBomber))
		{
			AdvanceToAttack(TickedBomber, ZeroDistanceLocation);
			UBomberExplodedTestListener* TickedListener = NewObject<UBomberExplodedTestListener>();
			TickedBomber->OnBomberExploded.AddDynamic(TickedListener, &UBomberExplodedTestListener::HandleBomberExploded);
			TickedBomber->Tick(TickedBomber->AttackTelegraphSeconds);
			TestEqual(TEXT("Tick() drives the telegraph through to firing"), TickedListener->CallCount, 1);
		}
		// (n) TriggerExplosion doesn't crash with no UWorld (GetWorld() null guard);
		// OnBomberExploded still fires even though no player exists to damage.
		ABomberEnemy* WorldlessBomber = NewObject<ABomberEnemy>();
		AdvanceToAttack(WorldlessBomber, ZeroDistanceLocation);
		UBomberExplodedTestListener* WorldlessListener = NewObject<UBomberExplodedTestListener>();
		WorldlessBomber->OnBomberExploded.AddDynamic(WorldlessListener, &UBomberExplodedTestListener::HandleBomberExploded);
		WorldlessBomber->AdvanceAttackTelegraph(WorldlessBomber->AttackTelegraphSeconds);
		TestEqual(TEXT("fires once even with no UWorld/player"), WorldlessListener->CallCount, 1);

		// (o) the core no-kill proof: a real player pawn's UPlayerEnergyComponent
		// takes clamped damage, not the raw (deliberately huge) ExplosionDamageAmount
		// (found via TActorIterator<APawn> - see BomberEnemy.cpp's TriggerExplosion).
		APawn* PlayerPawn = World->SpawnActor<APawn>();
		if (TestNotNull(TEXT("Player pawn should spawn into the test World"), PlayerPawn))
		{
			UPlayerEnergyComponent* Energy = NewObject<UPlayerEnergyComponent>(PlayerPawn);
			Energy->RegisterComponent();
			// Friend-seeded (no live BeginPlay()), same idiom KrowdKontrolEnergyMeterWidgetTest.cpp uses.
			Energy->CurrentEnergy = Energy->MaxEnergy;
			ABomberEnemy* AttackingBomber = World->SpawnActor<ABomberEnemy>();
			if (TestNotNull(TEXT("ABomberEnemy should spawn into the test World"), AttackingBomber))
			{
				AdvanceToAttack(AttackingBomber, ZeroDistanceLocation);
				AttackingBomber->AdvanceAttackTelegraph(AttackingBomber->AttackTelegraphSeconds);
				TestEqual(TEXT("energy drops by MaxDamagePerHit, not raw ExplosionDamageAmount"), Energy->GetCurrentEnergy(), Energy->MaxEnergy - Energy->MaxDamagePerHit);
				TestTrue(TEXT("energy never driven negative/lethal"), Energy->GetCurrentEnergy() >= 0.0f);
			}
		}
	}

	// (p) OnAttackEntry spawns the attack-tell audio cue when AttackTellSound is
	// configured - needs a real UWorld (SpawnSoundAtLocation resolves it via the
	// actor's outer), same shape as case (m). NewObject<USoundWave>() (no .uasset) is
	// sufficient, per KrowdKontrolSniperEnemyTest.cpp case (n)'s precedent.
	UWorld* AudioWorld = FAutomationEditorCommonUtils::CreateNewMap();
	if (TestNotNull(TEXT("CreateNewMap should return a valid World for the audio test"), AudioWorld))
	{
		ABomberEnemy* AudioBomber = AudioWorld->SpawnActor<ABomberEnemy>();
		if (TestNotNull(TEXT("ABomberEnemy should spawn into the audio test World"), AudioBomber))
		{
			USoundWave* ConfiguredSound = NewObject<USoundWave>();
			AudioBomber->AttackTellSound = ConfiguredSound;
			AdvanceToAttack(AudioBomber, ZeroDistanceLocation);
			if (TestNotNull(TEXT("Entering Attack with a configured AttackTellSound should spawn an audio cue"),
				AudioBomber->AttackTellAudioComponent.Get()))
			{
				TestEqual(TEXT("The spawned audio cue should play the configured AttackTellSound, not some other sound"),
					static_cast<USoundBase*>(AudioBomber->AttackTellAudioComponent->Sound.Get()),
					static_cast<USoundBase*>(ConfiguredSound));
			}
		}
	}

	// (q) AttackTellSound now defaults to a real placeholder asset (constructor's
	// AttackTellSoundFinder, issue #33 AC: "a distinct sound effect plays" out of the
	// box) rather than being left unset, so a freshly spawned, unconfigured
	// ABomberEnemy must actually spawn an audio cue on Attack entry.
	UWorld* DefaultSoundWorld = FAutomationEditorCommonUtils::CreateNewMap();
	if (TestNotNull(TEXT("CreateNewMap should return a valid World for the default-audio test"), DefaultSoundWorld))
	{
		ABomberEnemy* DefaultSoundBomber = DefaultSoundWorld->SpawnActor<ABomberEnemy>();
		if (TestNotNull(TEXT("ABomberEnemy should spawn into the default-audio test World"), DefaultSoundBomber))
		{
			TestFalse(TEXT("AttackTellSound should default to a configured placeholder asset, not be left unset"),
				DefaultSoundBomber->AttackTellSound.IsNull());
			TestEqual(TEXT("AttackTellSound should default to the WhiteNoise placeholder, not some other asset"),
				DefaultSoundBomber->AttackTellSound.ToSoftObjectPath().ToString(),
				FString(TEXT("/Engine/EngineSounds/WhiteNoise.WhiteNoise")));
			TestNotEqual(TEXT("Bomber's default tell must differ from Sniper's, so the two enemies are audibly distinct"),
				DefaultSoundBomber->AttackTellSound.ToSoftObjectPath().ToString(),
				FString(TEXT("/Engine/EngineSounds/1kSineTonePing.1kSineTonePing")));
			AdvanceToAttack(DefaultSoundBomber, ZeroDistanceLocation);
			TestNotNull(TEXT("Entering Attack with the default AttackTellSound should spawn an audio cue"),
				DefaultSoundBomber->AttackTellAudioComponent.Get());
		}
	}

	// (r) the graceful, no-crash fallback is still exercised for the defensive case an
	// explicit override (Blueprint/Details panel) clears AttackTellSound back to unset -
	// same placeholder-first shape UMusicSubsystem's CalmTrack/CombatTrack document. No
	// assertion on the warning log itself (no existing test in this module asserts
	// UE_LOG output).
	UWorld* SilentWorld = FAutomationEditorCommonUtils::CreateNewMap();
	if (TestNotNull(TEXT("CreateNewMap should return a valid World for the silent-audio test"), SilentWorld))
	{
		ABomberEnemy* SilentBomber = SilentWorld->SpawnActor<ABomberEnemy>();
		if (TestNotNull(TEXT("ABomberEnemy should spawn into the silent-audio test World"), SilentBomber))
		{
			SilentBomber->AttackTellSound = nullptr;
			AdvanceToAttack(SilentBomber, ZeroDistanceLocation);
			TestNull(TEXT("Entering Attack with AttackTellSound explicitly cleared should not spawn an audio cue"),
				SilentBomber->AttackTellAudioComponent.Get());
		}
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
