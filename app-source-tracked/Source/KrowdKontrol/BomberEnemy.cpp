#include "BomberEnemy.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "ReservedGameplayColours.h"
#include "PlayerEnergyComponent.h"
#include "EnemyTypeIndicatorComponent.h"
#include "EnemyType.h"
#include "Sound/SoundBase.h"
#include "Kismet/GameplayStatics.h"
#include "Components/AudioComponent.h"
#include "AbilityData.h"

ABomberEnemy::ABomberEnemy()
{
	PrimaryActorTick.bCanEverTick = true;

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	RootComponent = MeshComponent;

	EliteTrimLightComponent = CreateDefaultSubobject<UPointLightComponent>(TEXT("BomberEliteTrimLightComponent"));
	EliteTrimLightComponent->SetupAttachment(MeshComponent);
	// PLACEHOLDER COLOUR - see RunnerEnemy.cpp's constructor comment for the full
	// rationale (colour choice, and why this is inline rather than a shared
	// AEnemyBase helper).
	EliteTrimLightComponent->SetLightColor(FLinearColor(0.1f, 1.0f, 0.15f));
	EliteTrimLightComponent->SetIntensity(0.0f); // off unless bIsElite
	EliteTrimLightComponent->SetAttenuationRadius(300.0f);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMeshFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMeshFinder.Succeeded())
	{
		MeshComponent->SetStaticMesh(SphereMeshFinder.Object);
	}
	// Distinct silhouette from Cube/Cylinder/Sniper's Cone, even desaturated (AC).
	MeshComponent->SetRelativeScale3D(FVector(1.3f, 1.3f, 1.3f));

	CoreGlowLightComponent = CreateDefaultSubobject<UPointLightComponent>(TEXT("CoreGlowLightComponent"));
	CoreGlowLightComponent->SetupAttachment(MeshComponent);
	// Orange is B0-0MR's locked information colour (MISSION.md Hard Invariant 3).
	CoreGlowLightComponent->SetLightColor(ReservedGameplayColours::GetOrange());
	CoreGlowLightComponent->SetIntensity(CoreGlowBaselineIntensity);
	CoreGlowLightComponent->SetAttenuationRadius(300.0f);

	AttackTellLightComponent = CreateDefaultSubobject<UPointLightComponent>(TEXT("AttackTellLightComponent"));
	AttackTellLightComponent->SetupAttachment(MeshComponent);
	// PLACEHOLDER - not a reserved colour, distinct from Sniper's tell (1.0,0.85,0.1).
	AttackTellLightComponent->SetLightColor(FLinearColor(1.0f, 0.15f, 0.05f));
	AttackTellLightComponent->SetIntensity(0.0f); // off until Attack entry
	AttackTellLightComponent->SetAttenuationRadius(300.0f);

	EnemyTypeIndicatorComponent = CreateDefaultSubobject<UEnemyTypeIndicatorComponent>(TEXT("EnemyTypeIndicatorComponent"));
	EnemyTypeIndicatorComponent->EnemyType = EEnemyType::B0_0MR;

	// Placeholder-first default (MISSION.md) so issue #33's "a distinct sound effect
	// plays" AC holds without waiting on a designer to configure AttackTellSound - a
	// short built-in noise burst, distinct from ASniperEnemy's 1kSineTonePing and from
	// CalmTrack/CombatTrack (music loops) and any future ability-cast/UI sound. Still
	// Details-panel/Blueprint overridable once a real per-enemy-type tell is sourced.
	static ConstructorHelpers::FObjectFinder<USoundBase> AttackTellSoundFinder(
		TEXT("/Engine/EngineSounds/WhiteNoise.WhiteNoise"));
	if (AttackTellSoundFinder.Succeeded())
	{
		AttackTellSound = AttackTellSoundFinder.Object;
	}
}

float ABomberEnemy::GetAttackRangeUnits() const
{
	// Small relative to DetectionRangeUnits's default (1500.0f) - the mechanical
	// definition of "short-range", the opposite of ASniperEnemy's own override.
	return 150.0f;
}

float ABomberEnemy::GetMovementSpeedUnitsPerSecond() const
{
	// Per-type override (issue #122) - B0-0MR's slow-movement AC (issue #15) now
	// actually drives AEnemyBase::TickChaseMovement, not just a declared value.
	return MovementSpeed;
}

void ABomberEnemy::OnControlledEntry(EAbilitySlot Ability)
{
	// Clear the tell on any Controlled-entry interrupt (Alert/Attack -> Controlled),
	// so a bomber mid-telegraph doesn't keep showing an explosion that
	// AdvanceAttackTelegraph now guarantees will never fire. Root is the one exception
	// (bAllowsAttackWhileControlled) - see issue #255 - so the tell is deliberately
	// left alone for it.
	if (!AbilityData::Get(Ability).bAllowsAttackWhileControlled)
	{
		AttackTellLightComponent->SetIntensity(0.0f);
	}

	if (Ability != EAbilitySlot::Fear)
	{
		return;
	}
	// PRD 03: glow intensifies ONLY on Fear - every other ability, no response.
	CoreGlowLightComponent->SetIntensity(CoreGlowIntensifiedIntensity);
}

