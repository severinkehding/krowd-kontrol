#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AbilitySlot.h"
#include "QuestTrackerWidget.generated.h"

class UBorder;
class UTextBlock;
class AActor;
class UWaveSpawnerComponent;
class UAbilityUnlockComponent;
class ARoomActor;
class ADoorConnectorActor;

// 8-way world-space compass bucket for REQ-3's directional cue (PRD "Mission
// Briefing & Live Quest Tracker", issue #250) - world +X is North, +Y is East
// (an arbitrary but fixed convention; nothing else in this codebase defines
// "north" yet). None means no cue is currently shown - either no resolvable
// objective location exists, no player pawn is resolvable
// (UGameplayStatics::GetPlayerPawn returning null), or the target is
// within ComputeCompassDirection()'s own dead zone of the player.
UENUM(BlueprintType)
enum class EQuestDirection8 : uint8
{
	None,
	North,
	NorthEast,
	East,
	SouthEast,
	South,
	SouthWest,
	West,
	NorthWest
};

// Persistent quest tracker HUD widget (PRD "Mission Briefing & Live Quest Tracker"
// REQ-2, issue #247): a small, top-right-corner-anchored panel showing
// "Robots penned: X/Y", the level's live banked-enemy progress. Top-right is the one
// HUD corner not already claimed by UEnergyMeterWidget (top-left, PRD 13 REQ-1),
// UAbilityCooldownTrayWidget (bottom-right, issue #66), or UOnScreenPromptWidget
// (top-center, issue #34). X updates event-driven only, from every live
// ATargetZone::OnActorBanked broadcast in the level (never per-frame polling); Y comes
// from a TActorIterator<AEnemyBase> sweep, re-taken whenever ULevelLifecycleSubsystem::
// OnLevelBegin fires and again whenever any live UWaveSpawnerComponent's OnWaveSpawned
// does (e.g. ARootSurgeBoss's, whose adds spawn 3-9s after level begin and would
// otherwise never be counted into the denominator) - not from NativeOnInitialized()/
// CreateHUDWidgets() timing, because ATargetZone instances aren't guaranteed to exist
// yet at that point (ARoomActor::BeginPlay()'s EnsureBankingZonesWired() is what
// spawns/wires them). This widget's own creation isn't guaranteed to precede
// OnLevelBegin's broadcast either (AKrowdKontrolPlayerController::CreateHUDWidgets()
// racing ULevelLifecycleSubsystem::OnWorldBeginPlay() - the same hazard
// UAbilityUnlockLevelSubsystem/OnScreenPromptWidget already hit, issue #235), so
// BindToLevelLifecycle() self-invokes HandleLevelBegin() if
// ULevelLifecycleSubsystem::HasLevelBegun() is already true at bind time, since
// OnLevelBegin never re-fires. This widget builds its own UI tree in C++ (no Widget
// Blueprint asset), mirroring UEnergyMeterWidget/UOnScreenPromptWidget. Issue #249
// added a second line naming the ability that best counters the enemies still alive
// in the level, and issue #248 added a third line showing the state of the room the
// player is currently working through (chain order by X, "DOOR OPEN" once every room
// clears) - see RefreshRoomStateDisplay()'s own comment for the full contract.
UCLASS()
class KROWDKONTROL_API UQuestTrackerWidget : public UUserWidget
{
	GENERATED_BODY()

	friend class FKrowdKontrolQuestTrackerWidgetTest;
	friend class FKrowdKontrolQuestTrackerWidgetRoomStateTest;

public:
	// Read-only accessors for what's currently displayed/tracked - used by the
	// Automation Framework test, also generally useful to anything that wants to
	// confirm this widget's state without re-deriving formatting.
	UFUNCTION(BlueprintPure, Category = "Quest Tracker")
	FText GetQuestTrackerDisplayText() const;

	UFUNCTION(BlueprintPure, Category = "Quest Tracker")
	int32 GetBankedCount() const { return BankedCount; }

	UFUNCTION(BlueprintPure, Category = "Quest Tracker")
	int32 GetTotalEnemyCount() const { return TotalEnemyCount; }

	UFUNCTION(BlueprintPure, Category = "Quest Tracker")
	FText GetSuggestedAbilityDisplayText() const;

	UFUNCTION(BlueprintPure, Category = "Quest Tracker")
	FLinearColor GetSuggestedAbilityTextColour() const;

	UFUNCTION(BlueprintPure, Category = "Quest Tracker")
	FText GetRoomStateDisplayText() const;

	// Read-only accessor for REQ-3's computed compass bucket - lets the
	// Automation Framework test assert the exact enum value rather than parsing
	// the rendered glyph character out of GetRoomStateDisplayText().
	UFUNCTION(BlueprintPure, Category = "Quest Tracker")
	EQuestDirection8 GetObjectiveDirection() const { return CurrentObjectiveDirection; }

