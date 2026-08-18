#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TargetZone.generated.h"

class UBoxComponent;
class UPrimitiveComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnActorBanked, AActor*, BankedActor);

// Detects a controlled, colour-matched IHerdable (issue #79) actor physically
// arriving in this zone and broadcasts OnActorBanked - the missing spatial trigger
// for PRD 01 loop step 5 ("Bank"). Detection and announcement only: this class never
// destroys, pools, or otherwise mutates the overlapping actor, and never wires itself
// to URoomEnemyBudgetController::NotifyEnemyBanked() - that integration is future,
// out-of-scope work (see RoomEnemyBudgetController.h's own comments). Distinct from
// APlaceholderTargetZoneActor (issue #72), which only carries the beacon visual.
// See issue #80.
UCLASS()
class KROWDKONTROL_API ATargetZone : public AActor
{
	GENERATED_BODY()

public:
	ATargetZone();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Target Zone")
	TObjectPtr<UBoxComponent> ZoneCollisionComponent;

	// Plain FName, not a UENUM over MISSION.md's five locked colours - matches
	// IHerdable::GetHerdColourTag()'s own deferred-scope decision (issue #79).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Target Zone")
	FName ZoneColourTag = NAME_None;

	// True when this zone's approach is deliberately routed around an obstacle for
	// moment-to-moment difficulty variation (PRD 01 REQ-5, P1). Level-authoring
	// metadata only - no AI/pathfinding logic reads or enforces this flag yet; see
	// this class's own issue (#83) for the explicit scope limit.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Target Zone")
	bool bRequiresRouting = false;

	// Per-placed-instance reference to the blocking volume/obstacle actor between this
	// zone and its expected approach, not a Blueprint-class-default value - mirrors
	// ADoorConnectorActor::RoomA/RoomB's own EditInstanceOnly pattern
	// (DoorConnectorActor.h). Untyped AActor* rather than a narrower volume class, since
	// the obstacle can be any placed actor a level designer chooses. Only meaningful
	// when bRequiresRouting is true; the two fields are not cross-validated against each
	// other - metadata only, per issue #83's scope.
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Target Zone")
	TObjectPtr<AActor> RoutingObstacleActor;

	// Fires when a controlled, colour-matched IHerdable actor overlaps this zone.
	UPROPERTY(BlueprintAssignable, Category = "Target Zone")
	FOnActorBanked OnActorBanked;

private:
	// Bound in the constructor, not BeginPlay() - most of this module's Automation
	// Framework tests spawn actors into a UWorld that never runs World->BeginPlay()
	// (see KrowdKontrolRoomEnemyBudgetControllerTest.cpp), so a BeginPlay()-bound
	// dynamic delegate would silently never fire under those tests. This class's own
	// test (KrowdKontrolTargetZoneTest.cpp) is an exception - it does drive
	// World->SetBegunPlay(true) to get real overlap events, so BeginPlay() binding
	// would also have worked here - but constructor-binding is kept anyway, since it
	// stays correct even under tests that don't force play state, with no downside.
	UFUNCTION()
	void HandleZoneOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
};
