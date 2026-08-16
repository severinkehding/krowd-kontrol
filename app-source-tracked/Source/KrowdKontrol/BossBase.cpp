#include "BossBase.h"

ABossBase::ABossBase()
{
	PrimaryActorTick.bCanEverTick = false;
}

void ABossBase::AdvanceToArmed()
{
	if (CurrentState != EBossState::Idle)
	{
		return;
	}
	CurrentState = EBossState::Armed;
}

void ABossBase::AdvanceToVulnerable()
{
	if (CurrentState != EBossState::Armed)
	{
		return;
	}
	CurrentState = EBossState::Vulnerable;
}

void ABossBase::TransitionToBanked()
{
	if (CurrentState != EBossState::Vulnerable)
	{
		return;
	}
	// Flip before broadcasting (see APlaceholderTerminalActor::Interact()) so a
	// listener that re-enters from within OnBossBanked sees the terminal state
	// immediately, and this method is already a no-op for it.
	CurrentState = EBossState::Banked;
	OnBossBanked.Broadcast();
}

void ABossBase::SetHasShield(bool bNewHasShield)
{
	if (bHasShield == bNewHasShield)
	{
		return;
	}
	bHasShield = bNewHasShield;
	OnShieldChanged(bHasShield);
}

void ABossBase::SetIsSplit(bool bNewIsSplit)
{
	if (bIsSplit == bNewIsSplit)
	{
		return;
	}
	bIsSplit = bNewIsSplit;
	OnSplitChanged(bIsSplit);
}

void ABossBase::SetIsEnraged(bool bNewIsEnraged)
{
	if (bIsEnraged == bNewIsEnraged)
	{
		return;
	}
	bIsEnraged = bNewIsEnraged;
	OnEnrageChanged(bIsEnraged);
}
