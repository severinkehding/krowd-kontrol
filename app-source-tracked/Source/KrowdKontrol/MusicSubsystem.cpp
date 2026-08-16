#include "MusicSubsystem.h"
#include "EnemyBase.h"
#include "EngineUtils.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"

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

	if (CurrentMusicComponent)
	{
		CurrentMusicComponent->FadeOut(CrossfadeDurationSeconds, 0.0f);
		CurrentMusicComponent = nullptr;
	}

	// Flip before broadcasting (see GizmoNarrativeSubsystem::TriggerBark) so a
	// re-entrant listener sees CurrentState already updated.
	CurrentState = NewState;

	USoundBase* NextTrack = (NewState == EMusicState::Combat)
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
	// No track configured yet is a normal placeholder-first state, not an error - no
	// warning logged, matching RoomEnemyBudgetController::EnemyClassToSpawn's silent
	// left-unset precedent. CurrentState/OnMusicStateChanged still update either way,
	// so the switching logic itself is fully testable without a real asset.

	OnMusicStateChanged.Broadcast(CurrentState);
}
