# Issue #360: Cancel sniper shot and chase player on range-break, re-acquire on return to range

`AEnemyBase::TickCheckDetection` only ever moved a state forward (`Idle -> Alert` on
proximity, `Alert -> Attack` on attack-range proximity) — there was no reverse check
for an enemy already in `Attack` whose target had moved back out of attack range. This
left `ASniperEnemy` (SN-1PR) unbreakable once a shot started telegraphing: the player
could stand just inside 1400 units, trigger the wind-up, then walk away, and the shot
still landed because nothing ever re-checked range once `Attack` was entered.

The fix adds a symmetric `Attack -> Alert` range-break edge to `TickCheckDetection` —
the inverse of the existing `Alert -> Attack` comparison — that calls a new shared
private helper `RevertAttackToAlert()`, extracted from `TickAttackDuration`'s existing
timeout branch so both "how does Attack end without `ReceiveControl()`" paths
(duration-timeout, range-break) go through one `CurrentState = Alert` /
`OnAttackExpired()` / `OnEnemyAttackExpired.Broadcast()` sequence — exactly mirroring
the precedent issue #313 (PR #336) set for the sibling case. This lives in the shared
`AEnemyBase` base class (not `ASniperEnemy`-only), matching where every other
state-machine edge in this class already lives, so it generalizes for free to
Bomber/Runner/Trooper too (see Design Decisions in the plan for the regression-risk
check that justified this — a grep of all `TickCheckDetection` call sites across the
existing test suite found none that exercise the new branch, so no existing assertion
could regress).

`ASniperEnemy::MovementSpeed` (300.0f) was added as a new `GetMovementSpeedUnitsPerSecond()`
override, mirroring `ABomberEnemy::MovementSpeed`/`ARunnerEnemy::MovementSpeed` exactly
— SN-1PR never needed a chase speed before (its attack range almost equals its
detection range), so the AC's "named, tunable, below player speed" chase-speed
requirement now has a real, Blueprint-tunable home instead of silently inheriting the
base class's inert 600.0f default. `BomberEnemy.h`'s stale comment claiming Sniper
"deliberately keeps [the] base default" was corrected to reflect this.

## Acceptance criteria

