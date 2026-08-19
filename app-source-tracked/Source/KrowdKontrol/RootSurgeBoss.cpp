#include "RootSurgeBoss.h"
#include "TrooperEnemy.h"
#include "EnemyBase.h"
#include "AbilitySlot.h"
#include "PlayerEnergyComponent.h"
#include "WaveSpawnerComponent.h"
#include "EngineUtils.h"
#include "Components/PointLightComponent.h"
#include "GameFramework/Pawn.h"

ARootSurgeBoss::ARootSurgeBoss()
{
	// Unlike ABossBase (never ticks; every transition is externally driven), this
	// subclass must self-scan for its own Root-locked adds AND advance its own attack
	// telegraph every frame - deliberate, not an oversight, same divergence
	// ASleepShieldBoss's constructor comment documents for itself.
	PrimaryActorTick.bCanEverTick = true;

	// Boss actors have no mesh/visual representation yet anywhere in this codebase
	// (ABossBase is a pure logic actor) - this tell light becomes RootComponent rather
	// than attaching to a mesh, the same placeholder-first precedent
	// ASleepShieldBoss::ShieldTellLightComponent/ADualZoneBoss::ArmingTellLightComponent
	// already established.
	ArmingTellLightComponent = CreateDefaultSubobject<UPointLightComponent>(TEXT("ArmingTellLightComponent"));
	RootComponent = ArmingTellLightComponent;
	// PLACEHOLDER COLOUR - not one of MISSION.md Hard Invariant 3's five reserved
	// gameplay-information colours (Purple/Teal/Orange/Blue/White), and distinct from
	// every tell colour already claimed elsewhere in this module (TrooperEnemy
	// (1.0,0.1,0.6), SniperEnemy (1.0,0.85,0.1), BomberEnemy (1.0,0.15,0.05), RunnerEnemy
	// (0.6,1.0,0.2), ASleepShieldBoss's steel grey (0.55,0.6,0.65), ADualZoneBoss's
	// violet (0.75,0.35,1.0)) - an orchid/electric-pink reads as "arming/charging".
	ArmingTellLightComponent->SetLightColor(FLinearColor(0.95f, 0.55f, 0.85f));
	ArmingTellLightComponent->SetIntensity(0.0f); // off until BeginPlay's AdvanceToArmed()
	ArmingTellLightComponent->SetAttenuationRadius(500.0f);

	AttackTellLightComponent = CreateDefaultSubobject<UPointLightComponent>(TEXT("AttackTellLightComponent"));
	AttackTellLightComponent->SetupAttachment(RootComponent);
	// PLACEHOLDER COLOUR - deliberately distinct from ArmingTellLightComponent's own
	// colour above, same "arming tell" vs. "attack tell" distinction ATrooperEnemy's
	// GlowLightComponent/AttackTellLightComponent pair draws. Also distinct from every
	// tell colour already claimed elsewhere in this module (same cross-check as
	// ArmingTellLightComponent above): TrooperEnemy (1.0,0.1,0.6), SniperEnemy
	// (1.0,0.85,0.1), BomberEnemy (1.0,0.15,0.05), RunnerEnemy (0.6,1.0,0.2),
	// ASleepShieldBoss's steel grey (0.55,0.6,0.65), ADualZoneBoss's violet
	// (0.75,0.35,1.0), this boss's own ArmingTellLightComponent (0.95,0.55,0.85) - this
	// boss's own WaveSpawnerComponent spawns ATrooperEnemy adds every wave, so reusing
	// TrooperEnemy's attack-tell colour would make the boss's own attack indistinguishable
	// from its adds' attacks during the fight; a crimson red reads as "boss attack".
	AttackTellLightComponent->SetLightColor(FLinearColor(0.85f, 0.0f, 0.15f));
	AttackTellLightComponent->SetIntensity(0.0f); // off until it fires
	AttackTellLightComponent->SetAttenuationRadius(300.0f);

	// PLACEHOLDERS - "as authored for a normal (non-boss) encounter"; only the
	// accelerated-cadence relationship to WaveDelayAccelerationMultiplier is AC-mandated
	// (AC #5), not these specific numbers.
	BaselineWaveDelaySeconds = { 6.0f, 6.0f, 6.0f };
	WaveDelayAccelerationMultiplier = 0.5f;

	WaveSpawnerComponent = CreateDefaultSubobject<UWaveSpawnerComponent>(TEXT("WaveSpawnerComponent"));
	for (float BaselineDelay : BaselineWaveDelaySeconds)
	{
		FWaveEntry Entry;
		Entry.EnemyType = EEnemyType::TR_UPR;
		Entry.EnemyClass = ATrooperEnemy::StaticClass();
		Entry.Count = 1;
		Entry.DelaySeconds = BaselineDelay * WaveDelayAccelerationMultiplier;
		WaveSpawnerComponent->Waves.Add(Entry);
	}
}

