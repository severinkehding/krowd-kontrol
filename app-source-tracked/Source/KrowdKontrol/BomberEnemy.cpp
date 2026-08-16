#include "BomberEnemy.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "ReservedGameplayColours.h"
#include "GameFramework/Pawn.h"
#include "PlayerEnergyComponent.h"
#include "EngineUtils.h"

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
}

float ABomberEnemy::GetAttackRangeUnits() const
{
	// Small relative to DetectionRangeUnits's default (1500.0f) - the mechanical
	// definition of "short-range", the opposite of ASniperEnemy's own override.
	return 150.0f;
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

	// GetWorld() is nullptr for a NewObject<>()-constructed actor - friend-tested
	// AdvanceAttackTelegraph calls hit this; guarded so the delegate still fires.
	if (!GetWorld())
	{
		return;
	}
	// TActorIterator, not UGameplayStatics::GetPlayerPawn() - the latter needs a
	// driven World->BeginPlay() pass (never run by this module's Automation tests,
	// same gap KrowdKontrolRoomEnemyBudgetControllerTest.cpp documents for component
	// BeginPlay) to populate the controller/GameInstance registries it depends on.
	for (TActorIterator<APawn> It(GetWorld()); It; ++It)
	{
		if (UPlayerEnergyComponent* Energy = It->FindComponentByClass<UPlayerEnergyComponent>())
		{
			// Clamped and floors at 0 by construction - ExplosionDamageAmount stays
			// large and never lethal through this, the only legal damage mutator.
			Energy->ApplyContactDamage(ExplosionDamageAmount, this);
			return;
		}
	}
}

void ABomberEnemy::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	AdvanceAttackTelegraph(DeltaTime);
}
