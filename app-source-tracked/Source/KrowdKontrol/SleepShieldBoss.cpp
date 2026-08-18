#include "SleepShieldBoss.h"
#include "EnemyBase.h"
#include "AbilitySlot.h"
#include "EngineUtils.h"
#include "Components/PointLightComponent.h"

// Two independently-toggling signals layered on ABossBase (issue #44): HasShield()
// (inherited) freely flips true/false on every CheckShieldState() call, tracking
// live Sleep-controlled-minion proximity moment to moment - this is what
// "periodically shields itself" and "blocks all player damage/control input...
// while active" (issue #48 AC #1) actually gate, and it is what the tell light
// reflects. EBossState::Vulnerable, by contrast, only ever advances once
// (ABossBase's Armed->Vulnerable edge has no reverse edge, by design - see
// BossBase.h) the FIRST time a qualifying minion is found nearby; it marks "this
// encounter has proven the mechanic and is now herd-to-target-zone eligible"
// (issue #48 AC #4), independent of whichever way the shield is currently facing
// afterward. This is the only reading of the AC set that both fits ABossBase's
// documented one-way guarantee (KrowdKontrolBossBaseTest.cpp) and satisfies AC
// #5's literal "test confirms ... transitions to Vulnerable once one is present."

ASleepShieldBoss::ASleepShieldBoss()
{
	// Unlike ABossBase (never ticks; every transition is externally driven), this
	// subclass must self-scan for nearby Sleep-controlled minions every frame -
	// deliberate, not an oversight, same divergence AEnemyBase's own constructor
	// comment documents for itself.
	PrimaryActorTick.bCanEverTick = true;

	// Boss actors have no mesh/visual representation yet anywhere in this codebase
	// (ABossBase is a pure logic actor) - this tell light becomes RootComponent
	// rather than attaching to a mesh, matching that placeholder-art-first state.
	ShieldTellLightComponent = CreateDefaultSubobject<UPointLightComponent>(TEXT("ShieldTellLightComponent"));
	RootComponent = ShieldTellLightComponent;
	// PLACEHOLDER COLOUR - deliberately NOT one of MISSION.md Hard Invariant 3's
	// five reserved gameplay-information colours (Purple/Teal/Orange/Blue/White -
	// Blue in particular is SN-1PR/Sleep's own reserved colour and must not be
	// reused here for a non-informational shield tell), and distinct from every
	// enemy attack-tell colour already claimed (TrooperEnemy's (1.0,0.1,0.6),
	// SniperEnemy's (1.0,0.85,0.1), BomberEnemy's (1.0,0.15,0.05), RunnerEnemy's
	// (0.6,1.0,0.2)) - a desaturated steel tone reads as "shield/armour".
	ShieldTellLightComponent->SetLightColor(FLinearColor(0.55f, 0.6f, 0.65f));
	ShieldTellLightComponent->SetIntensity(0.0f); // off until BeginPlay's SetHasShield(true)
	ShieldTellLightComponent->SetAttenuationRadius(300.0f);
}

void ASleepShieldBoss::BeginPlay()
{
	Super::BeginPlay();

	// Immediate, not timer-delayed: BeginPlay() is fight start - trivially within
	// issue #48 AC #3's "first 10 seconds" window without new FTimerManager
	// machinery.
	AdvanceToArmed();
	SetHasShield(true);
	BossStateReflected = GetBossState();
}

void ASleepShieldBoss::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	CheckShieldState();
}

void ASleepShieldBoss::CheckShieldState()
{
	// A boss already delivered to Banked (whatever future system calls
	// TransitionToBanked() - out of scope here, see the plan's NOT Building
	// section) has nothing left to shield; stop re-toggling.
	if (GetBossState() == EBossState::Banked)
	{
		return;
	}

	const bool bQualifyingMinionNearby = HasNearbySleepControlledMinion();
	SetHasShield(!bQualifyingMinionNearby);

	if (bQualifyingMinionNearby)
	{
		// No-op once already past Armed - see ABossBase::AdvanceToVulnerable.
		AdvanceToVulnerable();
	}
	BossStateReflected = GetBossState();
}

bool ASleepShieldBoss::HasNearbySleepControlledMinion() const
{
	if (!GetWorld())
	{
		return false;
	}

	const float RadiusSquared = FMath::Square(ShieldDropRadiusUnits);
	for (TActorIterator<AEnemyBase> It(GetWorld()); It; ++It)
	{
		if (It->GetEnemyState() != EEnemyState::Controlled || It->GetControllingAbility() != EAbilitySlot::Sleep)
		{
			continue;
		}
		if (FVector::DistSquared(It->GetActorLocation(), GetActorLocation()) <= RadiusSquared)
		{
			return true;
		}
	}
	return false;
}

void ASleepShieldBoss::OnShieldChanged(bool bNewHasShield)
{
	ShieldTellLightComponent->SetIntensity(bNewHasShield ? ShieldTellIntensity : 0.0f);
	bHasShieldReflected = bNewHasShield;
}
