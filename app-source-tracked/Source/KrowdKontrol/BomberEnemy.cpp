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

ABomberEnemy::ABomberEnemy()
{
	PrimaryActorTick.bCanEverTick = true;

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	RootComponent = MeshComponent;
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
	// AdvanceAttackTelegraph now guarantees will never fire.
	AttackTellLightComponent->SetIntensity(0.0f);

	if (Ability != EAbilitySlot::Fear)
	{
		return;
	}
	// PRD 03: glow intensifies ONLY on Fear - every other ability, no response.
	CoreGlowLightComponent->SetIntensity(CoreGlowIntensifiedIntensity);
}

void ABomberEnemy::OnAttackEntry()
{
	AttackTellLightComponent->SetIntensity(AttackTellIntensity);
	RemainingTelegraphSeconds = AttackTelegraphSeconds;
	bExplodedForCurrentAttack = false;

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
	if (bExplodedForCurrentAttack || GetEnemyState() != EEnemyState::Attack)
	{
		return;
	}
	RemainingTelegraphSeconds = FMath::Max(0.0f, RemainingTelegraphSeconds - DeltaSeconds);
	if (RemainingTelegraphSeconds <= 0.0f)
	{
		// "Fire exactly once" guard, mirroring ASniperEnemy::AdvanceAttackTelegraph.
		bExplodedForCurrentAttack = true;
		TriggerExplosion();
	}
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
