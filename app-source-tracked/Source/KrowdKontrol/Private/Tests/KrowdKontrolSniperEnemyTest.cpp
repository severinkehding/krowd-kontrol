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
#include "AbilityCastComponent.h"
#include "AbilityUnlockComponent.h"
#include "AbilityCooldownComponent.h"
#include "GameFramework/Pawn.h"
#include "EnemyAttackExpiredTestListener.h"
#include "AbilityTargetingIndicatorComponent.h"
#include "KrowdKontrolPlayerController.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	// Copied verbatim from KrowdKontrolLevelBriefingSubsystemTest.cpp's own helper -
	// CreateNewMap() worlds skip PostInitializeComponents, so
	// World->GetFirstPlayerController() (and thus UGameplayStatics::GetPlayerPawn())
	// reads empty without this explicit registration step.
	AKrowdKontrolPlayerController* SpawnPossessedController(UWorld* World, APawn* Pawn)
	{
		AKrowdKontrolPlayerController* Controller = World->SpawnActor<AKrowdKontrolPlayerController>();
		if (!Controller)
		{
			return nullptr;
		}
		Controller->Player = NewObject<ULocalPlayer>(GEngine);
		Controller->SetAsLocalPlayerController();
		Controller->Possess(Pawn);
		World->AddController(Controller);
		return Controller;
	}
}

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

	// (d2) issue #361: no Stun-first activation gate exists - Stun (like every other
	// ability) applies its effect directly to a sniper already in Attack state, with no
	// prior control cast of any kind. Audited: AEnemyBase::ReceiveControl gates only on
	// CurrentState (Alert/Attack), never on ControllingAbility or a prior-Stun flag, and
	// ASniperEnemy adds no override that changes this.
	ASniperEnemy* StunnedSniper = NewObject<ASniperEnemy>();
	AdvanceToAttack(StunnedSniper, ZeroDistanceLocation);
	StunnedSniper->ReceiveControl(EAbilitySlot::Stun);
	TestEqual(TEXT("(d2) Sniper should be Controlled after Stun, direct from Attack, no prior cast"),
		static_cast<uint8>(StunnedSniper->GetEnemyState()), static_cast<uint8>(EEnemyState::Controlled));

	// (d3) issue #361: same rule for Fear.
	ASniperEnemy* FearedSniper = NewObject<ASniperEnemy>();
	AdvanceToAttack(FearedSniper, ZeroDistanceLocation);
	FearedSniper->ReceiveControl(EAbilitySlot::Fear);
	TestEqual(TEXT("(d3) Sniper should be Controlled after Fear, direct from Attack, no prior cast"),
		static_cast<uint8>(FearedSniper->GetEnemyState()), static_cast<uint8>(EEnemyState::Controlled));

	// (d4) issue #361 pass-2 follow-up: (d2)/(d3) above prove ReceiveControl() itself
	// has no gate, but not that the cast/targeting layer above it doesn't add one.
	// Goes through the real player-facing entry point, UAbilityCastComponent::
	// TryCastAbility, against an Attack-state sniper with no prior cast - same
	// UWorld-spawning pattern KrowdKontrolAbilityCastComponentTest.cpp's cases use.
	// Stun is used because it's the one ability unlocked by default
	// (UAbilityUnlockComponent's construction), so no extra unlock setup is needed.
	{
		UWorld* CastWorld = FAutomationEditorCommonUtils::CreateNewMap();
		if (TestNotNull(TEXT("(d4) CreateNewMap should return a valid World"), CastWorld))
		{
			APawn* CastOwner = CastWorld->SpawnActor<APawn>();
			UAbilityUnlockComponent* CastUnlockComponent = NewObject<UAbilityUnlockComponent>(CastOwner);
			CastUnlockComponent->RegisterComponent();
			UAbilityCooldownComponent* CastCooldownComponent = NewObject<UAbilityCooldownComponent>(CastOwner);
			CastCooldownComponent->RegisterComponent();
			UAbilityCastComponent* RealCastComponent = NewObject<UAbilityCastComponent>(CastOwner);
			RealCastComponent->RegisterComponent();

			ASniperEnemy* CastTargetSniper = CastWorld->SpawnActor<ASniperEnemy>();
			if (TestNotNull(TEXT("(d4) ASniperEnemy should spawn into the test World"), CastTargetSniper))
			{
				AdvanceToAttack(CastTargetSniper, ZeroDistanceLocation);
				TestEqual(TEXT("(d4) precondition: sniper should be in Attack before the real cast"),
					static_cast<uint8>(CastTargetSniper->GetEnemyState()), static_cast<uint8>(EEnemyState::Attack));

				const bool bCastResult = RealCastComponent->TryCastAbility(EAbilitySlot::Stun);
				TestTrue(TEXT("(d4) TryCastAbility(Stun) should succeed against an Attack-state sniper, no prior cast"),
					bCastResult);
				TestEqual(TEXT("(d4) Sniper should be Controlled after a real TryCastAbility(Stun) - no gate in the cast layer"),
					static_cast<uint8>(CastTargetSniper->GetEnemyState()), static_cast<uint8>(EEnemyState::Controlled));
			}
		}
	}

	// (d5) issue #361 pass-1 review follow-up: (d2)/(d3)/(d4) above cover Stun and
	// Fear; the remaining 3 control abilities (Sleep, Root, Snare) already had
	// equivalent Attack -> Controlled coverage elsewhere in this file (cases (c)/(s),
	// (l2), (m-snare) respectively), but that coverage predates this PR and is
	// therefore not diff-visible - added here, mirroring (d2)/(d3) exactly, so all 5
	// control abilities have a diff-visible "no prior cast" regression case in this PR.
	ASniperEnemy* SleepGateSniper = NewObject<ASniperEnemy>();
	AdvanceToAttack(SleepGateSniper, ZeroDistanceLocation);
	SleepGateSniper->ReceiveControl(EAbilitySlot::Sleep);
	TestEqual(TEXT("(d5) Sniper should be Controlled after Sleep, direct from Attack, no prior cast"),
		static_cast<uint8>(SleepGateSniper->GetEnemyState()), static_cast<uint8>(EEnemyState::Controlled));

	// (d6) issue #361: same rule for Root.
	ASniperEnemy* RootGateSniper = NewObject<ASniperEnemy>();
	AdvanceToAttack(RootGateSniper, ZeroDistanceLocation);
	RootGateSniper->ReceiveControl(EAbilitySlot::Root);
	TestEqual(TEXT("(d6) Sniper should be Controlled after Root, direct from Attack, no prior cast"),
		static_cast<uint8>(RootGateSniper->GetEnemyState()), static_cast<uint8>(EEnemyState::Controlled));

	// (d7) issue #361: same rule for Snare.
	ASniperEnemy* SnareGateSniper = NewObject<ASniperEnemy>();
	AdvanceToAttack(SnareGateSniper, ZeroDistanceLocation);
	SnareGateSniper->ReceiveControl(EAbilitySlot::Snare);
	TestEqual(TEXT("(d7) Sniper should be Controlled after Snare, direct from Attack, no prior cast"),
		static_cast<uint8>(SnareGateSniper->GetEnemyState()), static_cast<uint8>(EEnemyState::Controlled));

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
		// Issue #316 (test-coverage review): SpawnActor<>() alone does NOT run
		// BeginPlay() on a bare CreateNewMap() world - both calls below are required
		// (see KrowdKontrolTargetZoneTest.cpp's file comment for why neither alone
		// suffices), made once up front so every actor spawned into World afterward
		// auto-begins-play via the engine's normal flow.
		World->InitializeActorsForPlay(FURL());
		World->SetBegunPlay(true);

		ASniperEnemy* TickedSniper = World->SpawnActor<ASniperEnemy>();
		if (TestNotNull(TEXT("ASniperEnemy should spawn into the test World"), TickedSniper))
		{
			// The direct-invocation case above only proves ApplyBodyChainColourTint()
			// works when called directly, not that BeginPlay() actually calls it - this
			// proves the wiring.
			TestNotNull(TEXT("BeginPlay() should have created BodyChainColourMaterialInstance"),
				TickedSniper->BodyChainColourMaterialInstance.Get());
			TestTrue(TEXT("BeginPlay() should have applied SN_1PR's chain colour"),
				TickedSniper->CurrentBodyChainColour.Equals(
					AbilityData::GetChainColourForEnemyType(EEnemyType::SN_1PR), 0.01f));

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

	// (v) issue #360: the player leaving attack range mid-telegraph cancels the shot
	// - OnSniperShotFired never fires even once the (now-frozen) telegraph's remaining
	// time is advanced well past its original duration - and reverts the sniper to
	// Alert via the same shared RevertAttackToAlert()/OnAttackExpired() path the #313
	// timeout uses (tell light clears; OnEnemyAttackExpired fires exactly once).
	//
	// Uses the OnSniperShotFired listener-count pattern (v) below already relies on for
	// "shot cancelled", rather than a UPlayerEnergyComponent energy check: damage
	// application is out of this issue's scope (see issue #358, tracked separately),
	// so asserting on the delegate keeps this test meaningful regardless of which PR
	// ends up owning the damage-application code path.
	UWorld* RangeBreakWorld = FAutomationEditorCommonUtils::CreateNewMap();
	if (TestNotNull(TEXT("(v) CreateNewMap should return a valid World for the range-break test"), RangeBreakWorld))
	{
		ASniperEnemy* RangeBreakSniper = RangeBreakWorld->SpawnActor<ASniperEnemy>();
		APawn* RangeBreakPlayerPawn = RangeBreakWorld->SpawnActor<APawn>();
		if (TestNotNull(TEXT("(v) ASniperEnemy should spawn into the range-break test World"), RangeBreakSniper)
			&& TestNotNull(TEXT("(v) Player pawn should spawn into the range-break test World"), RangeBreakPlayerPawn))
		{
			// Issue #359: possessed so UGameplayStatics::GetPlayerPawn() actually resolves
			// this pawn - without this, UpdateTelegraphIndicator()'s guard would no-op
			// every Show() call and the bIsVisible assertions below would pass trivially
			// (already false) rather than exercising the real teardown path.
			SpawnPossessedController(RangeBreakWorld, RangeBreakPlayerPawn);

			USniperShotFiredTestListener* RangeBreakShotListener = NewObject<USniperShotFiredTestListener>();
			RangeBreakSniper->OnSniperShotFired.AddDynamic(RangeBreakShotListener, &USniperShotFiredTestListener::HandleSniperShotFired);

			UEnemyAttackExpiredTestListener* RangeBreakExpiredListener = NewObject<UEnemyAttackExpiredTestListener>();
			RangeBreakSniper->OnEnemyAttackExpired.AddDynamic(RangeBreakExpiredListener, &UEnemyAttackExpiredTestListener::HandleEnemyAttackExpired);

			AdvanceToAttack(RangeBreakSniper, ZeroDistanceLocation);
			TestTrue(TEXT("(v) Attack tell should be visibly on before the range-break"),
				RangeBreakSniper->AttackTellLightComponent->Intensity > 0.0f);
			TestTrue(TEXT("(v) issue #359: telegraph should be visible before the range-break"),
				RangeBreakSniper->TelegraphIndicatorComponent->bIsVisible);

			RangeBreakSniper->AdvanceAttackTelegraph(RangeBreakSniper->AttackTelegraphSeconds * 0.5f); // mid-telegraph

			const FVector BeyondAttackRangeLocation(1500.0f, 0.0f, 0.0f); // > 1400.0f GetAttackRangeUnits()
			RangeBreakSniper->TickCheckDetection(BeyondAttackRangeLocation);
			TestEqual(TEXT("(v) Sniper should revert to Alert once the player leaves attack range mid-telegraph"),
				static_cast<uint8>(RangeBreakSniper->GetEnemyState()), static_cast<uint8>(EEnemyState::Alert));
			TestEqual(TEXT("(v) Attack tell should be cleared by the shared OnAttackExpired hook"),
				RangeBreakSniper->AttackTellLightComponent->Intensity, 0.0f);
			TestFalse(TEXT("(v) issue #359: telegraph should be hidden by the shared OnAttackExpired hook"),
				RangeBreakSniper->TelegraphIndicatorComponent->bIsVisible);
			TestEqual(TEXT("(v) OnEnemyAttackExpired should fire exactly once on the range-break"),
				RangeBreakExpiredListener->CallCount, 1);

			RangeBreakSniper->AdvanceAttackTelegraph(RangeBreakSniper->AttackTelegraphSeconds); // well past original duration
			TestEqual(TEXT("(v) The shot must never fire - the range-broken telegraph must not resolve"),
				RangeBreakShotListener->CallCount, 0);
		}
	}

	// (w) issue #360: re-entering attack range after a range-break restarts the
	// telegraph from zero - no partial credit from the aborted attempt survives.
	ASniperEnemy* ReacquireSniper = NewObject<ASniperEnemy>();
	AdvanceToAttack(ReacquireSniper, ZeroDistanceLocation);
	ReacquireSniper->AdvanceAttackTelegraph(ReacquireSniper->AttackTelegraphSeconds - 0.1f); // 0.1s from firing

	const FVector ReacquireBeyondRangeLocation(1500.0f, 0.0f, 0.0f);
	ReacquireSniper->TickCheckDetection(ReacquireBeyondRangeLocation); // Attack -> Alert (range-break)
	TestEqual(TEXT("(w) Sniper should be back to Alert after the range-break"),
		static_cast<uint8>(ReacquireSniper->GetEnemyState()), static_cast<uint8>(EEnemyState::Alert));

	ReacquireSniper->TickCheckDetection(ZeroDistanceLocation); // Alert -> Attack, fresh OnAttackEntry()
	TestEqual(TEXT("(w) Sniper should re-enter Attack once back in range"),
		static_cast<uint8>(ReacquireSniper->GetEnemyState()), static_cast<uint8>(EEnemyState::Attack));

	USniperShotFiredTestListener* ReacquireListener = NewObject<USniperShotFiredTestListener>();
	ReacquireSniper->OnSniperShotFired.AddDynamic(ReacquireListener, &USniperShotFiredTestListener::HandleSniperShotFired);
	// If the old progress had carried over, this small advance (well under a fresh
	// AttackTelegraphSeconds) would be enough to fire, since the aborted attempt was
	// only 0.1s from completion - it must NOT fire, proving the telegraph restarted
	// from zero rather than resuming.
	ReacquireSniper->AdvanceAttackTelegraph(0.2f);
	TestEqual(TEXT("(w) The shot should not fire yet - the telegraph must restart from zero on re-acquire, not resume"),
		ReacquireListener->CallCount, 0);

	ReacquireSniper->AdvanceAttackTelegraph(ReacquireSniper->AttackTelegraphSeconds); // finish a full fresh telegraph
	TestEqual(TEXT("(w) The shot should fire once a full fresh telegraph elapses after re-acquire"),
		ReacquireListener->CallCount, 1);

	// (x) issue #360: SN-1PR now has its own named, tunable chase-speed constant
	// (MovementSpeed) driving GetMovementSpeedUnitsPerSecond() - below AEnemyBase's
	// own base-class default (600.0f), and below the player pawn's own
	// UFloatingPawnMovement MaxSpeed (this project's unmodified engine default,
	// 1200.0f - no C++ override exists anywhere in this module), so outrunning a
	// chasing sniper is achievable at the project's current move speeds.
	ASniperEnemy* ChaseSpeedSniper = NewObject<ASniperEnemy>();
	TestTrue(TEXT("(x) Sniper's chase speed should be a positive, tunable value"),
		ChaseSpeedSniper->MovementSpeed > 0.0f);
	TestTrue(TEXT("(x) Sniper's chase speed should be below AEnemyBase's own base-class default (600.0f)"),
		ChaseSpeedSniper->MovementSpeed < 600.0f);
	TestEqual(TEXT("(x) GetMovementSpeedUnitsPerSecond() should return the named MovementSpeed constant"),
		ChaseSpeedSniper->GetMovementSpeedUnitsPerSecond(), ChaseSpeedSniper->MovementSpeed);

	// (y) issue #360: driving TickChaseMovement directly (friend-accessible, same as
	// TickCheckDetection) during Alert - e.g. right after a range-break - advances
	// the sniper at exactly its own MovementSpeed, not the inherited 600.0f base,
	// mirroring KrowdKontrolBomberEnemyTest.cpp's (l3) case exactly.
	ASniperEnemy* ChasingSniper = NewObject<ASniperEnemy>();
	const FVector FarSniperPlayerLocation(1000.0f, 0.0f, 0.0f);
	const FVector AlertOnlyLocation(1450.0f, 0.0f, 0.0f); // > 1400.0f attack range, <= 1500.0f detection range
	ChasingSniper->TickCheckDetection(AlertOnlyLocation); // Idle -> Alert (not Attack)
	TestEqual(TEXT("(y) precondition: sniper is Alert"),
		static_cast<uint8>(ChasingSniper->GetEnemyState()), static_cast<uint8>(EEnemyState::Alert));
	const FVector BeforeChase = ChasingSniper->GetActorLocation();
	ChasingSniper->TickChaseMovement(FarSniperPlayerLocation, 0.5f);
	const float DistanceMoved = FVector::Dist(ChasingSniper->GetActorLocation(), BeforeChase);
	TestEqual(TEXT("(y) sniper chase advances at its own MovementSpeed * DeltaSeconds"),
		DistanceMoved, ChasingSniper->MovementSpeed * 0.5f);

	// (z) issue #359: the world-space "shot incoming" telegraph line - spawn-on-
	// acquisition (Line shape, non-reserved colour matching the attack tell light),
	// teardown-on-shot-fire, and teardown-on-Controlled(Root) (the one ability that
	// leaves AttackTellLightComponent lit, proving the telegraph's unconditional
	// Hide() genuinely diverges from the light tell's own conditional clear).
	// Teardown-on-range-break is covered by case (v) above, extended with the same
	// bIsVisible assertions.
	UWorld* TelegraphWorld = FAutomationEditorCommonUtils::CreateNewMap();
	if (TestNotNull(TEXT("(z) CreateNewMap should return a valid World for the telegraph test"), TelegraphWorld))
	{
		ASniperEnemy* TelegraphSniper = TelegraphWorld->SpawnActor<ASniperEnemy>();
		APawn* TelegraphPlayerPawn = TelegraphWorld->SpawnActor<APawn>();
		if (TestNotNull(TEXT("(z) ASniperEnemy should spawn into the telegraph test World"), TelegraphSniper)
			&& TestNotNull(TEXT("(z) Player pawn should spawn into the telegraph test World"), TelegraphPlayerPawn))
		{
			SpawnPossessedController(TelegraphWorld, TelegraphPlayerPawn);

			AdvanceToAttack(TelegraphSniper, ZeroDistanceLocation);

			UAbilityTargetingIndicatorComponent* Telegraph = TelegraphSniper->TelegraphIndicatorComponent;
			if (TestNotNull(TEXT("(z) ASniperEnemy should have a TelegraphIndicatorComponent"), Telegraph))
			{
				TestTrue(TEXT("(z) Telegraph should be visible once the sniper enters Attack against a resolvable player pawn"),
					Telegraph->bIsVisible);
				TestEqual(TEXT("(z) Telegraph should use the Line shape kind"),
					static_cast<uint8>(Telegraph->CurrentShapeSpec.Kind), static_cast<uint8>(EAbilityIndicatorShapeKind::Line));
				TestTrue(TEXT("(z) Telegraph colour should match the attack tell light's own (already-vetted) colour"),
					Telegraph->CurrentColour.Equals(TelegraphSniper->AttackTellLightComponent->GetLightColor(), 0.01f));
				TestFalse(TEXT("(z) Telegraph colour should not collide with a reserved gameplay-information colour"),
					ReservedGameplayColours::GetAll().ContainsByPredicate(
						[Telegraph](const FLinearColor& Reserved) { return Reserved.Equals(Telegraph->CurrentColour, 0.01f); }));

				// Teardown on shot-fire.
				USniperShotFiredTestListener* TelegraphShotListener = NewObject<USniperShotFiredTestListener>();
				TelegraphSniper->OnSniperShotFired.AddDynamic(TelegraphShotListener, &USniperShotFiredTestListener::HandleSniperShotFired);
				TelegraphSniper->AdvanceAttackTelegraph(TelegraphSniper->AttackTelegraphSeconds);
				TestEqual(TEXT("(z) precondition: the shot should have fired"), TelegraphShotListener->CallCount, 1);
				TestFalse(TEXT("(z) Telegraph should be hidden the instant the shot fires"),
					Telegraph->bIsVisible);
			}
		}
	}

	// (z2) issue #359: teardown on Controlled by Root specifically - the one ability
	// that leaves AttackTellLightComponent lit (bAllowsAttackWhileControlled), proving
	// the telegraph's Hide() is unconditional, unlike the light tell's own guard.
	UWorld* TelegraphRootWorld = FAutomationEditorCommonUtils::CreateNewMap();
	if (TestNotNull(TEXT("(z2) CreateNewMap should return a valid World for the Root-Controlled telegraph test"), TelegraphRootWorld))
	{
		ASniperEnemy* TelegraphRootSniper = TelegraphRootWorld->SpawnActor<ASniperEnemy>();
		APawn* TelegraphRootPlayerPawn = TelegraphRootWorld->SpawnActor<APawn>();
		if (TestNotNull(TEXT("(z2) ASniperEnemy should spawn into the Root-Controlled telegraph test World"), TelegraphRootSniper)
			&& TestNotNull(TEXT("(z2) Player pawn should spawn into the Root-Controlled telegraph test World"), TelegraphRootPlayerPawn))
		{
			SpawnPossessedController(TelegraphRootWorld, TelegraphRootPlayerPawn);

			AdvanceToAttack(TelegraphRootSniper, ZeroDistanceLocation);
			UAbilityTargetingIndicatorComponent* RootTelegraph = TelegraphRootSniper->TelegraphIndicatorComponent;
			if (TestNotNull(TEXT("(z2) ASniperEnemy should have a TelegraphIndicatorComponent"), RootTelegraph))
			{
				TestTrue(TEXT("(z2) precondition: telegraph should be visible before Root interrupts"),
					RootTelegraph->bIsVisible);

				TelegraphRootSniper->ReceiveControl(EAbilitySlot::Root);
				TestTrue(TEXT("(z2) Attack tell light should stay on - Root does not clear it"),
					TelegraphRootSniper->AttackTellLightComponent->Intensity > 0.0f);
				TestFalse(TEXT("(z2) Telegraph should be hidden on Controlled entry even for Root, unlike the light tell"),
					RootTelegraph->bIsVisible);
			}
		}
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
