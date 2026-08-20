#pragma once
#include "CoreMinimal.h"
#include "LevelFailedTestListener.generated.h"

// Test-only listener for ULevelFailComponent::OnLevelFailed (issue #171).
// Dynamic multicast delegates only bind UFUNCTIONs via AddDynamic, not a
// capturing lambda - mirrors UPunishmentTriggeredTestListener. Used only by
// KrowdKontrolLevelFailedTest.cpp.
UCLASS()
class ULevelFailedTestListener : public UObject
{
	GENERATED_BODY()

public:
	int32 CallCount = 0;

	UFUNCTION()
	void HandleLevelFailed();
};
