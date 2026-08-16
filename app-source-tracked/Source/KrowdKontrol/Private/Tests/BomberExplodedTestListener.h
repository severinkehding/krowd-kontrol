#pragma once
#include "CoreMinimal.h"
#include "BomberExplodedTestListener.generated.h"

// Test-only listener for ABomberEnemy::OnBomberExploded (issue #15). Dynamic
// multicast delegates only bind UFUNCTIONs via AddDynamic, not a capturing lambda -
// mirrors USniperShotFiredTestListener. Used only by KrowdKontrolBomberEnemyTest.cpp.
UCLASS()
class UBomberExplodedTestListener : public UObject
{
	GENERATED_BODY()

public:
	int32 CallCount = 0;

	UFUNCTION()
	void HandleBomberExploded();
};