	// Production wiring (mirrors AbilityCooldownTrayWidget::BindAbilityUnlockComponent):
	// seeds the suggested-ability line from the pawn's current unlock state and keeps
	// it live via OnAbilityUnlocked. AKrowdKontrolPlayerController::WireWidgetsToPawn()
	// calls this once per (re)possession; the Automation test calls it directly.
	UFUNCTION(BlueprintCallable, Category = "Quest Tracker")
	void BindAbilityUnlockComponent(UAbilityUnlockComponent* UnlockComponent);

protected:
	// Fires synchronously from CreateWidget(), before any Slate/viewport realization -
	// matters for the -nullrhi headless Automation run this project's tests use (see
	// UPostRunSummaryWidget's NativeOnInitialized() precedent, issue #74).
	virtual void NativeOnInitialized() override;

	// Safety net mirroring UEnergyMeterWidget::Initialize() - guarantees
	// WidgetTree->RootWidget exists before this widget's first TakeWidget() call even
	// when CreateWidget() is called without an owning player/controller (exactly how
	// the Automation Framework test constructs this widget).
	virtual bool Initialize() override;

	// Deliberately no NativeTick() override - this widget has no cosmetic per-frame
	// state (unlike UEnergyMeterWidget's damage-flash timer). 100% event-driven,
	// matching issue #247's explicit "never per-frame polling" requirement.

private:
	void BuildWidgetTree();

	// Builds the widget tree exactly once, regardless of which of
	// NativeOnInitialized()/Initialize() fires first - mirrors
	// UEnergyMeterWidget::EnsureWidgetTreeBuilt().
	void EnsureWidgetTreeBuilt();

	// Resolves this world's ULevelLifecycleSubsystem and subscribes to its
	// OnLevelBegin - mirrors UCrowdMasterySubsystem::Initialize()'s identical
	// self-subscribe idiom, adapted from a subsystem's Initialize() to a widget's
	// NativeOnInitialized(). AddUniqueDynamic makes this safe to call more than once.
	// Also self-invokes HandleLevelBegin() immediately if
	// ULevelLifecycleSubsystem::HasLevelBegun() is already true at bind time - this
	// widget's creation (from AKrowdKontrolPlayerController::CreateHUDWidgets(), an
	// actor's BeginPlay()) isn't guaranteed to precede the subsystem's own
	// OnWorldBeginPlay(), and OnLevelBegin never re-fires (see HasLevelBegun()'s own
	// comment for the identical, already-documented race this mirrors).
	void BindToLevelLifecycle();

	// Bound to ULevelLifecycleSubsystem::OnLevelBegin (and self-invoked directly by
	// BindToLevelLifecycle() for a late-subscribe catch-up - see its comment). Captures
	// TotalEnemyCount via RecountTotalEnemies() and binds HandleActorBanked to every
	// live ATargetZone's OnActorBanked, and HandleWaveSpawned to every live
	// UWaveSpawnerComponent's OnWaveSpawned. Safe to call more than once - every bind
	// below uses AddUniqueDynamic and RecountTotalEnemies() is a plain re-sweep.
	UFUNCTION()
	void HandleLevelBegin(FName MapName);

	// Bound to each discovered ATargetZone's OnActorBanked via AddUniqueDynamic -
	// must be a real UFUNCTION() for that to compile, same as any BlueprintAssignable
	// delegate binding. Matches FOnActorBanked's signature exactly (TargetZone.h:11).
	UFUNCTION()
	void HandleActorBanked(AActor* BankedActor);

	// Bound to each discovered UWaveSpawnerComponent's OnWaveSpawned via
	// AddUniqueDynamic - a boss's spawner (e.g. ARootSurgeBoss's) adds new bankable
	// AEnemyBase actors well after HandleLevelBegin()'s initial sweep, so TotalEnemyCount
	// must be refreshed each time a wave lands or the denominator undercounts (visible
	// as an impossible "banked > total" display once those adds get banked). Re-sweeps
	// via RecountTotalEnemies() rather than incrementing by the wave's configured Count,
	// since ATargetZone never destroys a banked actor (TargetZone.h) so a fresh sweep is
	// always safe and needs no per-spawner bookkeeping. WaveIndex is unused - this
	// widget only cares that a wave landed, not which one.
	UFUNCTION()
	void HandleWaveSpawned(int32 WaveIndex);

	// Shared by HandleLevelBegin()/HandleWaveSpawned(): a fresh TActorIterator<AEnemyBase>
	// sweep of the world, replacing TotalEnemyCount. Safe to call repeatedly/at any time.
	void RecountTotalEnemies();

	// Re-renders BankedCountText from the current BankedCount/TotalEnemyCount.
	void RefreshDisplay();

	// Bound to UAbilityUnlockComponent::OnAbilityUnlocked via BindAbilityUnlockComponent().
	UFUNCTION()
	void HandleAbilityUnlocked(EAbilitySlot Ability);

