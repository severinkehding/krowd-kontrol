#include "RunnerEnemy.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "ReservedGameplayColours.h"
#include "EnemyTypeIndicatorComponent.h"
#include "EnemyType.h"
#include "Sound/SoundBase.h"
#include "Kismet/GameplayStatics.h"
#include "Components/AudioComponent.h"
#include "AbilityData.h"

ARunnerEnemy::ARunnerEnemy()
{
	PrimaryActorTick.bCanEverTick = true;

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	RootComponent = MeshComponent;

	EliteTrimLightComponent = CreateDefaultSubobject<UPointLightComponent>(TEXT("RunnerEliteTrimLightComponent"));
	EliteTrimLightComponent->SetupAttachment(MeshComponent);
	// PLACEHOLDER COLOUR - deliberately NOT one of MISSION.md Hard Invariant 3's five
	// reserved gameplay-information colours (Purple/Teal/Orange/Blue/White), and
	// distinct from every existing per-type AttackTellLightComponent colour
	// ((1.0,0.85,0.1) Sniper, (1.0,0.15,0.05) Bomber, (1.0,0.1,0.6) Trooper,
	// (0.6,1.0,0.2) Runner) - a bright saturated green, the one hue family none of the
	// existing warm-hue tell colours or the 5 reserved colours occupy. Created inline
	// here (not via a shared AEnemyBase helper) - matches every other type-tied
	// component's shape (AttackTellLightComponent, etc). See app-changelog/issue-19.md's
	// "Design note" for why a shared-helper version was tried and reverted (a
	// GC/test-lifetime issue, not a declaration-shape problem).
	EliteTrimLightComponent->SetLightColor(FLinearColor(0.1f, 1.0f, 0.15f));
	EliteTrimLightComponent->SetIntensity(0.0f); // off unless bIsElite
	EliteTrimLightComponent->SetAttenuationRadius(300.0f);

	// Deliberate mesh reuse, not a new primitive: all 5 /Engine/BasicShapes/ meshes are
	// now claimed (Cube - APlaceholderCubeActor; Cylinder - APlaceholderTargetZoneActor/
	// APlaceholderTerminalActor; Cone - ASniperEnemy; Sphere - ABomberEnemy; Plane -
	// ATrooperEnemy). The AC only requires distinctness from the other 3 core enemy
	// types, not from non-enemy placeholder props, and this codebase already has
	// precedent for cross-role mesh reuse (PlaceholderTerminalActor.cpp: "Reuse the
	// cylinder already used by APlaceholderTargetZoneActor rather than introducing a
	// third distinct placeholder mesh reference"). Reusing Cube here, at a scale no
	// other actor uses, is the same move.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMeshFinder(
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMeshFinder.Succeeded())
	{
		MeshComponent->SetStaticMesh(CubeMeshFinder.Object);
	}
	// Elongated, low, forward-rushing "dart/blade" silhouette, visibly distinct by
	// shape alone (even desaturated) from Sniper's tall Cone, Bomber's round Sphere,
	// Trooper's flat standing Plane, and from APlaceholderCubeActor's own unscaled
	// (1,1,1) cube.
	MeshComponent->SetRelativeScale3D(FVector(1.8f, 0.6f, 0.6f));

	DrainGlowLightComponent = CreateDefaultSubobject<UPointLightComponent>(TEXT("DrainGlowLightComponent"));
	DrainGlowLightComponent->SetupAttachment(MeshComponent);
	// Purple is RU-NNR's locked information colour (MISSION.md Hard Invariant 3) -
	// used here ONLY as this signal, never as decoration.
	DrainGlowLightComponent->SetLightColor(ReservedGameplayColours::GetPurple());
	DrainGlowLightComponent->SetIntensity(DrainGlowBaselineIntensity);
	DrainGlowLightComponent->SetAttenuationRadius(300.0f);

	AttackTellLightComponent = CreateDefaultSubobject<UPointLightComponent>(TEXT("AttackTellLightComponent"));
	AttackTellLightComponent->SetupAttachment(MeshComponent);
	// PLACEHOLDER COLOUR - deliberately NOT one of MISSION.md Hard Invariant 3's five
	// reserved gameplay-information colours (Purple/Teal/Orange/Blue/White), same
	// caveat every sibling's own tell colour documents, and distinct from all 3 of
	// theirs ((1.0,0.85,0.1), (1.0,0.15,0.05), (1.0,0.1,0.6)).
	AttackTellLightComponent->SetLightColor(FLinearColor(0.6f, 1.0f, 0.2f));
	AttackTellLightComponent->SetIntensity(0.0f); // off until Attack entry
	AttackTellLightComponent->SetAttenuationRadius(300.0f);

	EnemyTypeIndicatorComponent = CreateDefaultSubobject<UEnemyTypeIndicatorComponent>(TEXT("EnemyTypeIndicatorComponent"));
	EnemyTypeIndicatorComponent->EnemyType = EEnemyType::RU_NNR;

	// Placeholder-first default (MISSION.md) so issue #28's "a distinct sound
	// effect plays" AC holds without waiting on a designer to configure
	// AttackTellSound. /Engine/EngineSounds/ has only two playable USoundBase
	// assets (1kSineTonePing, WhiteNoise) and both are already claimed by
	// ASniperEnemy/ABomberEnemy respectively, and
	// /Engine/EditorSounds/Notifications/CompileSuccess is already claimed by
	// ATrooperEnemy (verified against the installed Engine content) - reusing any
	// of those here would make two enemy types share an "audibly distinct" tell,
	// contradicting issue #28's AC directly. This uses a fourth built-in Engine
	// asset instead, in the same notification-chime family: a short "compile
	// failed" chime, always present in a stock Engine install and loadable in the
	// Editor context this project's Automation tests actually run in (no
	// packaging/cook step exists yet - see harness/README.md). Still Details-panel/
	// Blueprint overridable.
	static ConstructorHelpers::FObjectFinder<USoundBase> AttackTellSoundFinder(
		TEXT("/Engine/EditorSounds/Notifications/CompileFailed.CompileFailed"));
	if (AttackTellSoundFinder.Succeeded())
	{
		AttackTellSound = AttackTellSoundFinder.Object;
	}
}

