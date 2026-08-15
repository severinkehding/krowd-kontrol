#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "StationPowerUpComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnLightEnabled, int32, LightIndex, AActor*, LightActor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPowerUpSequenceComplete);

// Reusable Opening Scene power-up sequence (PRD 07 REQ-1): as the player acts
// (moves/interacts), lights in OrderedLights enable one at a time, in order - never
// a Sequencer/Matinee cutscene, never a camera lock, never blocking player input.
// See issue #60.
//
// This component deliberately does NOT wire itself to any trigger volume or input
// event - mirroring RoomEnemyBudgetController.h's precedent (issue #82), callers (a
// future trigger-volume actor, or player-interaction code) invoke
// NotifyPowerUpStageTriggered() directly. Authoring the actual Opening Scene level
// layout - placing OrderedLights, wiring real trigger volumes to this method - is
// level-design content and explicitly out of scope for this issue; see its Notes.
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class KROWDKONTROL_API UStationPowerUpComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UStationPowerUpComponent();

	// The power-up order. Level-instance-only (EditInstanceOnly, not
	// EditDefaultsOnly) because these are references to placed level actors - a
	// Blueprint class default has no level to reference yet.
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Station Power Up")
	TArray<TObjectPtr<AActor>> OrderedLights;

	// Fires each time NotifyPowerUpStageTriggered() enables a light, with its index
	// in OrderedLights and the actor itself.
	UPROPERTY(BlueprintAssignable, Category = "Station Power Up")
	FOnLightEnabled OnLightEnabled;

	// Fires exactly once, when every entry in OrderedLights has been enabled. Never
	// fires for an empty OrderedLights - see InitializeSequence()'s warning for that
	// misconfiguration case instead.
	UPROPERTY(BlueprintAssignable, Category = "Station Power Up")
	FOnPowerUpSequenceComplete OnPowerUpSequenceComplete;

	// Hides every entry in OrderedLights and resets the sequence to its start. Called
	// automatically from BeginPlay(); exposed publicly (and made idempotent) so
	// callers - including the Automation Framework test - can drive it
	// deterministically without needing a full actor BeginPlay lifecycle. Logs a
	// warning, and otherwise no-ops on the empty case, if OrderedLights is empty -
	// that configuration can never progress or complete.
	UFUNCTION(BlueprintCallable, Category = "Station Power Up")
	void InitializeSequence();

	// The sole entry point that advances the sequence. Called by whatever
	// player-triggered event a level author wires up (a trigger volume overlap,
	// player-driven interaction, etc.) - this component has no opinion on what that
	// event is, and never touches player input APIs itself. Enables the next light in
	// OrderedLights, in order; no-ops once the sequence is already complete (or if
	// OrderedLights is empty).
	UFUNCTION(BlueprintCallable, Category = "Station Power Up")
	void NotifyPowerUpStageTriggered();

	int32 GetEnabledLightCount() const { return NextLightIndex; }
	bool IsSequenceComplete() const { return bSequenceCompleteFired; }

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY()
	int32 NextLightIndex = 0;

	bool bHasInitializedSequence = false;
	bool bSequenceCompleteFired = false;
};
