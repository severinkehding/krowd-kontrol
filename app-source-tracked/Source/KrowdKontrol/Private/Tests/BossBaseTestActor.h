#pragma once

#include "CoreMinimal.h"
#include "BossBase.h"
#include "BossBaseTestActor.generated.h"

class USceneComponent;

// Minimal test-only subclass of ABossBase (issue #44) overriding the shield/split/
// enrage hooks to count invocations, so KrowdKontrolBossBaseTest.cpp can confirm the
// hooks actually fire on real changes (and are skipped on redundant same-value
// sets), not just that the flag getters report the right value. Used by that test and
// by KrowdKontrolBossCheckpointRestartTest.cpp (issue #173), which additionally needs
// a real, settable location - ABossBase itself has no RootComponent (real placed
// bosses are Blueprint subclasses whose mesh supplies one), so a bare SpawnActor<>()
// of the base class always reports GetActorLocation() as the origin. ABossBase is
// UCLASS(Abstract), so a test-only concrete subclass is required to NewObject<>() it
// at all.
UCLASS()
class ABossBaseTestActor : public ABossBase
{
	GENERATED_BODY()

public:
	ABossBaseTestActor();

	int32 ShieldChangedCallCount = 0;
	int32 SplitChangedCallCount = 0;
	int32 EnrageChangedCallCount = 0;

protected:
	virtual void OnShieldChanged(bool bNewHasShield) override;
	virtual void OnSplitChanged(bool bNewIsSplit) override;
	virtual void OnEnrageChanged(bool bNewIsEnraged) override;

private:
	UPROPERTY()
	TObjectPtr<USceneComponent> TestRootComponent;
};
