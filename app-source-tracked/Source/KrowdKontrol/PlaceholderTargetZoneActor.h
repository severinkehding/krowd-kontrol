// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PlaceholderTargetZoneActor.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;
class USceneComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UAbilityTargetingIndicatorComponent;

// Minimal placeholder-first actor (MISSION.md Quality Standards): a flattened mesh
// disc plus a point light standing in for a world-space target-zone beacon, before
// any real target-zone mechanic (detection radius, banking) exists. This is not the
// real ATargetZone that RoomEnemyBudgetController.h's comments already reserve for a
// future "OnActorBanked" integration - this class only carries the beacon visual. See
// issue #72 and PRD 13 REQ-6. No longer just a flattened disc: it also carries a tall
// column - mesh + point light crowning its top - so the beacon reads from farther away
// and across rooms (issue #190).
UCLASS()
class KROWDKONTROL_API APlaceholderTargetZoneActor : public AActor
{
	GENERATED_BODY()

public:
	APlaceholderTargetZoneActor();

	// PR #199 fix-pass 1 review: exposed as a member (rather than a constructor-local)
	// so PostInitializeComponents() can reference it to self-heal actors placed in a
	// level before this class had a dedicated root - see that function's comment.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Target Zone")
	TObjectPtr<USceneComponent> TargetZoneRootComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Target Zone")
	TObjectPtr<UStaticMeshComponent> BeaconMeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Target Zone")
	TObjectPtr<UStaticMeshComponent> BeaconColumnMeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Target Zone")
	TObjectPtr<UPointLightComponent> BeaconLightComponent;

	// Taller than ARoomActor::RoomWallHeight (300.f, RoomActor.h:59) so the column
	// physically pokes above a room's walls, making the beacon visible from an
	// adjacent room or across open floor (issue #190).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Target Zone")
	float BeaconColumnHeight = 400.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Target Zone")
	float BeaconBaselineIntensity = 5000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Target Zone")
	float BeaconIntensifiedIntensity = 15000.0f;

	// Raises BeaconLightComponent's intensity to BeaconIntensifiedIntensity. Called
	// externally by UFirstStunBeaconComponent (issue #29) on the first successful
	// Stun cast of a session - mirrors AEnemyBase::ReceiveControl()'s "called
	// externally by the cast component" idiom (see EnemyBase.h:98-99).
	void IntensifyBeacon();

	// Overrides this beacon's default placeholder-green tint with a type-keyed pen's
	// chain colour (docs/prd-colour-coded-herding.md REQ-3, issue #317). Called only by
	// ARoomActor::EnsureBankingZonesWired() for zones with bAcceptAnyEnemyType == false
	// - never called for any-type zones, which must keep this class's constructor
	// default green (MISSION.md Hard Invariant 3: an any-type marker must never wear
	// one of the five reserved colours). Idempotent/safe to call more than once with the
	// same or a different colour - lazily creates ChainColourMaterialInstance once, then
	// just re-applies the parameter, same "safe to call repeatedly" contract every other
	// lazy-init method in this module documents (EnsureBeaconHierarchy,
	// InitializeMarkerVisual, InitializeIndicatorVisual).
	UFUNCTION(BlueprintCallable, Category = "Target Zone")
	void ApplyChainColour(FLinearColor Colour);

	// Reflected state (mirrors UControlledDurationIndicatorComponent::CurrentColour) -
	// the Automation test, and any future Unreal MCP-driven E2E holdout (which per this
	// project's established convention can only read reflected UPROPERTY state, not GPU
	// pixels), asserts against this directly rather than any rendered output. Stays at
	// the default (Black) for any-type zones, which never call ApplyChainColour().
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Target Zone")
	FLinearColor CurrentChainColour = FLinearColor::Black;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Target Zone")
	TObjectPtr<UMaterialInstanceDynamic> ChainColourMaterialInstance;

	// Persistent ground-ring indicator of the co-located ATargetZone's real banking
	// extent (issue #365). Reuses UAbilityTargetingIndicatorComponent (issue #264)
	// verbatim - same mesh/material technique as the 5 abilities' cast-preview
	// circles - rather than new decal/mesh-rendering plumbing.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Target Zone")
	TObjectPtr<UAbilityTargetingIndicatorComponent> BankingRadiusIndicatorComponent;

	// Shows the banking-radius ring at RadiusUnits/Colour via BankingRadiusIndicatorComponent.
	// Called externally by ARoomActor::EnsureBankingZonesWired() - same "called externally"
	// idiom as ApplyChainColour() above. Never Hide()-den anywhere: the ring must stay
	// visible during normal play, not gated behind a debug-view toggle (issue #365).
	UFUNCTION(BlueprintCallable, Category = "Target Zone")
	void ShowBankingRadiusIndicator(float RadiusUnits, FLinearColor Colour);

protected:
	// Self-heals actors placed in a level before this PR's component-hierarchy change
	// (root swapped from BeaconMeshComponent to TargetZoneRootComponent; light moved
	// from BeaconMeshComponent to BeaconColumnMeshComponent). See .cpp for why the
	// constructor alone doesn't fix already-placed instances. Two entry points on
	// purpose: PostInitializeComponents covers game-time init (PIE/packaged), but it
	// never runs for actors sitting in an editor world - which is exactly where the
	// pass-2 E2E holdout inspected them and where a Save writes serialized state - so
	// PostLoad applies the same heal on deserialization everywhere, letting an editor
	// re-save persist the corrected hierarchy permanently.
	virtual void PostInitializeComponents() override;
	virtual void PostLoad() override;

private:
	// Shared heal body for the two hooks above. Registration-aware: PostLoad runs
	// before components register (SetupAttachment territory), PostInitializeComponents
	// after (AttachToComponent territory).
	void EnsureBeaconHierarchy();
};
