#include "DualZoneBoss.h"
#include "TargetZone.h"

ADualZoneBoss::ADualZoneBoss()
{
	PrimaryActorTick.bCanEverTick = false;
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

	// Immediate, not timer-delayed: BeginPlay() is fight start (no earlier/later
	// "encounter start" trigger exists anywhere in this codebase yet), so this is
	// trivially "within the first 10 seconds" per the issue's AC without needing new
	// FTimerManager machinery the AC doesn't otherwise call for.
	AdvanceToArmed();
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
