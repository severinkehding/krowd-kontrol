#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ControlledDurationIndicatorComponent.generated.h"

class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;

// World-space depleting bar shown above an AEnemyBase-derived enemy while it is
// Controlled (issue #225, PRD docs/prd-enemy-effect-indicator.md REQ-1/REQ-3). Mirrors
// UAbilityTargetingIndicatorComponent's mesh+MID+idempotent-init+reflected-state
// structure, but is a distinct, narrowly-scoped component rather than a reuse of it -
// that component is a documented ability-cast targeting/range renderer with a
// one-instance-per-Owner limit, and repurposing its RangeUnits to mean "time
// remaining" would be a semantic hijack of its contract.
//
// Base-class-owned (CreateDefaultSubobject'd once in AEnemyBase::AEnemyBase(), not
// per-subclass) since every field it reads (RemainingControlledSeconds/
// TotalControlledSeconds, via GetRemainingControlledSeconds()/
// GetTotalControlledSeconds() only - never the private field directly, per this
// issue's own instruction) is already base-class-owned with zero per-subclass
// variation. (Fill colour is base-class-sourced too, but arrives as a Show()
// parameter rather than being read directly by this component.)
//
// Event-driven, not Tick()-driven: AEnemyBase calls Show()/Hide()/RefreshFillFraction()
// directly at the exact points its own state machine already changes CurrentState
// around Controlled (ReceiveControl()/TickControlledDuration()/TransitionToBanked()),
// so the indicator's reflected state is correct immediately after those calls return -
// no world tick pump required, matching every existing EnemyBase-family test's
// no-World, direct-call idiom.
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class KROWDKONTROL_API UControlledDurationIndicatorComponent : public UActorComponent
{
	GENERATED_BODY()

	friend class FKrowdKontrolControlledDurationIndicatorComponentTest;

public:
	UControlledDurationIndicatorComponent();

	// Lazily creates and attaches FillMeshComponent + its dynamic material instance.
	// Called automatically from BeginPlay(); exposed publicly (and idempotent) so
	// Show() can call it deterministically without a full actor BeginPlay lifecycle -
	// mirrors UAbilityTargetingIndicatorComponent::InitializeIndicatorVisual().
	//
	// No-ops (retryable) if Owner->GetWorld() is null, in addition to the usual no-root-
	// component check: UActorComponent::RegisterComponent() requires a non-null
	// GetOwner()->GetWorld() internally, and Show() (which calls this) is reachable from
	// AEnemyBase::ReceiveControl(), which dozens of existing tests call on a bare
	// NewObject<...>() actor with no UWorld at all. Skipping this guard would crash
	// every one of those tests. Reflected state (bIsVisible/FillFraction/CurrentColour)
	// is still set unconditionally by Show()/Hide()/RefreshFillFraction() regardless of
	// whether the mesh/material were actually created.
	UFUNCTION(BlueprintCallable, Category = "Controlled Duration Indicator")
	void InitializeIndicatorVisual();

	// Sets bIsVisible=true, applies Colour, calls RefreshFillFraction() (reads
	// GetRemainingControlledSeconds()/GetTotalControlledSeconds() off the Owner - right
	// after AEnemyBase::ReceiveControl() sets Remaining==Total, this yields exactly
	// 1.0). Idempotent-safe to call InitializeIndicatorVisual() internally first, same
	// shape as UAbilityTargetingIndicatorComponent::Show().
	UFUNCTION(BlueprintCallable, Category = "Controlled Duration Indicator")
	void Show(FLinearColor Colour, bool bInIsColourMatchBonused = false);

	UFUNCTION(BlueprintCallable, Category = "Controlled Duration Indicator")
	void Hide();

	// Casts GetOwner() to AEnemyBase and recomputes FillFraction from
	// GetRemainingControlledSeconds()/GetTotalControlledSeconds() - per the issue's
	// explicit instruction, NEVER reads AEnemyBase's private RemainingControlledSeconds
	// field directly, even though this module could grant a friend. True no-op if
	// GetOwner() is not an AEnemyBase. If GetTotalControlledSeconds() is <= 0 (guards
	// the 0/0 division GetTotalControlledSeconds()'s own doc comment warns callers
	// about), FillFraction is reset to 0.0f (not left at its previous value) and the
	// visual is re-applied to match.
	UFUNCTION(BlueprintCallable, Category = "Controlled Duration Indicator")
	void RefreshFillFraction();

	// Reflected state - the Automation test (and any future Unreal MCP-driven E2E
	// holdout, which per project convention can only read reflected UPROPERTY state,
	// not GPU pixels) asserts against these directly rather than any rendered output.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Controlled Duration Indicator")
	bool bIsVisible = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Controlled Duration Indicator")
	float FillFraction = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Controlled Duration Indicator")
	FLinearColor CurrentColour = FLinearColor::Black;

	// True while this Controlled application's duration came from a per-enemy
	// GetControlledDurationOverrideSeconds() colour-match bonus (issue #65) rather than
	// AbilityData::BaseDurationSeconds unmodified - issue #357's legibility fix. Read-only
	// signal, set only by Show(); never derived here (this component never re-derives the
	// colour-matching rule itself, per its own "reads through Owner accessors only" contract
	// above). Does NOT affect CurrentColour, which stays the pure ability colour regardless
	// (see KrowdKontrolControlledDurationIndicatorComponentTest.cpp case g4) - this is a
	// separate, additive visual channel (bar thickness - see ApplyVisualFillFraction()).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Controlled Duration Indicator")
	bool bIsColourMatchBonused = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Controlled Duration Indicator")
	TObjectPtr<UStaticMeshComponent> FillMeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Controlled Duration Indicator")
	TObjectPtr<UMaterialInstanceDynamic> FillMaterialInstance;

protected:
	virtual void BeginPlay() override;

private:
	void ApplyVisualFillFraction();

	// Distinct from UEnemyTypeIndicatorComponent::MarkerHeightOffset's 150.0f (REQ-3:
	// this bar and the type marker must read as siblings, not stacked at the same
	// offset).
	static constexpr float BarHeightOffset = 190.0f;

	// Target world size of the plane at FillFraction == 1.0, in the "100uu at scale
	// 1.0" convention (see .cpp's ApplyVisualFillFraction()).
	static constexpr float BarWidthUnits = 120.0f;
	static constexpr float BarDepthUnits = 24.0f;

	// issue #357: a colour-match-bonused application renders as a visibly thicker bar than
	// BarDepthUnits - the only visual difference from a base-duration application (colour
	// and width behave identically either way). Chosen well above BarDepthUnits (24.0f) so
	// the difference reads at gameplay camera distance without a second material/texture -
	// same "placeholder-first, geometry over shaders" precedent InitializeIndicatorVisual()
	// already sets for this whole component.
	static constexpr float BarDepthUnitsBonused = 40.0f;

	bool bHasInitializedIndicatorVisual = false;
};
