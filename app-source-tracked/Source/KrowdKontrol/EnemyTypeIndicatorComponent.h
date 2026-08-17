#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "EnemyType.h"
#include "EnemyTypeIndicatorComponent.generated.h"

class UTextRenderComponent;

// Colourblind-safe non-colour marker (PRD 13 REQ-7, issue #77): a floating
// world-space text label above an owning enemy actor, showing its own codename
// (e.g. "SN-1PR") derived from EEnemyType's own UMETA(DisplayName) - the single
// source of truth, not a second label table to keep in sync. Complements (never
// replaces) the existing colour coding each concrete enemy already carries; see
// ReservedGameplayColours.h for MISSION.md Hard Invariant 3's locked 5-colour
// channel this marker's own colour must never collide with.
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class KROWDKONTROL_API UEnemyTypeIndicatorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UEnemyTypeIndicatorComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Type Indicator")
	EEnemyType EnemyType = EEnemyType::RU_NNR;

	UFUNCTION(BlueprintPure, Category = "Enemy Type Indicator")
	FText GetMarkerText() const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy Type Indicator")
	TObjectPtr<UTextRenderComponent> MarkerTextComponent;

	// Lazily creates and attaches MarkerTextComponent. Called automatically from
	// BeginPlay(); exposed publicly (and made idempotent) so callers - including the
	// Automation Framework test - can drive it deterministically without needing a
	// full actor BeginPlay lifecycle, same shape as UStationPowerUpComponent's
	// InitializeSequence().
	UFUNCTION(BlueprintCallable, Category = "Enemy Type Indicator")
	void InitializeMarkerVisual();

protected:
	virtual void BeginPlay() override;

private:
	static constexpr float MarkerHeightOffset = 150.0f;

	bool bHasInitializedMarkerVisual = false;
};
