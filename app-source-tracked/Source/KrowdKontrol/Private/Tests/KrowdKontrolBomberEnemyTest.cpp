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
#include "AbilityData.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "EnemyTypeIndicatorComponent.h"

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

	// (b2) Elite trim light (issue #19): exists, attached to MeshComponent, colour
	// non-reserved. Checked here (early, on the original un-GC'd Bomber) rather than
	// at the end of this test after dozens more NewObject<>() instances and several
	// CreateNewMap() calls have run - a NewObject<>()-constructed actor held only by
	// a local pointer has no GC roots, and asserting on it this late risked it having
	// already been collected by an incidental GC pass triggered by the later, heavier
	// cases (reproduced empirically: intermittent null failures when checked late).
	if (TestNotNull(TEXT("ABomberEnemy should have an EliteTrimLightComponent"), Bomber->EliteTrimLightComponent.Get()))
	{
		TestTrue(TEXT("EliteTrimLightComponent should be attached to MeshComponent"),
			Bomber->EliteTrimLightComponent->GetAttachParent() == Mesh);
		TestFalse(TEXT("EliteTrimLightComponent colour should not collide with a reserved gameplay-information colour"),
			ReservedGameplayColours::GetAll().ContainsByPredicate(
				[Bomber](const FLinearColor& Reserved) { return Reserved.Equals(Bomber->EliteTrimLightComponent->GetLightColor(), 0.01f); }));
	}

	// (b3) Body chain-colour tint (issue #316): B0-0MR's chain colour is Fear's colour
	// (AbilityData::GetChainColourForEnemyType), applied to MeshComponent via a lazily-
	// created MID, and ApplyBodyChainColourTint() is idempotent (same MID instance,
	// re-applying the same colour, on a second call).
	Bomber->ApplyBodyChainColourTint();
	const FLinearColor ExpectedBodyColour = AbilityData::GetChainColourForEnemyType(EEnemyType::B0_0MR);
	TestTrue(TEXT("body chain colour matches AbilityData::GetChainColourForEnemyType(B0_0MR)"),
		Bomber->CurrentBodyChainColour.Equals(ExpectedBodyColour, 0.01f));
	UMaterialInstanceDynamic* FirstBodyMID = Bomber->BodyChainColourMaterialInstance;
	if (TestNotNull(TEXT("BodyChainColourMaterialInstance should be created"), FirstBodyMID))
	{
		TestTrue(TEXT("MeshComponent's material 0 is the BodyChainColourMaterialInstance"),
			Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(0)) == FirstBodyMID);
	}
	Bomber->ApplyBodyChainColourTint();
	TestTrue(TEXT("second call reuses the same MID instance"),
		Bomber->BodyChainColourMaterialInstance.Get() == FirstBodyMID);
	TestEqual(TEXT("EnemyTypeIndicatorComponent's own EnemyType is unchanged by the body tint"),
		static_cast<uint8>(Bomber->EnemyTypeIndicatorComponent->EnemyType), static_cast<uint8>(EEnemyType::B0_0MR));

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

	// (l) ReceiveControl mid-telegraph clears the tell and the explosion never fires;
	// also proves GetAttackTelegraphStage()'s claimed "stale read, guarded by state"
	// contract (BomberEnemy.h's comment on that accessor) - the stage should hold its
	// last value across the interrupt, not reset.
	ABomberEnemy* InterruptedBomber = NewObject<ABomberEnemy>();
	AdvanceToAttack(InterruptedBomber, ZeroDistanceLocation);
	TestTrue(TEXT("tell on before interrupt"), InterruptedBomber->AttackTellLightComponent->Intensity > 0.0f);
	InterruptedBomber->AdvanceAttackTelegraph(InterruptedBomber->AttackTelegraphSeconds * 0.5f); // reach Mid
	const EBomberTelegraphStage StageBeforeInterrupt = InterruptedBomber->GetAttackTelegraphStage();
	TestEqual(TEXT("precondition: interrupted bomber reached Mid before interruption"),
		static_cast<uint8>(StageBeforeInterrupt), static_cast<uint8>(EBomberTelegraphStage::Mid));
	InterruptedBomber->ReceiveControl(EAbilitySlot::Sleep);
	TestEqual(TEXT("tell cleared once interrupted"), InterruptedBomber->AttackTellLightComponent->Intensity, 0.0f);
	TestEqual(TEXT("telegraph stage is a stale read after interruption, not reset"),
		static_cast<uint8>(InterruptedBomber->GetAttackTelegraphStage()), static_cast<uint8>(StageBeforeInterrupt));
	UBomberExplodedTestListener* InterruptedListener = NewObject<UBomberExplodedTestListener>();
	InterruptedBomber->OnBomberExploded.AddDynamic(InterruptedListener, &UBomberExplodedTestListener::HandleBomberExploded);
	InterruptedBomber->AdvanceAttackTelegraph(InterruptedBomber->AttackTelegraphSeconds);
	TestEqual(TEXT("interrupted explosion never fires"), InterruptedListener->CallCount, 0);

	// (l-root) issue #255: unlike Sleep above, Root-triggered Controlled does NOT
	// clear the attack tell or stop the telegraph - the explosion still fires once
	// (bExplodedForCurrentAttack still latches), but firing itself is not silenced by
	// entering Controlled, since AbilityData::Get(Root).bAllowsAttackWhileControlled.
	ABomberEnemy* RootedBomber = NewObject<ABomberEnemy>();
	AdvanceToAttack(RootedBomber, ZeroDistanceLocation);
	TestTrue(TEXT("(l-root) Attack tell should be visibly on before Root interrupts"),
		RootedBomber->AttackTellLightComponent->Intensity > 0.0f);
	RootedBomber->ReceiveControl(EAbilitySlot::Root);
	TestEqual(TEXT("(l-root) Bomber should be Controlled after Root"),
		static_cast<uint8>(RootedBomber->GetEnemyState()), static_cast<uint8>(EEnemyState::Controlled));
	TestTrue(TEXT("(l-root) Attack tell should stay on - Root does not clear it"),
		RootedBomber->AttackTellLightComponent->Intensity > 0.0f);
	UBomberExplodedTestListener* RootedListener = NewObject<UBomberExplodedTestListener>();
	RootedBomber->OnBomberExploded.AddDynamic(RootedListener, &UBomberExplodedTestListener::HandleBomberExploded);
	RootedBomber->AdvanceAttackTelegraph(RootedBomber->AttackTelegraphSeconds);
	TestEqual(TEXT("(l-root) The explosion should still fire once while Controlled by Root, unlike Sleep"),
		RootedListener->CallCount, 1);
	RootedBomber->AdvanceAttackTelegraph(RootedBomber->AttackTelegraphSeconds);
	TestEqual(TEXT("(l-root) The one-shot guard should still prevent a second explosion while Rooted"),
		RootedListener->CallCount, 1);

	// (l-root) OnControlledExpired regression (pass-1 review follow-up, issue #255):
	// the tell light must not stay lit forever once Root's Controlled window naturally
	// expires without the enemy being banked - this is the exact bug OnControlledExpired
	// was added to fix, proven directly rather than just proving its precondition.
	RootedBomber->TickControlledDuration(AbilityData::Get(EAbilitySlot::Root).BaseDurationSeconds);
	TestEqual(TEXT("(l-root) Bomber should be back to Alert once Root's Controlled window naturally expires"),
		static_cast<uint8>(RootedBomber->GetEnemyState()), static_cast<uint8>(EEnemyState::Alert));
	TestEqual(TEXT("(l-root) OnControlledExpired should clear the tell light once Root's window ends"),
		RootedBomber->AttackTellLightComponent->Intensity, 0.0f);

	// (l-attack-expired) issue #313 pass-1 review follow-up (HIGH): OnAttackExpired
	// must clear the tell light once the Attack-duration timeout reverts Attack ->
	// Alert unconditionally, mid-telegraph - the same bug OnControlledExpired above
	// exists to prevent, but for the Attack -> Alert edge instead of Controlled -> Alert.
	ABomberEnemy* ExpiredAttackBomber = NewObject<ABomberEnemy>();
	AdvanceToAttack(ExpiredAttackBomber, ZeroDistanceLocation);
	TestTrue(TEXT("(l-attack-expired) Attack tell should be visibly on before the Attack-duration timeout"),
		ExpiredAttackBomber->AttackTellLightComponent->Intensity > 0.0f);
	ExpiredAttackBomber->TickAttackDuration(ExpiredAttackBomber->GetAttackDurationSeconds());
	TestEqual(TEXT("(l-attack-expired) Bomber should be back to Alert once the Attack-duration timeout elapses"),
		static_cast<uint8>(ExpiredAttackBomber->GetEnemyState()), static_cast<uint8>(EEnemyState::Alert));
	TestEqual(TEXT("(l-attack-expired) OnAttackExpired should clear the tell light once the Attack-duration timeout elapses"),
		ExpiredAttackBomber->AttackTellLightComponent->Intensity, 0.0f);

	// (m-snare) issue #254: unlike Root above (which runs its attack unmodified), Snare
	// scales the attack telegraph's elapsed time by ControlledSpeedMultiplier (0.5f) -
	// a full AttackTelegraphSeconds' worth of ticks only advances the telegraph 50% of
	// the way, so the explosion has NOT fired yet; a second identical tick brings the
	// cumulative elapsed time up to AttackTelegraphSeconds and the explosion fires
	// exactly once. This is the concrete, observable proof the slow is real, not just a
	// flag.
	ABomberEnemy* SnaredBomber = NewObject<ABomberEnemy>();
	AdvanceToAttack(SnaredBomber, ZeroDistanceLocation);
	SnaredBomber->ReceiveControl(EAbilitySlot::Snare);
	TestEqual(TEXT("(m-snare) Bomber should be Controlled after Snare"),
		static_cast<uint8>(SnaredBomber->GetEnemyState()), static_cast<uint8>(EEnemyState::Controlled));
	UBomberExplodedTestListener* SnaredListener = NewObject<UBomberExplodedTestListener>();
	SnaredBomber->OnBomberExploded.AddDynamic(SnaredListener, &UBomberExplodedTestListener::HandleBomberExploded);
	SnaredBomber->AdvanceAttackTelegraph(SnaredBomber->AttackTelegraphSeconds);
	TestEqual(TEXT("(m-snare) The explosion should NOT have fired after only one telegraph's worth of half-speed ticks"),
		SnaredListener->CallCount, 0);
	SnaredBomber->AdvanceAttackTelegraph(SnaredBomber->AttackTelegraphSeconds);
	TestEqual(TEXT("(m-snare) The explosion should fire exactly once once cumulative elapsed time (at half speed) reaches AttackTelegraphSeconds"),
		SnaredListener->CallCount, 1);

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
		// Issue #316 (test-coverage review): SpawnActor<>() alone does NOT run
		// BeginPlay() on a bare CreateNewMap() world - both calls below are required
		// (see KrowdKontrolTargetZoneTest.cpp's file comment for why neither alone
		// suffices), made once up front so every actor spawned into World afterward
		// auto-begins-play via the engine's normal flow.
		World->InitializeActorsForPlay(FURL());
		World->SetBegunPlay(true);

		ABomberEnemy* TickedBomber = World->SpawnActor<ABomberEnemy>();
		if (TestNotNull(TEXT("ABomberEnemy should spawn into the test World"), TickedBomber))
		{
			// (b3) above only proves ApplyBodyChainColourTint() works when called
			// directly, not that BeginPlay() actually calls it - this proves the wiring.
			TestNotNull(TEXT("BeginPlay() should have created BodyChainColourMaterialInstance"),
				TickedBomber->BodyChainColourMaterialInstance.Get());
			TestTrue(TEXT("BeginPlay() should have applied B0_0MR's chain colour"),
				TickedBomber->CurrentBodyChainColour.Equals(
					AbilityData::GetChainColourForEnemyType(EEnemyType::B0_0MR), 0.01f));

			AdvanceToAttack(TickedBomber, ZeroDistanceLocation);
			UBomberExplodedTestListener* TickedListener = NewObject<UBomberExplodedTestListener>();
			TickedBomber->OnBomberExploded.AddDynamic(TickedListener, &UBomberExplodedTestListener::HandleBomberExploded);
			TickedBomber->Tick(TickedBomber->AttackTelegraphSeconds);
			// Issue #313 (test-coverage review): proves the base class's own
			// TickAttackDuration (2.5s) hasn't already preempted this telegraph
			// (2.0s) within this same Tick() call - the exact tick-order race the
			// AttackDuration-vs-AttackTelegraphSeconds margin exists to avoid.
			TestEqual(TEXT("Attack-duration timeout must not have preempted the telegraph"),
				static_cast<uint8>(TickedBomber->GetEnemyState()), static_cast<uint8>(EEnemyState::Attack));
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

	// (p)/(q)/(r) OnAttackEntry's audio-cue spawning - needs a real UWorld
	// (SpawnSoundAtLocation resolves it via the actor's outer), same shape as case (m),
	// and shares one World across the three cases the same way (m)/(n)/(o) do above.
	// NewObject<USoundWave>() (no .uasset) is sufficient, per
	// KrowdKontrolSniperEnemyTest.cpp case (n)'s precedent.
	UWorld* AudioWorld = FAutomationEditorCommonUtils::CreateNewMap();
	if (TestNotNull(TEXT("CreateNewMap should return a valid World for the audio tests"), AudioWorld))
	{
		// (p) OnAttackEntry spawns the attack-tell audio cue when AttackTellSound is configured.
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

		// (q) AttackTellSound now defaults to a real placeholder asset (constructor's
		// AttackTellSoundFinder, issue #33 AC: "a distinct sound effect plays" out of the
		// box) rather than being left unset, so a freshly spawned, unconfigured
		// ABomberEnemy must actually spawn an audio cue on Attack entry.
		ABomberEnemy* DefaultSoundBomber = AudioWorld->SpawnActor<ABomberEnemy>();
		if (TestNotNull(TEXT("ABomberEnemy should spawn into the default-audio test World"), DefaultSoundBomber))
		{
			TestFalse(TEXT("AttackTellSound should default to a configured placeholder asset, not be left unset"),
				DefaultSoundBomber->AttackTellSound.IsNull());
			TestEqual(TEXT("AttackTellSound should default to the WhiteNoise placeholder, not some other asset"),
				DefaultSoundBomber->AttackTellSound.ToSoftObjectPath().ToString(),
				FString(TEXT("/Engine/EngineSounds/WhiteNoise.WhiteNoise")));
			// Issue #33 AC: "distinguishable from existing ability-cast/UI sounds". As of
			// this PR the codebase has no ability-cast/UI sound system at all - no
			// USoundBase/USoundCue/USoundWave/UAudioComponent usage anywhere outside
			// BomberEnemy/SniperEnemy/MusicSubsystem - so there is no such asset to
			// compare against yet (tracked separately; see the PR's "Not in scope" note).
			// The one other sound asset hardcoded anywhere in this codebase today is
			// ASniperEnemy's own tell, checked below; this is the strongest distinctness
			// proof currently possible and should gain more entries if a real
			// ability-cast/UI sound is ever hardcoded the same way.
			TestNotEqual(TEXT("Bomber's default tell must differ from Sniper's, so the two enemies are audibly distinct"),
				DefaultSoundBomber->AttackTellSound.ToSoftObjectPath().ToString(),
				FString(TEXT("/Engine/EngineSounds/1kSineTonePing.1kSineTonePing")));
			AdvanceToAttack(DefaultSoundBomber, ZeroDistanceLocation);
			TestNotNull(TEXT("Entering Attack with the default AttackTellSound should spawn an audio cue"),
				DefaultSoundBomber->AttackTellAudioComponent.Get());
		}

		// (r) the graceful, no-crash fallback is still exercised for the defensive case
		// an explicit override (Blueprint/Details panel) clears AttackTellSound back to
		// unset - same placeholder-first shape UMusicSubsystem's CalmTrack/CombatTrack
		// document. No assertion on the warning log itself (no existing test in this
		// module asserts UE_LOG output).
		ABomberEnemy* SilentBomber = AudioWorld->SpawnActor<ABomberEnemy>();
		if (TestNotNull(TEXT("ABomberEnemy should spawn into the silent-audio test World"), SilentBomber))
		{
			SilentBomber->AttackTellSound = nullptr;
			AdvanceToAttack(SilentBomber, ZeroDistanceLocation);
			TestNull(TEXT("Entering Attack with AttackTellSound explicitly cleared should not spawn an audio cue"),
				SilentBomber->AttackTellAudioComponent.Get());
		}

		// (s) Issue #33 AC: the attack-tell audio "is not replayed if the attack is
		// interrupted or the enemy is controlled mid-telegraph". EnemyBase.cpp's state
		// machine is strictly linear (Idle->Alert->Attack->Controlled->Banked, no edges
		// back - see EnemyBase.h's transition table) and AdvanceToAttack() itself guards
		// on CurrentState == Alert, so once ReceiveControl() has moved an actor to
		// Controlled mid-telegraph, no further TickCheckDetection() call can ever drive
		// it back into Attack and re-invoke OnAttackEntry() - structurally, not just "in
		// practice" (mirrors the bHasWarnedMissingAttackTellSound comment in
		// BomberEnemy.h). This proves the audio cue captured on first entry is never
		// replaced/re-spawned, the same guarantee case (l) above already proves for the
		// visual tell and the explosion.
		ABomberEnemy* ReplayGuardBomber = AudioWorld->SpawnActor<ABomberEnemy>();
		if (TestNotNull(TEXT("ABomberEnemy should spawn into the replay-guard test World"), ReplayGuardBomber))
		{
			AdvanceToAttack(ReplayGuardBomber, ZeroDistanceLocation);
			UAudioComponent* FirstAudioComponent = ReplayGuardBomber->AttackTellAudioComponent.Get();
			if (TestNotNull(TEXT("Entering Attack should spawn the attack-tell audio cue"), FirstAudioComponent))
			{
				ReplayGuardBomber->ReceiveControl(EAbilitySlot::Sleep); // interrupts mid-telegraph
				TestEqual(TEXT("interrupted enemy is Controlled"),
					static_cast<uint8>(ReplayGuardBomber->GetEnemyState()), static_cast<uint8>(EEnemyState::Controlled));

				// Further detection checks (e.g. player still in range post-interrupt)
				// must not drive the state machine back into Attack.
				ReplayGuardBomber->TickCheckDetection(ZeroDistanceLocation);
				ReplayGuardBomber->TickCheckDetection(ZeroDistanceLocation);
				TestEqual(TEXT("state stays Controlled - no edge back to Attack exists"),
					static_cast<uint8>(ReplayGuardBomber->GetEnemyState()), static_cast<uint8>(EEnemyState::Controlled));
				TestEqual(TEXT("audio cue is not replaced/re-spawned after the interrupt"),
					ReplayGuardBomber->AttackTellAudioComponent.Get(), FirstAudioComponent);
			}
		}
	}

	// (t) issue #138/#65: expiry-reversion via Fear (Bomber's own OnControlledEntry
	// ability, case (c) above). B0-0MR's GetControlledDurationOverrideSeconds now
	// returns a 7s colour-match bonus for Fear (issue #65), not the 5s
	// AbilityData::Get(Fear).BaseDurationSeconds baseline - read the actually-applied
	// duration off the enemy itself rather than assuming the base duration governs.
	ABomberEnemy* ExpiryBomber = NewObject<ABomberEnemy>();
	AdvanceToAttack(ExpiryBomber, ZeroDistanceLocation);
	ExpiryBomber->ReceiveControl(EAbilitySlot::Fear); // Attack -> Controlled, 7.0f override
	const float FearDurationSeconds = ExpiryBomber->GetTotalControlledSeconds();
	TestEqual(TEXT("Fear's colour-match override (7.0f) should govern expiry, not the 5s base duration"),
		FearDurationSeconds, 7.0f);
	ExpiryBomber->TickControlledDuration(FearDurationSeconds - 1.0f);
	TestEqual(TEXT("Bomber should still be Controlled before the Fear duration elapses"),
		static_cast<uint8>(ExpiryBomber->GetEnemyState()), static_cast<uint8>(EEnemyState::Controlled));
	ExpiryBomber->TickControlledDuration(1.5f);
	TestEqual(TEXT("Bomber should revert to Alert once the Fear duration elapses"),
		static_cast<uint8>(ExpiryBomber->GetEnemyState()), static_cast<uint8>(EEnemyState::Alert));

	// (u) issue #221: the attack telegraph's escalation stage starts Early and
	// advances monotonically Early -> Mid -> Imminent as the fuse burns down,
	// verifiable without rendering - the pulsing light intensity itself is
	// presentation-only, playtest-verified per the issue's AC (see BomberEnemy.h's
	// GetAttackTelegraphStage() comment).
	ABomberEnemy* EscalationBomber = NewObject<ABomberEnemy>();
	AdvanceToAttack(EscalationBomber, ZeroDistanceLocation);
	TestEqual(TEXT("telegraph starts at Early stage"),
		static_cast<uint8>(EscalationBomber->GetAttackTelegraphStage()), static_cast<uint8>(EBomberTelegraphStage::Early));

	EscalationBomber->AdvanceAttackTelegraph(EscalationBomber->AttackTelegraphSeconds * 0.1f); // 10% elapsed
	TestEqual(TEXT("telegraph stays Early well before the Mid threshold"),
		static_cast<uint8>(EscalationBomber->GetAttackTelegraphStage()), static_cast<uint8>(EBomberTelegraphStage::Early));

	EscalationBomber->AdvanceAttackTelegraph(EscalationBomber->AttackTelegraphSeconds * 0.4f); // 50% elapsed total
	TestEqual(TEXT("telegraph reaches Mid stage past the Mid threshold"),
		static_cast<uint8>(EscalationBomber->GetAttackTelegraphStage()), static_cast<uint8>(EBomberTelegraphStage::Mid));

	EscalationBomber->AdvanceAttackTelegraph(EscalationBomber->AttackTelegraphSeconds * 0.3f); // 80% elapsed total
	TestEqual(TEXT("telegraph reaches Imminent stage past the Imminent threshold"),
		static_cast<uint8>(EscalationBomber->GetAttackTelegraphStage()), static_cast<uint8>(EBomberTelegraphStage::Imminent));

	// (v) the stage never regresses once advanced, even as remaining time keeps
	// decreasing toward (but not yet reaching) explosion - the monotonic,
	// non-decreasing guarantee the AC requires.
	EscalationBomber->AdvanceAttackTelegraph(EscalationBomber->AttackTelegraphSeconds * 0.19f); // 99% elapsed, not yet exploded
	TestEqual(TEXT("telegraph stays Imminent, does not regress"),
		static_cast<uint8>(EscalationBomber->GetAttackTelegraphStage()), static_cast<uint8>(EBomberTelegraphStage::Imminent));

	// (w) a fresh OnAttackEntry() resets the stage back to Early, mirroring
	// bExplodedForCurrentAttack's own reset in the same function - proven directly via
	// friend access to the protected OnAttackEntry() hook, same idiom this file already
	// uses for other protected/private members.
	ABomberEnemy* ResetBomber = NewObject<ABomberEnemy>();
	AdvanceToAttack(ResetBomber, ZeroDistanceLocation);
	ResetBomber->AdvanceAttackTelegraph(ResetBomber->AttackTelegraphSeconds); // exploded; stage is Imminent
	TestEqual(TEXT("exploded bomber's stage is Imminent"),
		static_cast<uint8>(ResetBomber->GetAttackTelegraphStage()), static_cast<uint8>(EBomberTelegraphStage::Imminent));
	ResetBomber->OnAttackEntry(); // simulate a fresh attack entry directly
	TestEqual(TEXT("a fresh OnAttackEntry resets the stage back to Early"),
		static_cast<uint8>(ResetBomber->GetAttackTelegraphStage()), static_cast<uint8>(EBomberTelegraphStage::Early));

	// (x) the real Tick() path (not just direct AdvanceAttackTelegraph calls) also
	// drives the stage forward - mirrors case (m)'s same proof for the explosion.
	UWorld* EscalationWorld = FAutomationEditorCommonUtils::CreateNewMap();
	if (TestNotNull(TEXT("CreateNewMap should return a valid World for the escalation test"), EscalationWorld))
	{
		ABomberEnemy* TickedEscalationBomber = EscalationWorld->SpawnActor<ABomberEnemy>();
		if (TestNotNull(TEXT("ABomberEnemy should spawn into the escalation test World"), TickedEscalationBomber))
		{
			AdvanceToAttack(TickedEscalationBomber, ZeroDistanceLocation);
			TickedEscalationBomber->Tick(TickedEscalationBomber->AttackTelegraphSeconds * 0.5f);
			TestEqual(TEXT("Tick() drives the stage to Mid at the halfway point"),
				static_cast<uint8>(TickedEscalationBomber->GetAttackTelegraphStage()), static_cast<uint8>(EBomberTelegraphStage::Mid));
		}
	}

	// (y) issue #221: AttackTelegraphSeconds == 0 (a designer-legal edge case per
	// ClampMin=0.0) is treated as "already Imminent" rather than dividing by zero -
	// see UpdateTelegraphEscalation()'s ElapsedFraction guard.
	ABomberEnemy* ZeroDurationBomber = NewObject<ABomberEnemy>();
	ZeroDurationBomber->AttackTelegraphSeconds = 0.0f;
	AdvanceToAttack(ZeroDurationBomber, ZeroDistanceLocation);
	ZeroDurationBomber->AdvanceAttackTelegraph(0.0f);
	TestEqual(TEXT("zero-duration telegraph is immediately Imminent, no divide-by-zero"),
		static_cast<uint8>(ZeroDurationBomber->GetAttackTelegraphStage()), static_cast<uint8>(EBomberTelegraphStage::Imminent));

	// (z) issue #221: exact >= boundary behavior at TelegraphMidThreshold (0.33 default) -
	// single AdvanceAttackTelegraph calls, not accumulated, to avoid the float-accumulation
	// imprecision case (g2) documents.
	ABomberEnemy* BoundaryBomber = NewObject<ABomberEnemy>();
	AdvanceToAttack(BoundaryBomber, ZeroDistanceLocation);
	BoundaryBomber->AdvanceAttackTelegraph(BoundaryBomber->AttackTelegraphSeconds * (BoundaryBomber->TelegraphMidThreshold - 0.01f));
	TestEqual(TEXT("just below TelegraphMidThreshold stays Early"),
		static_cast<uint8>(BoundaryBomber->GetAttackTelegraphStage()), static_cast<uint8>(EBomberTelegraphStage::Early));
	BoundaryBomber->AdvanceAttackTelegraph(BoundaryBomber->AttackTelegraphSeconds * 0.02f); // crosses the threshold
	TestEqual(TEXT("just past TelegraphMidThreshold advances to Mid"),
		static_cast<uint8>(BoundaryBomber->GetAttackTelegraphStage()), static_cast<uint8>(EBomberTelegraphStage::Mid));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
