# Issue #20: Overcrowd screen-distortion visual effect (Panic Overload)

Adds a screen-space distortion visual treatment (chromatic aberration + vignette, eased
in/out) that engages while `UOvercrowdDetectionComponent` reports Panic Overload
`Active`, and a test proving it and the existing audio-muffling treatment
(`UOvercrowdAudioSubsystem`, issue #38) engage/disengage together off the same
delegate. Per the issue's own final rescoping comment (2026-08-18, OWNER-approved),
audio muffling was already shipped by issue #38 and is intentionally **not** touched
by this change — see `plan.md`'s Scope Decision section for the full comment-thread
history.

New: `UCameraModifier_OvercrowdDistortion` (a `UCameraModifier` subclass driving
`FPostProcessSettings::SceneFringeIntensity`/`VignetteIntensity` on an eased 0..1
alpha) and `UOvercrowdVisualEffectSubsystem` (a `UTickableWorldSubsystem`,
structurally mirroring `UOvercrowdAudioSubsystem`) that binds to
`UOvercrowdDetectionComponent::OnPanicOverloadStateChanged` and drives the modifier.
Zero new content assets, zero new `Build.cs` dependencies (`Engine` module already
present).

## Files changed (all under `app/`, gitignored per D-003 — this is the tracked-repo
record of that change)

| File | Action | What it contains |
|------|--------|-------------------|
| `app-source-tracked/Source/KrowdKontrol/CameraModifier_OvercrowdDistortion.h`/`.cpp` | CREATE | New `UCameraModifier` subclass: `SetEngaged(bool)`, `GetCurrentAlpha()`, per-frame eased chromatic-aberration/vignette blend in `ModifyPostProcess`. |
| `app-source-tracked/Source/KrowdKontrol/OvercrowdVisualEffectSubsystem.h`/`.cpp` | CREATE | New `UTickableWorldSubsystem`, mirrors `UOvercrowdAudioSubsystem`'s bind/state-flip/broadcast shape; binds to the detection component and to the player's `APlayerCameraManager`, owns the `UCameraModifier_OvercrowdDistortion` instance. |
| `app-source-tracked/Source/KrowdKontrol/OvercrowdDetectionComponent.h` | UPDATE | 2 new friend grants for the new test classes — no behavior change. |
| `app-source-tracked/Source/KrowdKontrol/EnemyBase.h` | UPDATE | 2 new friend grants for the new test classes (curated — see note below). |
| `app-source-tracked/Source/KrowdKontrol/Private/Tests/OvercrowdDistortionStateTestListener.h`/`.cpp` | CREATE | Dynamic-multicast-delegate test listener, mirrors `OvercrowdMuffleStateTestListener`. |
| `app-source-tracked/Source/KrowdKontrol/Private/Tests/KrowdKontrolOvercrowdVisualEffectSubsystemTest.cpp` | CREATE | `KrowdKontrol.Unit.OvercrowdVisualEffectSubsystem` — bind/idempotency/state-flip/convergence coverage. |
| `app-source-tracked/Source/KrowdKontrol/Private/Tests/KrowdKontrolOvercrowdAudioVisualSyncTest.cpp` | CREATE | `KrowdKontrol.Unit.OvercrowdAudioVisualSync` — direct closure of the issue's "wiring verification" AC: one `AdvancePanicOverloadState` call flips both the audio and visual subsystems together; one simulated recovery broadcast clears both together. |

**Note on `EnemyBase.h`:** the shared, unisolated `app/` symlink (`FACTORY_RULES.md`
§8) had picked up an in-progress, unmerged line from PR #164 (issue #50,
`FKrowdKontrolRootSurgeBossTest`) by the time this task's validation ran. That line
belongs to a different, still-open PR and is deliberately **excluded** from this
diff — only the 2 friend-grant lines this issue actually needs were copied into
`app-source-tracked/`. `app/`'s own live copy (never repo content) was left as-is
per validation's non-destructive-fix guidance, since editing shared state outside
this task's own PR risks corrupting issue #50's in-flight work.

## Acceptance criteria

- [x] **Screen-space distortion visual effect engages/disengages on Panic Overload,
      each on its own eased curve.** `UCameraModifier_OvercrowdDistortion` eases
      chromatic aberration + vignette in over `EaseInSeconds` (default 1.0s) and out
      over `EaseOutSeconds` (default 1.5s, deliberately slower — "tension lingers a
      beat longer than it resolves").
- [x] **Driven entirely by `UOvercrowdDetectionComponent::OnPanicOverloadStateChanged`**
      — no re-implementation of trigger logic; `UOvercrowdVisualEffectSubsystem` binds
      to the existing delegate, same pattern as `UOvercrowdAudioSubsystem`.
- [x] **No numeric penalty added** — presentation-only, touches no gameplay/health/
      energy systems.
- [x] **Audio muffling untouched** — `UOvercrowdAudioSubsystem` is not modified by
      this change at all.
- [x] **New automation test proves both treatments engage/disengage together**
      (`KrowdKontrolOvercrowdAudioVisualSyncTest.cpp`), directly closing the issue's
      final rescoping comment's explicit ask.
- [x] **No Hard Invariant violated** — no gameplay-information colour introduced
      (chromatic aberration/vignette are not hue tints), no `Build.cs`/module change,
      no `.uasset`/content asset added.

## Validation evidence

`python3 harness/ci.py` (full mode):
```
HARNESS_START mode=full driver=cli
STATIC_SKIPPED no 'static' command in harness.config.json
UNIT_PASSED tests=63
APP_STARTED driver=cli
UE_BUILD_OK
UE_AUTOMATION_RESULT passed=1 total=1
UE_AUTOMATION_OK
E2E_PASSED steps=1
GATE_OK mode=full
```
63 unit tests (61 pre-existing + 2 new: `KrowdKontrol.Unit.OvercrowdVisualEffectSubsystem`,
`KrowdKontrol.Unit.OvercrowdAudioVisualSync`), no regressions. Concurrent-task
leakage check (cross-diffed every `UPDATE` file against its `app-source-tracked/`
baseline) confirmed only `EnemyBase.h` was affected by shared-`app/`-state leakage,
curated per the note above; `OvercrowdDetectionComponent.h` diffed clean and was
copied verbatim.
