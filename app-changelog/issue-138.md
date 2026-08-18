# Issue #138: Core ability-cast/application system (player casts CC onto enemies)

Builds the keystone system that finally calls `AEnemyBase::ReceiveControl(EAbilitySlot)`
from a real gameplay path - previously a bare, zero-production-caller hook (issue #48
and 7 other rejected issues were blocked on this exact gap). A new
`UAbilityCastComponent`, attached to `AFlatCamera3DPrototypePawn` (the only pawn placed
in the project's actual playable level, per issue #132's precedent), is the single
`TryCastAbility(EAbilitySlot)` entry point a 5-key input binding (One-Five, one per
`EAbilitySlot`) calls: it gates the attempt through `UAbilityUnlockComponent::
IsAbilityUnlocked` (locked abilities can't cast) and `UAbilityCooldownComponent::
TryStartCooldown` (the documented, sole legal cast-gating point), does minimal
automatic targeting (nearest hot - Alert or Attack - enemy within `CastRangeUnits`,
mirroring `UOvercrowdDetectionComponent`'s scan shape), and applies control via
`AEnemyBase::ReceiveControl()`. A whiff (no valid target in range) does not consume the
cooldown.

`AEnemyBase` gains a stored `ControllingAbility` + `GetControllingAbility()` accessor
(unblocking issue #48-class consumers), and a timed `Controlled -> Alert` reversion
edge: `ReceiveControl()` now arms a `RemainingControlledSeconds` countdown sourced from
`AbilityData::Get(Ability).BaseDurationSeconds`, with a per-enemy override hook
(`GetControlledDurationOverrideSeconds()`) that `ASniperEnemy` uses for issue #121's
Sleep = 7s case (vs the 5s baseline every other enemy/ability pairing uses). If that
duration elapses before `TransitionToBanked()` is called, the enemy reverts to Alert
and re-engages - operator decision, 2026-08-18: no enemy is ever stuck Controlled
forever, and this is never treated as a kill (MISSION.md Hard Invariant 2). Banking
within the window remains the only path to Banked.

`UAbilityCastComponent` broadcasts `OnAbilityCastApplied` exactly once per successful
cast, an extension point future issues (Gizmo bark trigger #59, instrumentation #37)
consume - not wired up by this issue.

## Files changed

| File | Action | What it contains |
|------|--------|-------------------|
| `EnemyBase.h`/`.cpp` | UPDATE | `ControllingAbility` + `GetControllingAbility()`, `RemainingControlledSeconds`, protected virtual `GetControlledDurationOverrideSeconds()` (base default -1.0f = no override), private `TickControlledDuration()`; `ReceiveControl()` now arms the duration; `Tick()` calls `TickControlledDuration()` unconditionally; transition-table doc comment updated with the new `Controlled -> Alert` edge |
| `SniperEnemy.h`/`.cpp` | UPDATE | Overrides `GetControlledDurationOverrideSeconds()`: Sleep = 7.0f (issue #121), all other abilities fall through to `Super::` |
| `AbilityCastComponent.h`/`.cpp` | CREATE | `UAbilityCastComponent`: `TryCastAbility(EAbilitySlot)`, `FindNearestValidTarget()`, `OnAbilityCastApplied` delegate, `CastRangeUnits` (flat, all 5 abilities alike) |
| `FlatCamera3DPrototypePawn.h`/`.cpp` | UPDATE | New `AbilityCooldownComponent`/`AbilityCastComponent` members (constructed alongside the existing `AbilityUnlockComponent`), 5 `BindAction` calls + 5 thin per-slot handler methods in `SetupPlayerInputComponent()` |
| `app/Config/DefaultInput.ini` | UPDATE (app/ only, no `app-source-tracked/` mirror - `.ini` is not `.h`/`.cpp`/`.Build.cs`) | 5 new `+ActionMappings=` entries: `CastStun`/`CastSleep`/`CastRoot`/`CastFear`/`CastSnare` bound to keys One-Five |
| `Private/Tests/KrowdKontrolEnemyBaseTest.cpp` | UPDATE | `GetControllingAbility()` assertions extending the existing Alert/Attack `ReceiveControl` cases; new duration-expiry-reversion case (Stun, `AbilityData::Get(Stun).BaseDurationSeconds`); new case proving `TransitionToBanked()` before expiry prevents the reversion |
| `Private/Tests/KrowdKontrolSniperEnemyTest.cpp` | UPDATE | Expiry-reversion case via Sleep, explicitly proving the 7s override (not the 5s `AbilityData` baseline) governs (6.9f stays Controlled, past 7.0f reverts to Alert) |
| `Private/Tests/KrowdKontrolBomberEnemyTest.cpp`, `.../TrooperEnemyTest.cpp`, `.../RunnerEnemyTest.cpp` | UPDATE | One expiry-reversion case each, via that type's own `OnControlledEntry` ability (Fear/Root/Snare respectively), reading `AbilityData::BaseDurationSeconds` directly rather than a hardcoded magic number |
| `Private/Tests/AbilityCastAppliedTestListener.h`/`.cpp` | CREATE | `FOnAbilityCastApplied` test listener, mirrors `GizmoBarkTestListener.h`'s shape |
| `Private/Tests/KrowdKontrolAbilityCastComponentTest.cpp` | CREATE | 7 cases: locked-ability block, on-cooldown block, no-target-in-world (cooldown not consumed), successful cast (state + cooldown + broadcast), nearest-of-two selection, out-of-range exclusion, wrong-state (Idle/Controlled/Banked) exclusion |

## Notes

- `EnemyBase.h`'s friend-class list also gained `FKrowdKontrolAbilityCastComponentTest`
  (this issue's own test needs friend access to `TickCheckDetection`/
  `TickControlledDuration` via `AEnemyBaseTestActor`, the same pattern every other test
  class in this list already uses).
- **Known, deliberate divergence between `app/` and `app-source-tracked/`**: the live
  `app/EnemyBase.h` also carries `friend class FKrowdKontrolOvercrowdLevelThresholdTest;`,
  which this PR's tracked diff does not include. That grant is real, necessary,
  already-landed work from a separate, concurrent issue (#23) sharing this task's `app/`
  directory (`KrowdKontrolOvercrowdLevelThresholdTest.cpp` exists only in `app/`, not in
  `app-source-tracked/`) - same precedent `app-changelog/issue-19.md`'s own "Known,
  deliberate divergence" note documents for the same friend-class line. Left in `app/`
  (removing it there would break that unrelated, already-merged-elsewhere work),
  excluded from `app-source-tracked/` (keeping this PR's diff scoped to its own issue).
- Per the plan's explicit scope limits: per-ability cast range (mapping `EAbilityRange`
  Short/Medium/Long to distinct distances), aim-direction targeting, wiring
  `UAbilityCooldownTrayWidget`'s real cooldown display to this cast system, and a
  subclass's Controlled-entry glow resetting on timed expiry-reversion are all
  explicitly NOT built here - see the plan's "NOT Building" section for the full
  rationale on each.

## Acceptance criteria

- [x] Player-initiated cast: 5-key input -> nearest-in-range hot-enemy targeting ->
      `ReceiveControl()` applied.
- [x] Casting is gated through `UAbilityUnlockComponent::IsAbilityUnlocked` and
      `UAbilityCooldownComponent::TryStartCooldown`.
- [x] `AEnemyBase` stores and exposes which ability is controlling it
      (`GetControllingAbility()`).
- [x] `AbilityData::BaseDurationSeconds` drives the Controlled-state duration, with a
      working per-enemy override point (`ASniperEnemy`/Sleep = 7s, issue #121).
- [x] `Controlled -> Alert` timed reversion works per the operator's 2026-08-18
      decision; `EnemyBase.h`'s transition-table doc comment updated.
- [x] `OnAbilityCastApplied` broadcasts exactly once per successful cast+application.
- [x] All 4 concrete enemy subclasses' tests gain an expiry-reversion case.
- [x] Level 1-3 validation commands pass with exit 0.
- [x] Code mirrors existing patterns exactly (naming, structure, logging, doc-comment
      density).
- [x] No regressions in existing `KrowdKontrol.Unit.*` tests.
- [x] `app-changelog/issue-138.md` created per `MISSION.md` Hard Invariant 8.
- [x] Every `.h`/`.cpp` change made under both `app/` and `app-source-tracked/`; the
      `DefaultInput.ini` change (app/ only) is called out explicitly here since it
      won't appear in the `app-source-tracked/` diff.

## Validation evidence

```
$ python harness/ci.py --quick
STATIC_SKIPPED no 'static' command in harness.config.json
UNIT_PASSED tests=47
GATE_OK mode=quick

$ bash harness/run_ue_automation.sh "KrowdKontrol.Unit."
UE_BUILD_OK
UE_AUTOMATION_RESULT passed=47 total=47
UE_AUTOMATION_OK
```

Test count went from the pre-existing 46 (per `app-changelog/issue-19.md`) to 47 - one
new top-level Automation test (`KrowdKontrol.Unit.AbilityCastComponent`); the
`EnemyBase`/`SniperEnemy`/`BomberEnemy`/`TrooperEnemy`/`RunnerEnemy` expiry-reversion
coverage was added as additional cases inside their already-counted test functions.

One review-fix during implementation: `KrowdKontrolAbilityCastComponentTest.cpp`'s
out-of-range-exclusion case originally relocated `OutOfRangeEnemy` via
`SetActorLocation()` *before* calling `TickCheckDetection()`, which measures distance
from the enemy's actual `GetActorLocation()` - the enemy never reached Alert at all
(distance from the far relocated position to the zero-vector detection check exceeded
`DetectionRangeUnits`), so the assertion failed for the wrong reason (never-Alert, not
excluded-by-cast-range). Fixed by driving both enemies to Alert first, then relocating
- confirmed the case now actually exercises `FindNearestValidTarget`'s own range check.
