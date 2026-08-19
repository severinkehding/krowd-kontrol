#pragma once
#include "CoreMinimal.h"
#include "PunishmentTriggeredTestListener.generated.h"

// Test-only listener for UPunishmentManagerComponent::OnPunishmentTriggered
// (issue #177). Dynamic multicast delegates only bind UFUNCTIONs via
// AddDynamic, not a capturing lambda - mirrors UBomberExplodedTestListener.
// Used only by KrowdKontrolPunishmentManagerComponentTest.cpp.
UCLASS()
class UPunishmentTriggeredTestListener : public UObject
{
	GENERATED_BODY()

public:
	int32 CallCount = 0;

	UFUNCTION()
	void HandlePunishmentTriggered();
};
