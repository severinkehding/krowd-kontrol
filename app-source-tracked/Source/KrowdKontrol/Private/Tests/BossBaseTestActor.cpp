#include "BossBaseTestActor.h"

void ABossBaseTestActor::OnShieldChanged(bool bNewHasShield)
{
	++ShieldChangedCallCount;
}

void ABossBaseTestActor::OnSplitChanged(bool bNewIsSplit)
{
	++SplitChangedCallCount;
}

void ABossBaseTestActor::OnEnrageChanged(bool bNewIsEnraged)
{
	++EnrageChangedCallCount;
}