	// Bound to each discovered ARoomActor's OnRoomClearedStateChanged via
	// AddUniqueDynamic - must be a real UFUNCTION() for that to compile, same as every
	// other handler in this class.
	UFUNCTION()
	void HandleRoomClearedStateChanged();

	// Fresh TActorIterator<AEnemyBase> sweep (mirrors RecountTotalEnemies()'s own
	// "always safe to call repeatedly" shape) filtered to non-Banked enemies, matched
	// against AbilityData::GetAll()'s CounteredEnemyType via the same reverse-lookup
	// loop ARoomActor::EnsureBankingZonesWired() already uses. Returns EAbilitySlot::
	// Stun (bIsColourNeutral's only true case) whenever no remaining enemy's
	// counter is unlocked yet, or no UAbilityUnlockComponent is bound - the universal
	// fallback per this issue's AC. Level-wide, not room-scoped - see plan.md's
	// Alternatives Rejected for why.
	EAbilitySlot ComputeSuggestedAbility() const;

	// Re-renders SuggestedAbilityText from ComputeSuggestedAbility() - the second
	// line's counterpart to RefreshDisplay(). Called from every event that could
	// change either the remaining-enemy-type set or unlock state.
	void RefreshSuggestedAbilityDisplay();

	// Re-renders RoomStateText - the third line's counterpart to RefreshDisplay()/
	// RefreshSuggestedAbilityDisplay(). Fresh TActorIterator<ARoomActor> sweep every
	// call, safe to call repeatedly.
	void RefreshRoomStateDisplay();

	// Resolves the world-space location REQ-3's directional cue should point
	// toward, given the already-chain-sorted Rooms and the FocusIndex
	// RefreshRoomStateDisplay() just computed. Returns false (no cue) when
	// nothing resolvable exists. See this class's Design Decisions in plan.md
	// for the full rationale (mirrored briefly here): the focus room's own pen —
	// the first FRoomTargetZone (array order, not distance-based) whose
	// EnemyType still has a remaining enemy — for the common "Room N - K left"
	// case, or the last room's forward door marker once every room is cleared.
	bool ResolveObjectiveDirectionTarget(const TArray<ARoomActor*>& Rooms, int32 FocusIndex, FVector& OutTargetLocation) const;

	// World-space (not player-facing-relative) 8-way compass bucket from
	// FromLocation to ToLocation, using this codebase's flat top-down X/Y
	// convention (matches UAbilityCastComponent::IsPointInCone's own
	// SizeSquared2D usage) - world +X is North, +Y is East.
	// Returns None when ToLocation is within DirectionDeadZoneRadiusUnits of
	// FromLocation (mirrors AbilityCastComponent's own dead-zone constant).
	static EQuestDirection8 ComputeCompassDirection(const FVector& FromLocation, const FVector& ToLocation);

	// Single-character Unicode arrow glyph for a computed EQuestDirection8 -
	// empty FText for None (no cue to render).
	static FText GetDirectionGlyph(EQuestDirection8 Direction);

	UPROPERTY()
	TObjectPtr<UBorder> ChromeBorder;

	UPROPERTY()
	TObjectPtr<UTextBlock> BankedCountText;

	UPROPERTY()
	TObjectPtr<UTextBlock> SuggestedAbilityText;

	UPROPERTY()
	TObjectPtr<UTextBlock> RoomStateText;

	// Weak - this widget does not own the pawn's unlock component's lifetime. Mirrors
	// AbilityCooldownTrayWidget::BoundLockoutComponent's identical weak-ref shape.
	UPROPERTY()
	TWeakObjectPtr<UAbilityUnlockComponent> BoundUnlockComponent;

	UPROPERTY()
	int32 BankedCount = 0;

	UPROPERTY()
	int32 TotalEnemyCount = 0;

	// Actors already counted into BankedCount - ATargetZone::OnActorBanked fires once
	// per overlapping *component*, not once per actor (see TargetZone.cpp's own
	// "KNOWN GAP" comment), so this widget dedups at the point it turns the raw
	// broadcast into a number the player sees, rather than assuming the source is
	// already actor-unique.
	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> BankedActors;

	UPROPERTY()
	EQuestDirection8 CurrentObjectiveDirection = EQuestDirection8::None;

	// Fixed pixel footprint from the viewport's top-right corner. 160px + 24px margin
	// = 184px footprint, which is ~14.4% of a 1280px-wide viewport (this project's
	// documented minimum target resolution - see the resolution-safety test) - inside
	// issue #247's explicit "no more than roughly 15% of screen width" envelope, with
	// the 1280px case being the binding (smallest, tightest) one; every wider
	// resolution is strictly safer for the same fixed-size box. Recompute this comment
	// if either constant below changes.
	static constexpr float TrackerMarginPx = 24.0f;
	static constexpr float TrackerWidthPx = 160.0f;
	static constexpr float TrackerHeightPx = 80.0f;
};
