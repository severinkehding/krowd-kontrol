#include "BossBaseTestActor.h"
#include "Components/SceneComponent.h"

ABossBaseTestActor::ABossBaseTestActor()
{
	TestRootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("TestRootComponent"));
	SetRootComponent(TestRootComponent);
}

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
