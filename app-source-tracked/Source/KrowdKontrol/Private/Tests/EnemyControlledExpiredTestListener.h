#pragma once
#include "CoreMinimal.h"
#include "EnemyControlledExpiredTestListener.generated.h"

// Test-only listener for AEnemyBase::OnEnemyControlledExpired (issue #174). Dynamic
// multicast delegates only bind to UFUNCTIONs via AddDynamic - no AddLambda - so
// counting broadcasts in tests needs this rather than a capturing lambda. Mirrors
// UEnemyBankedTestListener's shape, adapted to FOnEnemyControlledExpired's
// (parameterless) signature.
UCLASS()
class UEnemyControlledExpiredTestListener : public UObject
{
	GENERATED_BODY()

public:
	int32 CallCount = 0;

	UFUNCTION()
	void HandleEnemyControlledExpired();
};