void ARootSurgeBoss::BeginPlay()
{
	Super::BeginPlay();

	// Immediate, not timer-delayed: BeginPlay() is fight start - trivially within issue
	// #50 AC #3's "first 10 seconds" window without new FTimerManager machinery, same
	// precedent ASleepShieldBoss/ADualZoneBoss's own BeginPlay() already establishes.
	AdvanceToArmed();
	ArmingTellLightComponent->SetIntensity(ArmingTellIntensity);
	WaveSpawnerComponent->StartWaves();

	// Must be initialized here, not left at its 0.0f constructor default, so the first
	// telegraph doesn't fire on tick 0.
	RemainingTelegraphSeconds = AttackTelegraphSeconds;
}

void ARootSurgeBoss::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	CheckVulnerableState();
	AdvanceAttackTelegraph(DeltaTime);
}

void ARootSurgeBoss::CheckVulnerableState()
{
	// A boss already delivered to Banked (whatever future system calls
	// TransitionToBanked() - out of scope here, same deferral
	// ASleepShieldBoss::CheckShieldState() documents) has nothing left to check.
	if (GetBossState() == EBossState::Banked)
	{
		return;
	}

	if (HasRootLockedAdd())
	{
		// No-op once already past Armed - see ABossBase::AdvanceToVulnerable.
		AdvanceToVulnerable();
	}
}

bool ARootSurgeBoss::HasRootLockedAdd() const
{
	for (const TObjectPtr<AActor>& SpawnedActor : WaveSpawnerComponent->GetSpawnedActors())
	{
		const AEnemyBase* SpawnedEnemy = Cast<AEnemyBase>(SpawnedActor);
		if (!SpawnedEnemy)
		{
			continue;
		}
		// State check strictly before the stale-tolerant GetControllingAbility() read -
		// mirrors ASleepShieldBoss::HasNearbySleepControlledMinion()'s exact guard order.
		if (SpawnedEnemy->GetEnemyState() == EEnemyState::Controlled
			&& SpawnedEnemy->GetControllingAbility() == EAbilitySlot::Root)
		{
			return true;
		}
	}
	return false;
}

void ARootSurgeBoss::AdvanceAttackTelegraph(float DeltaSeconds)
{
	// A Banked boss must stop attacking - same "stop once Banked" precedent
	// ASleepShieldBoss::CheckShieldState()'s early-return guard establishes.
	if (GetBossState() == EBossState::Banked)
	{
		return;
	}

	RemainingTelegraphSeconds = FMath::Max(0.0f, RemainingTelegraphSeconds - DeltaSeconds);
	if (RemainingTelegraphSeconds <= 0.0f)
	{
		AttackTellLightComponent->SetIntensity(AttackTellIntensity);

		// FindPlayerEnergyComponent() is protected on AEnemyBase, not reachable from a
		// ABossBase subclass - duplicated inline here, same TActorIterator<APawn> body
		// EnemyBase.cpp:132-149 uses (no shared free function exists to call instead).
		UPlayerEnergyComponent* Energy = nullptr;
		if (GetWorld())
		{
			for (TActorIterator<APawn> It(GetWorld()); It; ++It)
			{
				if (UPlayerEnergyComponent* Found = It->FindComponentByClass<UPlayerEnergyComponent>())
				{
					Energy = Found;
					break;
				}
			}
		}
		if (Energy)
		{
			Energy->ApplyContactDamage(AttackDamageAmount, this);
		}

		// Rapid re-arm (mirrors ATrooperEnemy::AdvanceAttackTelegraph - no fire-once
		// guard, unlike ABomberEnemy) - "managing the boss's own attack under time
		// pressure" implies a repeating threat, not a single trap.
		RemainingTelegraphSeconds = AttackTelegraphSeconds;
	}
}
