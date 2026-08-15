#pragma once

#include "CoreMinimal.h"
#include "RoomClearedTestListener.generated.h"

// Test-only listener for URoomEnemyBudgetController::OnRoomCleared. Dynamic
// multicast delegates (DECLARE_DYNAMIC_MULTICAST_DELEGATE) only bind to UFUNCTIONs
// via AddDynamic - no AddLambda - so counting broadcasts in
// KrowdKontrolRoomEnemyBudgetControllerTest.cpp needs this rather than a capturing
// lambda. Used only by that test.
UCLASS()
class URoomClearedTestListener : public UObject
{
	GENERATED_BODY()

public:
	int32 CallCount = 0;

	UFUNCTION()
	void HandleRoomCleared();
};
