#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "OvercrowdDetectionComponent.h"
#include "OvercrowdAudioSubsystem.generated.h"

class USubmixEffectFilterPreset;

// Exactly 2 states, mirroring MusicSubsystem.h's EMusicState placement convention.
UENUM(BlueprintType)
enum class EOvercrowdAudioMuffleState : uint8
{
	Clear,
	Muffled
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnOvercrowdAudioMuffleStateChanged, EOvercrowdAudioMuffleState, NewState);

// Drives a mix-wide low-pass "muffle" treatment (issue #38, Audio & Music PRD REQ-3) on/off
// in sync with UOvercrowdDetectionComponent::OnPanicOverloadStateChanged (issue #16,
// Difficulty & Balance PRD Punishment 3) - the primary, hard-to-miss audio channel for the
// Overcrowd punishment state.
//
// Uses a runtime submix effect chain override (UAudioMixerBlueprintLibrary::
// SetSubmixEffectChainOverride/ClearSubmixEffectChainOverride) on the main output submix,
// never per-UAudioComponent low-pass filter calls - those are documented (two independent
// Epic forum threads) as unreliable at runtime, since Sound Attenuation settings are cached
// at sound-instance init and later SetLowPassFilterFrequency calls are frequently silently
// dropped. A submix effect processes the downmixed sum of everything routed through it, so
// it covers "the game's audio output" as a whole (UMusicSubsystem's tracks today, any future
// SFX automatically) without needing to track which UAudioComponents exist.
//
// Mirrors UMusicSubsystem's 2-state structure (state getter, BlueprintAssignable delegate,
// friend-class test access, no-op-on-same-state guard, flip-before-broadcast ordering) per
// this issue's own triage decision. Unlike UMusicSubsystem, Tick() is not the driver of the
// effect itself (which is delegate-driven) - it only exists to opportunistically bind to the
// player pawn's UOvercrowdDetectionComponent once it exists in the world, mirroring
// AEnemyBase::FindPlayerEnergyComponent()'s TActorIterator<APawn>+FindComponentByClass search,
// generalized here to a UWorldSubsystem so it isn't tied to any one enemy instance.
UCLASS()
class KROWDKONTROL_API UOvercrowdAudioSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

	// Grants the Automation Framework test no special access beyond what's already public
	// (GetMuffleState(), TryBindOvercrowdComponent()) - declared for parity with
	// MusicSubsystem/OvercrowdDetectionComponent's friend-test convention in case a future
	// change needs private access; currently unused by the test itself.
	friend class FKrowdKontrolOvercrowdAudioSubsystemTest;

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	EOvercrowdAudioMuffleState GetMuffleState() const { return CurrentMuffleState; }

	// Fires every time CurrentMuffleState actually changes (never on a no-op).
	UPROPERTY(BlueprintAssignable, Category = "Overcrowd Audio")
	FOnOvercrowdAudioMuffleStateChanged OnOvercrowdAudioMuffleStateChanged;

	// Searches the world's pawns for the first UOvercrowdDetectionComponent and binds
	// HandlePanicOverloadStateChanged to its OnPanicOverloadStateChanged. Idempotent - a
	// no-op returning true if already bound. Called automatically from Tick() every frame
	// until it succeeds; exposed publicly (mirroring MusicSubsystem::RefreshMusicState()'s
	// rationale) so the Automation Framework test can drive it deterministically without a
	// real tick loop.
	UFUNCTION(BlueprintCallable, Category = "Overcrowd Audio")
	bool TryBindOvercrowdComponent();

	// Seconds SetSubmixEffectChainOverride/ClearSubmixEffectChainOverride cross-fade over.
	// Placeholder default, not a locked design value - same rationale
	// AbilityCooldownComponent::DefaultAbilityCooldownSeconds's comment documents.
	UPROPERTY(EditDefaultsOnly, Category = "Overcrowd Audio", meta = (ClampMin = "0.0"))
	float MuffleFadeTimeSeconds = 0.5f;

	// Low-pass cutoff frequency, in Hz, applied to the main submix while Muffled. Placeholder
	// default, not a locked design value.
	UPROPERTY(EditDefaultsOnly, Category = "Overcrowd Audio", meta = (ClampMin = "0.0", ClampMax = "20000.0"))
	float MuffleFilterFrequencyHz = 500.0f;

private:
	UFUNCTION()
	void HandlePanicOverloadStateChanged(EPanicOverloadState NewState);

	void SetMuffleState(EOvercrowdAudioMuffleState NewState);

	UPROPERTY()
	TObjectPtr<USubmixEffectFilterPreset> MuffleFilterPreset;

	EOvercrowdAudioMuffleState CurrentMuffleState = EOvercrowdAudioMuffleState::Clear;

	bool bHasBoundOvercrowdComponent = false;

	// One-shot guard so a missing world/audio device at SetMuffleState() time only logs once
	// per UOvercrowdAudioSubsystem instance, matching MusicSubsystem::bHasWarnedMissingTrack.
	bool bHasWarnedMissingAudioDevice = false;
};
