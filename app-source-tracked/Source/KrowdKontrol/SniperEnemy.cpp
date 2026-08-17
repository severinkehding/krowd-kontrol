#include "SniperEnemy.h"
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

ASniperEnemy::ASniperEnemy()
{
	PrimaryActorTick.bCanEverTick = true;

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	RootComponent = MeshComponent;

	EliteTrimLightComponent = CreateDefaultSubobject<UPointLightComponent>(TEXT("SniperEliteTrimLightComponent"));
	EliteTrimLightComponent->SetupAttachment(MeshComponent);
	// PLACEHOLDER COLOUR - see RunnerEnemy.cpp's constructor comment for the full
	// rationale (colour choice, and why this is inline rather than a shared
	// AEnemyBase helper).
	EliteTrimLightComponent->SetLightColor(FLinearColor(0.1f, 1.0f, 0.15f));
	EliteTrimLightComponent->SetIntensity(0.0f); // off unless bIsElite
	EliteTrimLightComponent->SetAttenuationRadius(300.0f);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeMeshFinder(
		TEXT("/Engine/BasicShapes/Cone.Cone"));
	if (ConeMeshFinder.Succeeded())
	{
		MeshComponent->SetStaticMesh(ConeMeshFinder.Object);
	}
	// Distinct silhouette from other placeholder shapes already in this module
	// (Cube - APlaceholderCubeActor; Cylinder - APlaceholderTargetZoneActor) so
	// SN-1PR reads as a different enemy type by shape alone, even with colour
	// removed/desaturated (issue #17 AC). The other 3 core enemy types (issues
	// #13/#14/#15) must each pick a shape distinct from this one and each other.
	MeshComponent->SetRelativeScale3D(FVector(1.0f, 1.0f, 1.8f));

	EyeGlowLightComponent = CreateDefaultSubobject<UPointLightComponent>(TEXT("EyeGlowLightComponent"));
	EyeGlowLightComponent->SetupAttachment(MeshComponent);
	// Blue is SN-1PR's locked information colour (MISSION.md Hard Invariant 3) -
	// used here ONLY as this signal, never as decoration.
	EyeGlowLightComponent->SetLightColor(ReservedGameplayColours::GetBlue());
	EyeGlowLightComponent->SetIntensity(EyeGlowBaselineIntensity);
	EyeGlowLightComponent->SetAttenuationRadius(300.0f);

	AttackTellLightComponent = CreateDefaultSubobject<UPointLightComponent>(TEXT("AttackTellLightComponent"));
	AttackTellLightComponent->SetupAttachment(MeshComponent);
	// PLACEHOLDER COLOUR - deliberately NOT one of MISSION.md Hard Invariant 3's
	// five reserved gameplay-information colours (Purple/Teal/Orange/Blue/White),
	// same caveat APlaceholderTargetZoneActor's beacon colour documents: needs a
	// human design ruling before this is replaced with the real tell VFX, and must
	// stay visually distinct from whatever colour issues #13/#14/#15 pick for
	// their own attack tells.
	AttackTellLightComponent->SetLightColor(FLinearColor(1.0f, 0.85f, 0.1f));
	AttackTellLightComponent->SetIntensity(0.0f); // off until Attack entry
	AttackTellLightComponent->SetAttenuationRadius(300.0f);

	EnemyTypeIndicatorComponent = CreateDefaultSubobject<UEnemyTypeIndicatorComponent>(TEXT("EnemyTypeIndicatorComponent"));
	EnemyTypeIndicatorComponent->EnemyType = EEnemyType::SN_1PR;

	// Placeholder-first default (MISSION.md) so issue #36's "a distinct sound effect
	// plays" AC holds without waiting on a designer to configure AttackTellSound - a
	// short, clean built-in tone, distinct from CalmTrack/CombatTrack (music loops) and
	// from any future ability-cast/UI sound. Still Details-panel/Blueprint overridable
	// once a real per-enemy-type tell is sourced.
	static ConstructorHelpers::FObjectFinder<USoundBase> AttackTellSoundFinder(
		TEXT("/Engine/EngineSounds/1kSineTonePing.1kSineTonePing"));
	if (AttackTellSoundFinder.Succeeded())
	{
		AttackTellSound = AttackTellSoundFinder.Object;
	}
}

float ASniperEnemy::GetAttackRangeUnits() const
{
	// Deliberately close to DetectionRangeUnits's default (1500.0f, inherited
	// unchanged), so SN-1PR enters Attack almost immediately after Alert, without
	// needing to close distance. This is the mechanical definition of "long-range" in
	// this state machine - SN-1PR doesn't chase, it just needs to be in Alert (i.e.
	// the player already within DetectionRangeUnits) to also already be within attack
	// range.
	return 1400.0f;
}

void ASniperEnemy::OnControlledEntry(EAbilitySlot Ability)
{
	// ReceiveControl only calls this from Alert/Attack, so any in-progress attack
	// telegraph is aborted the moment Controlled is entered - clear the tell
	// regardless of which ability triggered this, so a sniper put to sleep/rooted/
	// etc. mid-telegraph doesn't keep showing a shot that AdvanceAttackTelegraph now
	// guarantees will never fire.
	AttackTellLightComponent->SetIntensity(0.0f);

	if (Ability != EAbilitySlot::Sleep)
	{
		return;
	}
	// PRD 03 REQ-3: glow visibly intensifies ONLY on Sleep specifically - every other
	// ability produces no glow response at all.
	EyeGlowLightComponent->SetIntensity(EyeGlowIntensifiedIntensity);
}

void ASniperEnemy::OnAttackEntry()
{
	AttackTellLightComponent->SetIntensity(AttackTellIntensity);
	RemainingTelegraphSeconds = AttackTelegraphSeconds;
	bShotFiredForCurrentAttack = false;

	// AttackTellSound defaults to a placeholder engine tone (see the constructor), so
	// this resolves normally out of the box; the else-branch below is a defensive
	// fallback for the case a Blueprint/Details-panel override explicitly clears it.
	if (USoundBase* TellSound = AttackTellSound.LoadSynchronous())
	{
		AttackTellAudioComponent = UGameplayStatics::SpawnSoundAtLocation(this, TellSound, GetActorLocation());
	}
	else if (!bHasWarnedMissingAttackTellSound)
	{
		bHasWarnedMissingAttackTellSound = true;
		UE_LOG(LogTemp, Warning,
			TEXT("ASniperEnemy: no AttackTellSound configured on '%s' - attack telegraph will be silent."),
			*GetNameSafe(this));
	}
}

void ASniperEnemy::AdvanceAttackTelegraph(float DeltaSeconds)
{
	if (bShotFiredForCurrentAttack || GetEnemyState() != EEnemyState::Attack)
	{
		return;
	}
	RemainingTelegraphSeconds = FMath::Max(0.0f, RemainingTelegraphSeconds - DeltaSeconds);
	if (RemainingTelegraphSeconds <= 0.0f)
	{
		// Guards against re-firing OnSniperShotFired every subsequent tick once the
		// telegraph reaches zero - the same "fire exactly once" shape
		// ABossBase::TransitionToBanked/FOnBossBanked already established for this
		// module's delegates.
		bShotFiredForCurrentAttack = true;
		OnSniperShotFired.Broadcast();
	}
}

void ASniperEnemy::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	AdvanceAttackTelegraph(DeltaTime);
}
