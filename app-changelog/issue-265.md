# Issue #265: Wire press/hold indicator semantics to all five ability keys

## Summary

Wires the locked press/hold interaction (docs/prd-cursor-aiming.md REQ-3, decision 3)
onto all five ability keys on `AFlatCamera3DPrototypePawn`: a **press** flashes that
ability's shared `UAbilityTargetingIndicatorComponent` (issue #264) in its
`AbilityData` colour and casts immediately via the existing
`UAbilityCastComponent::TryCastAbility` (cast success/failure and all existing gates
are unaffected); **holding** the key past a short threshold — including while the
ability is on cooldown — keeps showing the indicator as a preview only, with no
additional cast; releasing a held key never triggers a delayed cast.

A new `UAbilityPressHoldComponent` owns the per-`EAbilitySlot` press/hold state
machine and its two timers (a fixed-duration press-flash hide timer and a shorter
hold-threshold timer). It calls `IndicatorComponent->Show()` directly rather than
`Flash()` and owns both "when to hide" timers itself — `Flash()`'s own internal
auto-hide timer would otherwise fire mid-hold-preview and yank the indicator hidden
out from under it. One `UAbilityTargetingIndicatorComponent` +
`UAbilityPressHoldComponent` pair is wired onto the pawn in its constructor, and the
five existing `Cast*Ability()` `IE_Pressed` wrappers now delegate through
`AbilityPressHoldComponent->HandleAbilityKeyPressed()` instead of calling
`AbilityCastComponent` directly; five new `Release*Ability()` `IE_Released` wrappers
were added, reusing the same `"CastStun"`/etc. action mappings already in
`Config/DefaultInput.ini` (no `.ini` change needed).

## Press-vs-hold threshold (implementer's judgment, per this issue's AC)

`HoldThresholdSeconds = 0.1f` (100ms), deliberately kept below
`PressFlashDurationSeconds = 0.15f` so the visual handoff from "flash" to
"hold-preview" never produces a hide-then-reshow flicker. Both are
`EditDefaultsOnly` so a future playtesting pass can retune either without a code
change, matching `AbilityCooldownComponent::AbilityCooldownDurations`'s own
"not a locked design value" precedent.

## Acceptance Criteria

| Criterion | Status |
|---|---|
| All 5 ability keys' `BindAction` sites: pressing flashes the indicator (`AbilityData` colour) and casts immediately, existing cast logic unchanged | Done |
| Holding a key past the documented threshold shows the indicator as a persistent preview with no additional cast | Done — `KrowdKontrol.Unit.AbilityPressHoldComponent` case (b) |
| Holding while on cooldown shows the same preview (cast simply fails via existing gates, as today) | Done — case (c) |
| Releasing a held (hold-preview) key never triggers a cast | Done — case (d) |
| Automation tests cover all 4 scenarios plus the fast-tap-cancels-timer regression | Done — cases (a)-(e) |
| `FKrowdKontrolFlatCamera3DAbilityCastWiringTest` and `FKrowdKontrolFlatCamera3DPipelineSmokeTest` still pass unmodified | Unmodified — `Cast*Ability()` method names/signatures kept exactly as-is, only bodies changed |
| PR body documents the chosen press-vs-hold threshold and why | Done — see above |
| `app/` and `app-source-tracked/` are both updated identically; changelog added | Done — verified via `diff`, no output |

## Not Building (Scope Limits)

- The real per-ability shapes/ranges (cone for Snare, line for Root, etc.) — deferred
  to the separate "Ability Targeting Shapes & Effect Semantics" PRD. This issue uses
  one placeholder `CircleAtActor` shape (radius = `CastRangeUnits`) for all five
  abilities.
- Robot facing the cursor (REQ-2) and cursor world-position consumption — out of
  scope; only `Colour`/`CastRangeUnits`/`GetActorLocation()` are read.
- Simultaneous multi-key hold previews — one shared indicator instance is wired onto
  the pawn (its own documented one-instance-per-Owner limit); holding two keys at
  once is last-press-wins, a known, accepted visual-only gap.

## Files