- [x] **Leaving the sniper's attack range while a shot is telegraphing cancels the
  shot and drops the target.** Covered by test `(v)` in `KrowdKontrolSniperEnemyTest.cpp`
  — breaks range mid-telegraph, asserts the tell light clears, `OnEnemyAttackExpired`
  fires exactly once, and `OnSniperShotFired` never fires even after advancing well
  past the original telegraph duration. (Damage application on a landed shot is
  issue #358's scope, tracked separately in PR #385 — not part of this issue.)
- [x] **On dropping its target, the sniper enters a chase state (reuses existing
  `Alert` + `TickChaseMovement`) and closes distance until back within attack range.**
  Covered by unit test `(y)` (drives `TickChaseMovement` directly, asserts distance
  moved == `MovementSpeed * DeltaSeconds`) and the new
  `KrowdKontrol.PIE.SniperRangeBreakChase` scenario (real end-to-end movement).
- [x] **Re-entering attack range re-acquires the player and restarts the telegraph
  from scratch, with no partial/stored progress carried over.** Covered by test `(w)`
  — breaks range 0.1s from firing, re-enters range, advances only 0.2s (would fire if
  progress had carried over) and asserts no shot, then finishes a full fresh telegraph
  and asserts the shot fires.
- [x] **Chase movement speed is a named, tunable constant, set lower than the
  player's own move speed.** `ASniperEnemy::MovementSpeed` (300.0f), covered by test
  `(x)` — below both the base-class 600.0f default and the player's engine-default
  1200.0f `UFloatingPawnMovement::MaxSpeed` (unmodified in this project's tracked
  C++, confirmed by grep).
- [x] **Unit test coverage for the full loop (acquire -> break -> chase ->
  re-acquire).** `(i6)`/`(i6b)` in `KrowdKontrolEnemyBaseTest.cpp` (generic
  base-class mechanism, all 4 enemy types) plus `(v)`/`(w)`/`(x)`/`(y)` in
  `KrowdKontrolSniperEnemyTest.cpp` (Sniper-specific AC).
- [x] **PIE scenario coverage of the same loop.**
  `KrowdKontrol.PIE.SniperRangeBreakChase` — real per-frame `Tick()` path, no
  friend/direct calls to `TickCheckDetection`/`TickChaseMovement`/`RevertAttackToAlert`.
- [x] All Level 1-4 validation commands pass with the expected output shape (see
  below).
- [x] Code mirrors existing patterns exactly (naming, structure, comment style,
  `RevertAttackToAlert()` sharing) — see the plan's Design Decisions and Patterns to
  Mirror sections.
- [x] No regressions in existing tests — full `KrowdKontrol.Unit.*`/`KrowdKontrol.PIE.*`
  suites pass unmodified elsewhere.

## Deviation from the plan

The plan's Task 8 specified opening `/Game/Maps/L_Level01` for the new PIE test.
Issue #42's own changelog (L_Level01's authoring issue) deliberately deferred
`ASniperEnemy` placement out of that level ("`ASniperEnemy` is deliberately deferred
to a later, denser level"), and issue #43's changelog confirms `L_Level02` is the
first level to place it (2x `ASniperEnemy`). `KrowdKontrolPIESniperRangeBreakChaseTest.cpp`
therefore opens `/Game/Maps/L_Level02` instead — this is the scenario the plan's own
Risks table anticipated ("if it doesn't [contain a placed `ASniperEnemy`], the fix is
either placing a sniper in the level or retargeting the test at a different level, not
a code change to this issue's actual fix"), resolved by retargeting rather than adding
a level asset.

## Validation evidence

`python harness/ci.py --quick`: `GATE_OK mode=quick` — `UNIT_PASSED tests=127`,
`PIE_PASSED tests=8` (full suite, no regressions).

Individual filters:
- `harness/run_ue_automation.sh KrowdKontrol.Unit.EnemyBase`: `passed=2 total=2`
- `harness/run_ue_automation.sh KrowdKontrol.Unit.SniperEnemy`: `passed=1 total=1`
- `harness/run_ue_automation.sh KrowdKontrol.Unit.BomberEnemy`: `passed=1 total=1`
- `harness/run_ue_automation.sh KrowdKontrol.PIE.SniperRangeBreakChase`: `passed=1 total=1`
- `harness/run_ue_automation.sh KrowdKontrol.PIE.`: `passed=8 total=8` (includes the
  pre-existing `PostContactAttackRecovery` scenario, unaffected)

`UE_BUILD_OK` (clean UBT compile, `KrowdKontrolEditor Win64 Development`) prior to the
above.

## Post-review correction

The PR's original `app-source-tracked/` mirror was diffed against the committed
baseline and its Validation Evidence claimed "no concurrent-task leakage from the
shared `app/` symlink" — that claim was false. The mirror actually contained the
complete, undisclosed implementation of unrelated issue #358 (sniper contact damage:
`ShotDamageAmount`, a `FindPlayerEnergyComponent()`/`ApplyContactDamage()` call in
`AdvanceAttackTelegraph()`, and tests `(t)`/`(u)`/`(u2)`), picked up from the shared,
gitignored `app/` symlink at mirror time from issue #358's own concurrent, independent
work (already landing separately in PR #385). This was caught by code review
(`code-review-findings.md`, CRITICAL) and independently corroborated by test-coverage
review and the pre-review scope note.

Fixed in the self-fix pass: stripped `ShotDamageAmount`, the `ApplyContactDamage` call
block and its `#include "PlayerEnergyComponent.h"`, and tests `(t)`/`(u)`/`(u2)` from
`SniperEnemy.h`/`.cpp`/`KrowdKontrolSniperEnemyTest.cpp`, leaving issue #358 to land
through its own dedicated review in PR #385. Test `(v)`'s no-damage assertion, which
was only meaningful because of the now-removed `ApplyContactDamage` call, was replaced
with an `OnSniperShotFired` listener-count check (mirroring `(w)`'s existing pattern)
so it stays a meaningful regression guard independent of which PR owns the
damage-application code path. Also added test `(i6-boundary)` in
`KrowdKontrolEnemyBaseTest.cpp`, pinning the `Distance == GetAttackRangeUnits()`
exact-boundary case that the new branch's `>`/`<=` asymmetry depends on (test-coverage
review, MEDIUM finding).

Re-validated after the fix: `python harness/ci.py --quick` → `GATE_OK mode=quick`,
`UNIT_PASSED tests=127`, `PIE_PASSED tests=8`.
