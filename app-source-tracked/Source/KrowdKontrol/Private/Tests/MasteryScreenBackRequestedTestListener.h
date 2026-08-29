#pragma once
#include "CoreMinimal.h"
#include "MasteryScreenBackRequestedTestListener.generated.h"

// Test-only listener for UMasteryScreenWidget::OnBackRequested (issue #373). Dynamic
// multicast delegates (DECLARE_DYNAMIC_MULTICAST_DELEGATE) only bind to UFUNCTIONs
// via AddDynamic - no AddLambda - so counting broadcasts in
// KrowdKontrolMasteryScreenWidgetTest.cpp/KrowdKontrolMainMenuMasteryScreenTest.cpp
// needs this rather than a capturing lambda. Mirrors UDrainRayFiredTestListener.
UCLASS()
class UMasteryScreenBackRequestedTestListener : public UObject
{
	GENERATED_BODY()

public:
	int32 CallCount = 0;

	UFUNCTION()
	void HandleBackRequested();
};
