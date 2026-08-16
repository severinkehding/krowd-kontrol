#include "MusicSubsystem.h"
#include "EnemyBase.h"
#include "EngineUtils.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"

void UMusicSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (bHasStartedInitialTrack)
	{
		return;
	}
	bHasStartedInitialTrack = true;

	// CurrentState starts at its member-initialized default (Calm). SetMusicState()'s
	// NewState == CurrentState no-op guard would otherwise swallow the very first
	// RefreshMusicState() call and playback would never actually start - see
	// PlayTrackForState() for the shared resolve/spawn/warn logic this bypasses the
	// guard to reach.
	PlayTrackForState(CurrentState);
}

void UMusicSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	RefreshMusicState();
}

TStatId UMusicSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UMusicSubsystem, STATGROUP_Tickables);
}

void UMusicSubsystem::RefreshMusicState()
{
	const EMusicState DesiredState = IsAnyEnemyInCombat() ? EMusicState::Combat : EMusicState::Calm;
	SetMusicState(DesiredState);
}

bool UMusicSubsystem::IsAnyEnemyInCombat() const
{
	if (!GetWorld())
	{
		return false;
	}
	for (TActorIterator<AEnemyBase> It(GetWorld()); It; ++It)
	{
		if (It->GetThreatState() == EThreatState::Hot)
		{
			return true;
		}
	}
	return false;
}

void UMusicSubsystem::SetMusicState(EMusicState NewState)
{
	if (NewState == CurrentState)
	{
		return;
	}

	// Flip before broadcasting (see GizmoNarrativeSubsystem::TriggerBark) so a
	// re-entrant listener sees CurrentState already updated.
	CurrentState = NewState;

	PlayTrackForState(NewState);

	OnMusicStateChanged.Broadcast(CurrentState);
}

void UMusicSubsystem::PlayTrackForState(EMusicState State)
{
	if (CurrentMusicComponent)
	{
		CurrentMusicComponent->FadeOut(CrossfadeDurationSeconds, 0.0f);
		CurrentMusicComponent = nullptr;
	}

	USoundBase* NextTrack = (State == EMusicState::Combat)
		? CombatTrack.LoadSynchronous()
		: CalmTrack.LoadSynchronous();

	if (NextTrack)
	{
		CurrentMusicComponent = UGameplayStatics::SpawnSound2D(this, NextTrack);
		if (CurrentMusicComponent)
		{
			CurrentMusicComponent->FadeIn(CrossfadeDurationSeconds, 1.0f);
		}
	}
	else if (!bHasWarnedMissingTrack)
	{
		// A still-unset CalmTrack/CombatTrack is a normal placeholder-first state
		// (matching RoomEnemyBudgetController::EnemyClassToSpawn's silent-left-unset
		// precedent) right up until DefaultGame.ini is expected to configure real
		// assets - past that point this is the only signal a typo'd/moved asset path
		// would ever produce, so it's worth one warning, not permanent silence.
		bHasWarnedMissingTrack = true;
		UE_LOG(LogTemp, Warning,
			TEXT("UMusicSubsystem: no track resolved for state %d - music will not play until CalmTrack/CombatTrack are configured."),
			static_cast<int32>(State));
	}
}
