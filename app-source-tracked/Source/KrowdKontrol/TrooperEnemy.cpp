#include "TrooperEnemy.h"
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

ATrooperEnemy::ATrooperEnemy()
{
	PrimaryActorTick.bCanEverTick = true;

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	RootComponent = MeshComponent;

	EliteTrimLightComponent = CreateDefaultSubobject<UPointLightComponent>(TEXT("TrooperEliteTrimLightComponent"));
	EliteTrimLightComponent->SetupAttachment(MeshComponent);
	// PLACEHOLDER COLOUR - see RunnerEnemy.cpp's constructor comment for the full
	// rationale (colour choice, and why this is inline rather than a shared
	// AEnemyBase helper).
	EliteTrimLightComponent->SetLightColor(FLinearColor(0.1f, 1.0f, 0.15f));
	EliteTrimLightComponent->SetIntensity(0.0f); // off unless bIsElite
	EliteTrimLightComponent->SetAttenuationRadius(300.0f);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMeshFinder(
		TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (PlaneMeshFinder.Succeeded())
	{
		MeshComponent->SetStaticMesh(PlaneMeshFinder.Object);
	}
	// Distinct silhouette from Cube (APlaceholderCubeActor), Cylinder
	// (APlaceholderTargetZoneActor/APlaceholderTerminalActor), Cone (ASniperEnemy), and
	// Sphere (ABomberEnemy) - the last unclaimed /Engine/BasicShapes/ primitive. Rotated
	// 90 degrees about the roll axis so the plane stands as a flat vertical panel rather
	// than lying flat like a floor tile, and scaled non-uniformly so it reads as a
	// narrow standing panel, distinct from the other three shapes even desaturated.
	MeshComponent->SetRelativeRotation(FRotator(0.0f, 0.0f, 90.0f));
	MeshComponent->SetRelativeScale3D(FVector(1.2f, 0.15f, 1.6f));

	GlowLightComponent = CreateDefaultSubobject<UPointLightComponent>(TEXT("GlowLightComponent"));
	GlowLightComponent->SetupAttachment(MeshComponent);
	// Teal is TR-UPR's locked information colour (MISSION.md Hard Invariant 3) - used
	// here ONLY as this signal, never as decoration.
	GlowLightComponent->SetLightColor(ReservedGameplayColours::GetTeal());
	GlowLightComponent->SetIntensity(GlowBaselineIntensity);
	GlowLightComponent->SetAttenuationRadius(300.0f);

	AttackTellLightComponent = CreateDefaultSubobject<UPointLightComponent>(TEXT("AttackTellLightComponent"));
	AttackTellLightComponent->SetupAttachment(MeshComponent);
	// PLACEHOLDER COLOUR - deliberately NOT one of MISSION.md Hard Invariant 3's five
	// reserved gameplay-information colours (Purple/Teal/Orange/Blue/White), same
	// caveat ASniperEnemy/ABomberEnemy's own tell colours document, and distinct from
	// both of theirs ((1.0,0.85,0.1) and (1.0,0.15,0.05) respectively).
	AttackTellLightComponent->SetLightColor(FLinearColor(1.0f, 0.1f, 0.6f));
	AttackTellLightComponent->SetIntensity(0.0f); // off until Attack entry
	AttackTellLightComponent->SetAttenuationRadius(300.0f);

	EnemyTypeIndicatorComponent = CreateDefaultSubobject<UEnemyTypeIndicatorComponent>(TEXT("EnemyTypeIndicatorComponent"));
	EnemyTypeIndicatorComponent->EnemyType = EEnemyType::TR_UPR;

	// Placeholder-first default (MISSION.md) so issue #30's "a distinct sound
	// effect plays" AC holds without waiting on a designer to configure
	// AttackTellSound. /Engine/EngineSounds/ has only two playable USoundBase
	// assets (1kSineTonePing, WhiteNoise) and both are already claimed by
	// ASniperEnemy/ABomberEnemy respectively (verified against the installed
	// Engine content) - reusing either here would make two enemy types share an
	// "audibly distinct" tell, contradicting issue #30's AC directly. This uses a
	// different built-in Engine asset instead: a short notification chime, always
	// present in a stock Engine install and loadable in the Editor context this
	// project's Automation tests actually run in (no packaging/cook step exists
	// yet - see harness/README.md). Revisit alongside #36/#33's own "sourcing a
	// real sound" follow-up once one exists. Still Details-panel/Blueprint
	// overridable.
	static ConstructorHelpers::FObjectFinder<USoundBase> AttackTellSoundFinder(
		TEXT("/Engine/EditorSounds/Notifications/CompileSuccess.CompileSuccess"));
	if (AttackTellSoundFinder.Succeeded())
	{
		AttackTellSound = AttackTellSoundFinder.Object;
	}
}

float ATrooperEnemy::GetAttackRangeUnits() const
{
	// Medium range - strictly between ABomberEnemy's short 150.0f and ASniperEnemy's
	// long 1400.0f.
	return 700.0f;
}

void ATrooperEnemy::OnControlledEntry(EAbilitySlot Ability)
{
	// ReceiveControl only calls this from Alert/Attack, so any in-progress attack
	// telegraph is aborted the moment Controlled is entered - clear the tell
	// regardless of which ability triggered this, same rationale ASniperEnemy/
	// ABomberEnemy's own OnControlledEntry documents.
	AttackTellLightComponent->SetIntensity(0.0f);

	if (Ability != EAbilitySlot::Root)
	{
		return;
	}
	// PRD 03 REQ-3: glow visibly intensifies ONLY on Root specifically - every other
	// ability produces no glow response at all.
	GlowLightComponent->SetIntensity(GlowIntensifiedIntensity);
}

void ATrooperEnemy::OnAttackEntry()
{
	AttackTellLightComponent->SetIntensity(AttackTellIntensity);
	RemainingTelegraphSeconds = AttackTelegraphSeconds;

	// AttackTellSound defaults to a placeholder engine chime (see the
	// constructor), so this resolves normally out of the box; the else-branch
	// below is a defensive fallback for the case a Blueprint/Details-panel
	// override explicitly clears it. Fires exactly once per Attack entry -
	// AdvanceAttackTelegraph()'s rapid re-arm loop below re-fires
	// OnTrooperRayFired repeatedly without ever re-calling OnAttackEntry(), so
	// this deliberately does NOT repeat per ray (issue #30 AC: "plays once per
	// telegraph - not looping").
	if (USoundBase* TellSound = AttackTellSound.LoadSynchronous())
	{
		AttackTellAudioComponent = UGameplayStatics::SpawnSoundAtLocation(this, TellSound, GetActorLocation());
	}
	else if (!bHasWarnedMissingAttackTellSound)
	{
		bHasWarnedMissingAttackTellSound = true;
		UE_LOG(LogTemp, Warning,
			TEXT("ATrooperEnemy: no AttackTellSound configured on '%s' - attack telegraph will be silent."),
			*GetNameSafe(this));
	}
}

void ATrooperEnemy::AdvanceAttackTelegraph(float DeltaSeconds)
{
	if (GetEnemyState() != EEnemyState::Attack)
	{
		return;
	}
	RemainingTelegraphSeconds = FMath::Max(0.0f, RemainingTelegraphSeconds - DeltaSeconds);
	if (RemainingTelegraphSeconds <= 0.0f)
	{
		OnTrooperRayFired.Broadcast();
		// Rapid re-fire (PRD 03: "rapid single-ray attacks") - re-arm immediately for
		// the next ray instead of latching a fire-once guard like ASniperEnemy/
		// ABomberEnemy do. GetEnemyState() != Attack above is what actually stops the
		// loop, once ReceiveControl moves this enemy to Controlled.
		RemainingTelegraphSeconds = AttackTelegraphSeconds;
	}
}

void ATrooperEnemy::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	AdvanceAttackTelegraph(DeltaTime);
}
