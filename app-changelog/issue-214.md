# Issue #214: Follow-the-player movement for Controlled enemies

`AEnemyBase` previously had no movement while `CurrentState == Controlled` for the
default case (Stun/Sleep/Root without their special flags) - `TickChaseMovement` only
ran during `Alert` (or Controlled+Snare, a pre-existing special case) and
`TickFleeMovement` only ran for Controlled+Fear. This blocked the `herd` step of the
core loop: there was no way to spatially move a controlled enemy toward a future
`ATargetZone` (issue #211). This issue adds a third, additive per-tick movement -
`TickFollowMovement` - that makes a Controlled enemy trail the player pawn
pied-piper-style at a new `FollowSpeedUnitsPerSecond`, stopping at a new
`FollowDistanceUnits` gap so it never stacks on the player pawn.

`FollowSpeedUnitsPerSecond` defaults to `300.0f` - half `AEnemyBase`'s own base
chase-speed default (`600.0f`): fast enough to keep pace with a walking player, slow
enough to read as trailing rather than chasing. `FollowDistanceUnits` defaults to
`200.0f`, the trailing gap the enemy stops at.

**Elite multiplier applies to follow speed too**: a new `GetEffectiveFollowSpeedUnitsPerSecond()`
mirrors `GetEffectiveMovementSpeedUnitsPerSecond()`'s exact shape
(`FollowSpeedUnitsPerSecond * (bIsElite ? EliteMovementSpeedMultiplier : 1.0f)`) - an
Elite enemy is harder to herd in every movement mode, not just while chasing.

**The Snare/Fear movement-conflict gate (the load-bearing design decision here)**: the
issue's AC says "while `CurrentState == Controlled`, the enemy moves toward the player
every tick" without qualification, but two of the five abilities already move a
Controlled enemy every tick via a different rule - Snare
(`bAllowsMovementWhileControlled`, keeps chasing via `TickChaseMovement` at half
speed, issue #254) and Fear (`bFleesFromCasterWhileControlled`, keeps fleeing via
`TickFleeMovement`, issue #253). Running `TickFollowMovement` unconditionally for
every Controlled state would move a Snare-controlled enemy twice in the same tick
(and defeat the "never stacks" requirement, since `TickChaseMovement` has no
stop-short clamp), and would move a Fear-controlled enemy in opposing directions in
the same tick. `TickFollowMovement` therefore no-ops whenever
`AbilityData::Get(ControllingAbility)` already flags either
`bAllowsMovementWhileControlled` or `bFleesFromCasterWhileControlled` - the same
"flag overrides the state default" pattern `IsAttackBehaviorActive()`/
`IsMovementBehaviorActive()` already establish for attack/chase. Snare and Fear's
existing, already-tested Controlled-state movement is entirely unchanged.

No new state, no new transition, no new public API surface beyond the two
`EditDefaultsOnly` properties - the existing `Controlled -> Alert` duration reversion
and `Controlled -> Banked` banking edge both already stop follow movement for free,
because `TickFollowMovement` is state-gated exactly like `TickChaseMovement`/
`TickFleeMovement` already are.

## Files changed

| File | Action | What it contains |
|------|--------|-------------------|
| `EnemyBase.h` | UPDATE | New `FollowSpeedUnitsPerSecond`/`FollowDistanceUnits` `EditDefaultsOnly` properties; protected `GetEffectiveFollowSpeedUnitsPerSecond()` declaration; private `TickFollowMovement()` declaration |
| `EnemyBase.cpp` | UPDATE | `TickFollowMovement()`/`GetEffectiveFollowSpeedUnitsPerSecond()` implementations; `Tick()` now calls `TickFollowMovement(PlayerLocation, DeltaTime)` between `TickChaseMovement` and `TickFleeMovement` |
| `Private/Tests/KrowdKontrolEnemyBaseTest.cpp` | UPDATE | 9 new cases `(a-follow)`-`(h-follow)`: toward-player movement, stop-short clamp, already-within-range no-op, no-op outside Controlled (Idle/Alert/Attack/Banked), no-op for Snare-Controlled, no-op for Fear-Controlled, duration-reversion halts follow, Elite multiplier, real `Tick()` wiring |

## Notes

- No pathfinding - follow movement is straight-line only, matching
  `TickChaseMovement`/`TickFleeMovement`'s existing "no pathfinding" precedent.
- `FollowSpeedUnitsPerSecond` is a flat base-class property, not a per-concrete-type
  virtual override point (unlike chase speed) - the issue's AC calls for "a new
  `EditDefaultsOnly` property" (singular, base-class). Every controlled enemy trails
  the player at the same, predictable pace regardless of its own type's chase speed.
  A future issue wanting per-type follow speeds would be new scope.
- No `KrowdKontrol.PIE.*` test added - this feature has no begin-play-order,
  serialization, save/load, or PIE-only-naming failure mode
  (`harness/README.md`'s criteria for requiring one); it's pure per-tick arithmetic,
  identical in shape to `TickChaseMovement`/`TickFleeMovement`, which likewise only
  ever got `KrowdKontrol.Unit.*` coverage.
- Issue #211's `IHerdable`/`ATargetZone`/banking mechanics remain a separate,
  unblocked-by-this-issue piece of work; this issue supplies only the movement half.
- **Implementation fix during development**: the first draft of test case
  `(h-follow)` placed the simulated player pawn at `(5000, 0, 0)`, outside
  `DetectionRangeUnits` (1500.0f default) - `TickCheckDetection` never reached Alert,
  so the subsequent `ReceiveControl()` no-op'd (it requires Alert/Attack), and
  `TickFollowMovement` correctly moved nothing because the enemy was never Controlled
  in the first place. Fixed by moving the simulated player pawn to `(1000, 0, 0)`,
  matching case `(t)`'s existing distance, so the enemy actually reaches Controlled
  before `Tick()` is exercised.

## Acceptance criteria

- [x] While `CurrentState == Controlled`, the enemy moves toward the player every
      tick, using the `min(remaining, speed x dt)` clamp - for abilities that don't
      already own Controlled-state movement (all except Snare/Fear).
- [x] Movement stops at `FollowDistanceUnits`, never stacking on the player.
- [x] `FollowSpeedUnitsPerSecond` is a new `EditDefaultsOnly` property, distinct from
      chase speed, defaulting to 300.0f (half the base chase-speed default).
- [x] Controlled ending (duration reversion or banking) stops follow movement via the
      existing state-gate, with zero new transition logic.
- [x] Idle/Alert(unchanged)/Attack/Banked unaffected.
- [x] Elite multiplier applies to follow speed via `GetEffectiveFollowSpeedUnitsPerSecond()`,
      covered by test `(g-follow)`.
- [x] The Snare/Fear movement-conflict gate is implemented and covered by tests
      `(d-follow)`/`(e-follow)` - without it, the literal AC as written would regress
      issue #254/#253's existing, tested behavior.
- [x] All 9 new `KrowdKontrol.Unit.*` cases pass; zero regressions in existing cases.
- [x] `app-changelog/issue-214.md` created; `app/`, `app-source-tracked/` both updated
      identically for `.h`/`.cpp`/test-file changes (MISSION.md Hard Invariant 8).

## Validation evidence

```
$ harness/run_ue_automation.sh KrowdKontrol.Unit.
UE_BUILD_START KrowdKontrolEditor Win64 Development
UE_BUILD_OK
UE_AUTOMATION_RESULT passed=104 total=104
UE_AUTOMATION_OK

$ python harness/ci.py --quick
HARNESS_START mode=quick driver=cli
STATIC_SKIPPED no 'static' command in harness.config.json
UNIT_PASSED tests=104
PIE_PASSED tests=5
GATE_OK mode=quick
```

This issue adds 9 cases inside the existing `KrowdKontrol.Unit.EnemyBase` top-level
test function - no new top-level test was added, matching the same pattern
`(v-fear)`-`(z-fear)` themselves established when they were added.

## Review findings addressed (PR #300)

- **HIGH - Root-Controlled follow semantics**: code review flagged that
  `TickFollowMovement`'s gate lets Root-Controlled enemies follow, apparently
  contradicting Root's "in place" flavor text and a claimed `ARootSurgeBoss`
  dependency on Root-locked adds staying stationary. Checked both premises against
  source: `docs/prd-herd-mechanic.md`'s operator design decision (2026-08-22, locked)
  says "Controlled enemies follow the player" with no per-ability exception, and
  `ARootSurgeBoss::HasRootLockedAdd()` only checks `GetEnemyState()`/
  `GetControllingAbility()`, never position - it doesn't depend on the add staying
  put. Root following is therefore intended, not an oversight; the boss-breakage
  premise was factually incorrect. Fixed the now-stale artifacts instead: Root's
  `EffectDescription` no longer claims "in place," `EnemyBase.h`'s
  `IsAttackBehaviorActive()` doc comment no longer says "Root immobilizes movement,"
  and `TickFollowMovement`'s own declaration comment now states explicitly why Root
  isn't excluded. Added test `(root-follow)` locking in the behavior.
- **MEDIUM - `GetControlledSpeedMultiplier()` omission untestable**: added a comment
  on `TickFollowMovement`'s `MoveDistance` line and near Snare's
  `ControlledSpeedMultiplier` in `AbilityData.cpp`, cross-referencing each other, per
  test-coverage review's recommended Option A (comment-only - a test seam would need
  restructuring the closed `AbilityData::Get()` switch for a non-bug).
- **MEDIUM - PRD REQ-1 not marked implemented**: annotated
  `docs/prd-herd-mechanic.md`'s REQ-1 heading with `— ✅ implemented, issue #214`,
  matching this repo's established per-PRD convention.
- **LOW - no `Tick()`-wired test for Snare/Fear gate**: added `(i-follow)`, mirroring
  `(h-follow)`'s real-`Tick()` scaffold for a Snare-Controlled actor, asserting total
  displacement matches `TickChaseMovement`'s half-speed distance exactly (proving the
  gate holds through the real per-frame wiring, not just in isolation).
