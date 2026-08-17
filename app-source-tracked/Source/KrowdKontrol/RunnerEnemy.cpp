#include "RunnerEnemy.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "ReservedGameplayColours.h"
#include "EnemyTypeIndicatorComponent.h"
#include "EnemyType.h"

ARunnerEnemy::ARunnerEnemy()
{
	PrimaryActorTick.bCanEverTick = true;

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	RootComponent = MeshComponent;
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
	// telegraph is aborted the moment Controlled is entered - clear the tell
	// regardless of which ability triggered this, same rationale every sibling's own
	// OnControlledEntry documents.
	AttackTellLightComponent->SetIntensity(0.0f);

	if (Ability != EAbilitySlot::Snare)
	{
		return;
	}
	// PRD 03 REQ-3: glow visibly intensifies ONLY on Snare specifically - every other
	// ability produces no glow response at all.
	DrainGlowLightComponent->SetIntensity(DrainGlowIntensifiedIntensity);
}

void ARunnerEnemy::OnAttackEntry()
{
	AttackTellLightComponent->SetIntensity(AttackTellIntensity);
	RemainingTelegraphSeconds = AttackTelegraphSeconds;
	bDrainFiredForCurrentAttack = false;
}

void ARunnerEnemy::AdvanceAttackTelegraph(float DeltaSeconds)
{
	if (bDrainFiredForCurrentAttack || GetEnemyState() != EEnemyState::Attack)
	{
		return;
	}
	RemainingTelegraphSeconds = FMath::Max(0.0f, RemainingTelegraphSeconds - DeltaSeconds);
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
