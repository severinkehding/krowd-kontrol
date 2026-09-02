#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "IntroFlythroughDirector.generated.h"

class UCameraComponent;

// Directs the game's intro flythrough (operator scene brief, 2026-09-02): on
// BeginPlay it takes over the player's view, eases a camera along a world-space
// waypoint path over DurationSeconds while looking at FocusPoint (the hangar
// mouth), starts a fade to black near the end, and finally travels to
// NextLevelMapName. No pawn is involved - pair with AIntroGameMode, which
// starts players as spectators.
//
// The real OpenLevel() call is gated on IsGameWorld() with a test-observable
// bTravelRequested seam, the exact pattern AKrowdKontrolPlayerController::
// RequestLevelRestart() documents (in-process map loads hang the Automation
// run - see issue #172's lineage).
UCLASS()
class KROWDKONTROL_API AIntroFlythroughDirector : public AActor
{
	GENERATED_BODY()

	friend class FKrowdKontrolIntroFlythroughDirectorTest;

public:
	AIntroFlythroughDirector();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Intro Flythrough")
	TObjectPtr<UCameraComponent> CameraComponent;

	// Open polyline the camera translates along (start -> hangar mouth).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Intro Flythrough")
	TArray<FVector> Waypoints;

	// The camera looks here the whole flight.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Intro Flythrough")
	FVector FocusPoint = FVector::ZeroVector;

	// Operator brief: the whole intro must fit in 7 seconds.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Intro Flythrough", meta = (ClampMin = "0.5"))
	float DurationSeconds = 7.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Intro Flythrough", meta = (ClampMin = "0.0"))
	float FadeStartSeconds = 6.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Intro Flythrough", meta = (ClampMin = "0.05"))
	float FadeDurationSeconds = 0.9f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Intro Flythrough")
	FName NextLevelMapName = TEXT("L_Level01");

private:
	// Eased position along the open polyline for normalised progress [0..1].
	FVector ComputePathPosition(float Alpha01) const;

	void ApplyProgress(float Alpha01);

	float ElapsedSeconds = 0.0f;
	bool bFadeStarted = false;
	// Test seam: set exactly once when the intro completes; the real map travel
	// additionally requires an actual game world.
	bool bTravelRequested = false;
};
