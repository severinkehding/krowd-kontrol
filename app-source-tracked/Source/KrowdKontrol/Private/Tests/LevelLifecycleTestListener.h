#pragma once
#include "CoreMinimal.h"
#include "LevelLifecycleTestListener.generated.h"

// Test-only listener for ULevelLifecycleSubsystem::OnLevelBegin/OnLevelClear/
// OnRunComplete (issues #169, #176). Dynamic multicast delegates only bind
// UFUNCTIONs via AddDynamic, not a capturing lambda - mirrors UWaveSpawnerTestListener
// (also one listener class serving multiple delegates off the same source). Used only
// by KrowdKontrolLevelLifecycleSubsystemTest.cpp.
UCLASS()
class ULevelLifecycleTestListener : public UObject
{
	GENERATED_BODY()

public:
	int32 LevelBeginCallCount = 0;
	FName LastLevelBeginMapName;
	int32 LevelClearCallCount = 0;
	int32 RunCompleteCallCount = 0;

	// Records each fired delegate's name in call order - lets tests assert relative
	// ordering (e.g. OnRunComplete firing after OnLevelClear), not just final counts.
	TArray<FString> CallOrder;

	UFUNCTION()
	void HandleLevelBegin(FName MapName);

	UFUNCTION()
	void HandleLevelClear();

	UFUNCTION()
	void HandleRunComplete();
};
