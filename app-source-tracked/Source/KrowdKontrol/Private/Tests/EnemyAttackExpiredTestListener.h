#pragma once
#include "CoreMinimal.h"
#include "EnemyAttackExpiredTestListener.generated.h"

// Test-only listener for AEnemyBase::OnEnemyAttackExpired (issue #313). Dynamic
// multicast delegates only bind to UFUNCTIONs via AddDynamic - no AddLambda - so
// counting broadcasts in tests needs this rather than a capturing lambda. Mirrors
// UEnemyControlledExpiredTestListener's shape, adapted to FOnEnemyAttackExpired's
// (parameterless) signature.
UCLASS()
class UEnemyAttackExpiredTestListener : public UObject
{
	GENERATED_BODY()

public:
	int32 CallCount = 0;

	UFUNCTION()
	void HandleEnemyAttackExpired();
};