void ABomberEnemy::OnControlledExpired()
{
	// Root (bAllowsAttackWhileControlled) is the only ability that leaves the tell lit
	// through OnControlledEntry above, so this is the only case that can ever find it
	// still on here - safe to unconditionally clear on every Controlled -> Alert edge
	// (pass-1 review follow-up, issue #255).
	AttackTellLightComponent->SetIntensity(0.0f);
}

void ABomberEnemy::OnAttackEntry()
{
	AttackTellLightComponent->SetIntensity(AttackTellIntensity);
	RemainingTelegraphSeconds = AttackTelegraphSeconds;
	bExplodedForCurrentAttack = false;
	CurrentTelegraphStage = EBomberTelegraphStage::Early;

	// AttackTellSound defaults to a placeholder engine noise burst (see the
	// constructor), so this resolves normally out of the box; the else-branch below is
	// a defensive fallback for the case a Blueprint/Details-panel override explicitly
	// clears it.
	if (USoundBase* TellSound = AttackTellSound.LoadSynchronous())
	{
		AttackTellAudioComponent = UGameplayStatics::SpawnSoundAtLocation(this, TellSound, GetActorLocation());
	}
	else if (!bHasWarnedMissingAttackTellSound)
	{
		bHasWarnedMissingAttackTellSound = true;
		UE_LOG(LogTemp, Warning,
			TEXT("ABomberEnemy: no AttackTellSound configured on '%s' - attack telegraph will be silent."),
			*GetNameSafe(this));
	}
}

void ABomberEnemy::AdvanceAttackTelegraph(float DeltaSeconds)
{
	if (bExplodedForCurrentAttack || !IsAttackBehaviorActive())
	{
		return;
	}
	RemainingTelegraphSeconds = FMath::Max(0.0f, RemainingTelegraphSeconds - DeltaSeconds * GetControlledSpeedMultiplier());
	UpdateTelegraphEscalation();
	if (RemainingTelegraphSeconds <= 0.0f)
	{
		// "Fire exactly once" guard, mirroring ASniperEnemy::AdvanceAttackTelegraph.
		bExplodedForCurrentAttack = true;
		TriggerExplosion();
	}
}

void ABomberEnemy::UpdateTelegraphEscalation()
{
	// AttackTelegraphSeconds is ClampMin=0.0 but not guaranteed nonzero (a designer
	// could set it to 0) - treat that edge case as "already Imminent", the closest
	// safe interpretation, rather than dividing by zero.
	const float ElapsedFraction = AttackTelegraphSeconds > 0.0f
		? 1.0f - (RemainingTelegraphSeconds / AttackTelegraphSeconds)
		: 1.0f;

	// Floor fractions (0.3/0.5/0.7) are a second, independent escalation axis -
	// minimum brightness rises alongside pulse rate - deliberately hardcoded rather
	// than exposed as EditDefaultsOnly like PulseFrequencyHz above; revisit if a
	// designer needs to tune brightness floor independently of frequency.
	float PulseFrequencyHz = EarlyPulseFrequencyHz;
	float StageFloorFraction = 0.3f;
	if (ElapsedFraction >= TelegraphImminentThreshold)
	{
		CurrentTelegraphStage = EBomberTelegraphStage::Imminent;
		PulseFrequencyHz = ImminentPulseFrequencyHz;
		StageFloorFraction = 0.7f;
	}
	else if (ElapsedFraction >= TelegraphMidThreshold)
	{
		CurrentTelegraphStage = EBomberTelegraphStage::Mid;
		PulseFrequencyHz = MidPulseFrequencyHz;
		StageFloorFraction = 0.5f;
	}
	else
	{
		CurrentTelegraphStage = EBomberTelegraphStage::Early;
	}

	// Placeholder-quality flash (MISSION.md, docs/prd-teaching-arc.md REQ-4) -
	// sinusoidal so the tell never fully blacks out mid-fuse, pulsing between a
	// per-stage floor and the full AttackTellIntensity ceiling; PulseFrequencyHz
	// rising per stage is the "flash rate clearly increases" signal. Presentation
	// only - not asserted by the automation test, which checks
	// GetAttackTelegraphStage() instead (see BomberEnemy.h's comment on that
	// accessor for why).
	const float ElapsedSeconds = AttackTelegraphSeconds - RemainingTelegraphSeconds;
	const float PulseAlpha = 0.5f + 0.5f * FMath::Sin(ElapsedSeconds * PulseFrequencyHz * 2.0f * PI);
	const float FloorIntensity = AttackTellIntensity * StageFloorFraction;
	AttackTellLightComponent->SetIntensity(FMath::Lerp(FloorIntensity, AttackTellIntensity, PulseAlpha));
}

void ABomberEnemy::TriggerExplosion()
{
	OnBomberExploded.Broadcast();

	// FindPlayerEnergyComponent() tolerates GetWorld() being nullptr (true for a
	// NewObject<>()-constructed actor - friend-tested AdvanceAttackTelegraph calls hit
	// this) by returning nullptr, so the delegate above still fires unconditionally.
	if (UPlayerEnergyComponent* Energy = FindPlayerEnergyComponent())
	{
		// Clamped and floors at 0 by construction - ExplosionDamageAmount stays
		// large and never lethal through this, the only legal damage mutator.
		Energy->ApplyContactDamage(ExplosionDamageAmount, this);
	}
}

void ABomberEnemy::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	AdvanceAttackTelegraph(DeltaTime);
}
