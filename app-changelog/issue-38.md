# Issue #38 — Add distinct audio treatment (muffling/distortion) for the Overcrowd punishment state

## Summary

Adds `UOvercrowdAudioSubsystem` (`UTickableWorldSubsystem`), a new event-driven system that
listens to `UOvercrowdDetectionComponent::OnPanicOverloadStateChanged` and applies a
submix-wide low-pass "muffle" filter to the game's main output submix
(`UAudioMixerBlueprintLibrary::SetSubmixEffectChainOverride`) the instant the Overcrowd state
becomes `Active`, clearing it (`ClearSubmixEffectChainOverride`) the instant it stops being
`Active`. This is a submix-level effect rather than per-`UAudioComponent` low-pass calls, which
are documented as unreliable at runtime because Sound Attenuation settings are cached at
sound-instance init — a submix effect instead covers the entire downmixed output (including
`UMusicSubsystem`'s tracks) regardless of when individual sounds started playing. Structure
mirrors `UMusicSubsystem`'s existing 2-state pattern (public state getter, `BlueprintAssignable`
delegate, friend-class test access, idempotent no-op-on-same-state guard) per the issue's own
triage decision.

## Acceptance Criteria

- [x] `UOvercrowdAudioSubsystem` applies a low-pass mix-wide filter the instant
      `OnPanicOverloadStateChanged` broadcasts `Active` — `HandlePanicOverloadStateChanged` calls
      `SetSubmixEffectChainOverride` on `NewState == Active`.
- [x] The effect clears immediately on any non-`Active` broadcast — same handler's `else` branch
      calls `ClearSubmixEffectChainOverride` (not an exhaustive switch, so it degrades safely to
      `Clear` even for a hypothetical future third state).
- [x] Genuinely distinct, audible treatment — submix-level low-pass via
      `USubmixEffectFilterPreset`, not the known-unreliable per-component
      `SetLowPassFilterFrequency` path.
- [x] `KrowdKontrol.Unit.OvercrowdAudioSubsystem` passes, proving activate/deactivate sync, no
      double-fire, and graceful no-component handling — `passed=1 total=1`.
- [x] No regressions in the existing `KrowdKontrol.Unit.*` suite after the `Build.cs` module
      additions (`AudioMixer`, `Synthesis`) — `tests=44` (43 pre-existing + 1 new), all passing.
- [x] `app-source-tracked/` left for `create-pr` to mirror automatically — implementation only
      touched `app/`; this mirror step is what produced this PR's tracked diff.

## Validation Evidence

```
$ python3 harness/ci.py
HARNESS_START mode=full driver=cli
STATIC_SKIPPED no 'static' command in harness.config.json
UNIT_PASSED tests=44
APP_STARTED driver=cli
UE_BUILD_START KrowdKontrolEditor Win64 Development
UE_BUILD_OK
UE_AUTOMATION_RESULT passed=1 total=1
UE_AUTOMATION_OK
E2E_PASSED steps=1
HOLDOUT_ABSENT no .factory/holdout/run.py
MUTATIONS_ABSENT no harness/mutations/run.py
GATE_OK mode=full
```

Full mode passed on the first run — no fixes were required. Hard invariants unaffected (no
gameplay-information colours, no ability/enemy/networking/dimensionality changes touched).

## Deviations from Plan

Two files outside the plan's "Files to Change" table needed a one-line addition each, so the
plan's own Task 5 test code would compile: `OvercrowdDetectionComponent.h` and `EnemyBase.h`
each got one `friend class FKrowdKontrolOvercrowdAudioSubsystemTest;` grant (matching each
file's existing per-test-class friend-grant convention), needed because the test drives both
classes' private tick/state-advance methods directly rather than through a real tick loop. No
behavior change to either class. See `implementation.md` for full detail.
