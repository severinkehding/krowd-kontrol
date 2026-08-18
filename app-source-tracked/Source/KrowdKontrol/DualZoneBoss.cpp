#include "DualZoneBoss.h"
#include "TargetZone.h"
#include "Components/PointLightComponent.h"

ADualZoneBoss::ADualZoneBoss()
{
	PrimaryActorTick.bCanEverTick = false;

	// Boss actors have no mesh/visual representation yet anywhere in this codebase
	// (ABossBase is a pure logic actor) - this tell light becomes RootComponent
	// rather than attaching to a mesh, the same placeholder-first precedent
	// ASleepShieldBoss::ShieldTellLightComponent already established
	// (SleepShieldBoss.cpp).
	ArmingTellLightComponent = CreateDefaultSubobject<UPointLightComponent>(TEXT("ArmingTellLightComponent"));
	RootComponent = ArmingTellLightComponent;
	// PLACEHOLDER COLOUR - not one of MISSION.md Hard Invariant 3's five reserved
	// gameplay-information colours (Purple/Teal/Orange/Blue/White -
	// ReservedGameplayColours.h), and distinct from every tell colour already
	// claimed elsewhere in this module (TrooperEnemy (1.0,0.1,0.6), SniperEnemy
	// (1.0,0.85,0.1), BomberEnemy (1.0,0.15,0.05), RunnerEnemy (0.6,1.0,0.2),
	// ASleepShieldBoss's steel grey (0.55,0.6,0.65)) - a saturated violet reads as
	// "arming/charging".
	ArmingTellLightComponent->SetLightColor(FLinearColor(0.75f, 0.35f, 1.0f));
	ArmingTellLightComponent->SetIntensity(0.0f); // off until BeginPlay's AdvanceToArmed()
	ArmingTellLightComponent->SetAttenuationRadius(500.0f);

	// Always split, for the lifetime of this boss - there is no single-zone mode to
	// revert to, unlike ABossBase's own generic (subclass-agnostic) Split flag.
	SetIsSplit(true);
}

void ADualZoneBoss::BeginPlay()
{
	Super::BeginPlay();

	// Bound here, not the constructor: ZoneA/ZoneB are EditInstanceOnly and are only
	// resolved once this actor is placed and wired in a level, unlike ATargetZone's
	// own overlap handler (bound in ITS constructor because that binding is to a
	// component this class itself owns and constructs).
	//
	// AddDynamic, not ATargetZone's AddUniqueDynamic (TargetZone.cpp) - BeginPlay() only
	// ever runs once per ADualZoneBoss instance today (no EndPlay()/re-init path exists
	// on ABossBase or this subclass), so duplicate-binding protection has no case to guard
	// against yet; revisit if that stops being true.
	if (ZoneA)
	{
		ZoneA->OnActorBanked.AddDynamic(this, &ADualZoneBoss::HandleZoneABanked);
	}
	else
	{
		UE_LOG(LogTemp, Warning,
			TEXT("ADualZoneBoss '%s': ZoneA is unset - this side of the encounter will never bank."),
			*GetName());
	}
	if (ZoneB)
	{
		ZoneB->OnActorBanked.AddDynamic(this, &ADualZoneBoss::HandleZoneBBanked);
	}
	else
	{
		UE_LOG(LogTemp, Warning,
			TEXT("ADualZoneBoss '%s': ZoneB is unset - this side of the encounter will never bank."),
			*GetName());
	}

	// Distinctness check, matching the precedent this PR itself cites
	// (ADoorConnectorActor::RoomA/RoomB, DoorConnectorActor.h) - if a level author
	// wires the same zone actor into both slots, BankedCountA and BankedCountB stay
	// permanently equal and Enrage can never trigger. Warning only, not a hard
	// guard - both zones are still bound above, matching this method's existing
	// unset-reference warnings, which likewise leave the misconfiguration in place
	// rather than correcting it.
	if (ZoneA && ZoneA == ZoneB)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("ADualZoneBoss '%s': ZoneA and ZoneB are the same actor - imbalance can never diverge and Enrage will never trigger."),
			*GetName());
	}

	// Immediate, not timer-delayed: BeginPlay() is fight start (no earlier/later
	// "encounter start" trigger exists anywhere in this codebase yet), so this is
	// trivially "within the first 10 seconds" per the issue's AC without needing new
	// FTimerManager machinery the AC doesn't otherwise call for.
	AdvanceToArmed();
	// Lit the moment Armed is reached - the issue's AC #3 renderable "arming" signal.
	ArmingTellLightComponent->SetIntensity(ArmingTellIntensity);
}

void ADualZoneBoss::HandleZoneABanked(AActor* BankedActor)
{
	++BankedCountA;
	RecheckImbalance();
}

void ADualZoneBoss::HandleZoneBBanked(AActor* BankedActor)
{
	++BankedCountB;
	RecheckImbalance();
}

void ADualZoneBoss::RecheckImbalance()
{
	if (FMath::Abs(BankedCountA - BankedCountB) > EnrageImbalanceThreshold)
	{
		SetIsEnraged(true);
	}
}