float ARunnerEnemy::GetControlledSpeedMultiplier() const
{
	// Issue #65: RU-NNR is specifically countered by Snare, and Snare's colour-match
	// bonus is POTENCY, not duration - the slow deepens from the 50% base
	// (AbilityData's 0.5f ControlledSpeedMultiplier, issue #254) to 75% (0.25f) on
	// match, per docs/prd-ability-shapes.md's locked ability table ("Slow: 50% base,
	// 75% on colour match (see #65)") and the operator's 2026-08-22 respec on issue
	// #65 (per-matchup enhanced effect on top of each ability's base - only the
	// three incapacitating matchups express theirs as extra duration via
	// GetControlledDurationOverrideSeconds()).
	if (IsControlled() && GetControllingAbility() == EAbilitySlot::Snare)
	{
		return 0.25f;
	}
	return Super::GetControlledSpeedMultiplier();
}

float ARunnerEnemy::GetAttackRangeUnits() const
{
	// Short range - a little beyond ABomberEnemy's 150.0f melee-contact range, since a
	// drain-ray is a ranged attack rather than contact damage; still far short of
	// Trooper's medium 700.0f/Sniper's long 1400.0f.
	return 220.0f;
}

float ARunnerEnemy::GetMovementSpeedUnitsPerSecond() const
{
	// Per-type override (issue #122) - RU-NNR's fast-movement AC now actually drives
	// AEnemyBase::TickChaseMovement, not just a declared value.
	return MovementSpeed;
}

void ARunnerEnemy::OnControlledEntry(EAbilitySlot Ability)
{
	// ReceiveControl only calls this from Alert/Attack, so any in-progress attack
	// telegraph is normally aborted the moment Controlled is entered - clear the tell
	// regardless of which ability triggered this, same rationale every sibling's own
	// OnControlledEntry documents. Root is the one exception (bAllowsAttackWhileControlled)
	// - see issue #255 - so the tell is deliberately left alone for it.
	if (!AbilityData::Get(Ability).bAllowsAttackWhileControlled)
	{
		AttackTellLightComponent->SetIntensity(0.0f);
	}

	if (Ability != EAbilitySlot::Snare)
	{
		return;
	}
	// PRD 03 REQ-3: glow visibly intensifies ONLY on Snare specifically - every other
	// ability produces no glow response at all.
	DrainGlowLightComponent->SetIntensity(DrainGlowIntensifiedIntensity);
}

void ARunnerEnemy::OnControlledExpired()
{
	// Root (bAllowsAttackWhileControlled) is the only ability that leaves the tell lit
	// through OnControlledEntry above, so this is the only case that can ever find it
	// still on here - safe to unconditionally clear on every Controlled -> Alert edge
	// (pass-1 review follow-up, issue #255).
	AttackTellLightComponent->SetIntensity(0.0f);
}

void ARunnerEnemy::OnAttackExpired()
{
	// Issue #313's Attack-duration timeout reverts Attack -> Alert unconditionally,
	// even mid-telegraph - without this the tell stays lit forever once that happens,
	// the same bug OnControlledExpired above exists to prevent for the Controlled ->
	// Alert edge (pass-1 review follow-up, issue #313).
	AttackTellLightComponent->SetIntensity(0.0f);
}

void ARunnerEnemy::OnAttackEntry()
{
	AttackTellLightComponent->SetIntensity(AttackTellIntensity);
	RemainingTelegraphSeconds = AttackTelegraphSeconds;
	bDrainFiredForCurrentAttack = false;

	// AttackTellSound defaults to a placeholder engine chime (see the
	// constructor), so this resolves normally out of the box; the else-branch below
	// is a defensive fallback for the case a Blueprint/Details-panel override
	// explicitly clears it.
	if (USoundBase* TellSound = AttackTellSound.LoadSynchronous())
	{
		AttackTellAudioComponent = UGameplayStatics::SpawnSoundAtLocation(this, TellSound, GetActorLocation());
	}
	else if (!bHasWarnedMissingAttackTellSound)
	{
		bHasWarnedMissingAttackTellSound = true;
		UE_LOG(LogTemp, Warning,
			TEXT("ARunnerEnemy: no AttackTellSound configured on '%s' - attack telegraph will be silent."),
			*GetNameSafe(this));
	}
}

void ARunnerEnemy::AdvanceAttackTelegraph(float DeltaSeconds)
{
	if (bDrainFiredForCurrentAttack || !IsAttackBehaviorActive())
	{
		return;
	}
	RemainingTelegraphSeconds = FMath::Max(0.0f, RemainingTelegraphSeconds - DeltaSeconds * GetControlledSpeedMultiplier());
	if (RemainingTelegraphSeconds <= 0.0f)
	{
		// Guards against re-firing OnRunnerDrainFired every subsequent tick once the
		// telegraph reaches zero - the same "fire exactly once" shape ASniperEnemy/
		// ABomberEnemy's own AdvanceAttackTelegraph already establish.
		bDrainFiredForCurrentAttack = true;
		OnRunnerDrainFired.Broadcast();
	}
}

void ARunnerEnemy::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	AdvanceAttackTelegraph(DeltaTime);
}