| File | Action |
|------|--------|
| `app/Source/KrowdKontrol/AbilityPressHoldComponent.h` | CREATE |
| `app/Source/KrowdKontrol/AbilityPressHoldComponent.cpp` | CREATE |
| `app/Source/KrowdKontrol/FlatCamera3DPrototypePawn.h` | UPDATE |
| `app/Source/KrowdKontrol/FlatCamera3DPrototypePawn.cpp` | UPDATE |
| `app/Source/KrowdKontrol/Private/Tests/KrowdKontrolAbilityPressHoldComponentTest.cpp` | CREATE |
| `app/Source/KrowdKontrol/EnemyBase.h` | UPDATE — added `friend class FKrowdKontrolAbilityPressHoldComponentTest;` (not called out by the original plan; needed because `TickCheckDetection` is private and the test drives it directly, same grant every other `KrowdKontrol.Unit.*` test that calls it already has) |
| `docs/prd-cursor-aiming.md` | UPDATE — REQ-3 marked fully implemented |

## Validation Evidence

```
$ python harness/ci.py --quick
HARNESS_START mode=quick driver=cli
STATIC_SKIPPED no 'static' command in harness.config.json
UNIT_PASSED tests=94
GATE_OK mode=quick
```

Targeted run (`KrowdKontrol.Unit.AbilityPressHoldComponent` +
`FlatCamera3DPrototypePawnAbilityCastWiring` + `FlatCamera3DPipelineSmoke` +
`AbilityCastComponent` + `AbilityTargetingIndicatorComponent`): `passed=5 total=5`.
Full `KrowdKontrol.Unit.*` suite: `passed=94 total=94`, no regressions.

See `implementation.md` and `validation.md` in the workflow run artifacts for the
full record.

## Post-Review Fixes (self-fix pass)

Addressed all HIGH/MEDIUM findings from the PR #278 review, plus the LOW error-handling
finding:

- Added `UAbilityPressHoldComponent::EndPlay()`, clearing both per-slot timer arrays on
  teardown — mirrors `UAbilityTargetingIndicatorComponent::EndPlay`, the pattern the
  original plan cited but only half-applied.
- Logged a `Warning` when `HandleAbilityKeyPressed` finds no `Owner` (previously silent),
  matching `UAbilityTargetingIndicatorComponent::InitializeIndicatorVisual`'s bar for the
  identical condition.
- Extended `FKrowdKontrolFlatCamera3DAbilityCastWiringTest` with a `ReleaseWrapper` per
  case, driving the pawn's real `AbilityPressHoldComponent` through both press and
  release rather than only press — closes the zero-coverage gap on the constructor
  wiring and the five `Release*Ability()` wrappers.
- Extended `FKrowdKontrolFlatCamera3DPipelineSmokeTest`'s `BindAction` existence-check
  loop to also cover the five `IE_Released` registrations.
- Added a timer-rate assertion to case (a) (`GetTimerRate` against
  `HoldThresholdSeconds`/`PressFlashDurationSeconds`), proving the real `SetTimer`
  delegate wiring — not just the callback bodies — matches the documented flicker-
  avoidance ordering.
- Added case (f): a repeated press→release→press cycle on the same slot, proving no
  timer-handle leak or double-fire. Only the first press casts (the second is blocked by
  the cooldown the first one started, same as case (c)'s hold-during-cooldown behavior)
  — the review's suggested "both presses cast" assertion didn't match
  `UAbilityCastComponent::TryCastAbility`'s cooldown gate and was corrected during
  self-fix.
- Added case (g): out-of-range `EAbilitySlot` calls to all four handlers no-op cleanly.

Not fixed: `CLAUDE.md`'s `Conventions` section remains `TBD` (LOW, docs-impact finding)
— `CLAUDE.md` is a protected path the factory cannot modify, and the finding's own
recommendation was to defer this to a separate documentation PR rather than block #265
on it.

Full `KrowdKontrol.Unit.*` suite after fixes: `passed=94 total=94`, no regressions
(confirmed the two flagged failures on the first fix-pass run —
`AbilityPressHoldComponent` and `EnemyBase` — were the new (f) assertion bug above and a
pre-existing test-order flake respectively; `EnemyBase` passes in isolation).
