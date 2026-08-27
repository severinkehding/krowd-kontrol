# Issue #313: Fix enemy freeze after player contact — guarantee an unconditional exit from every enemy state

An enemy driven into `Attack` (the state a player-contact/aggro advance lands it in)
had no coded exit except `ReceiveControl()` — a fresh player ability cast. If the
player retreated out of range, or the enemy's own per-type attack telegraph finished
with nothing left to do, the enemy froze in place forever, breaking the
chase → contact → recover → re-engage loop and leaving "cast a control ability on it"
as the only (unintended) way to unstick it.

The fix adds `RemainingAttackSeconds` / `GetAttackDurationSeconds()` /
`TickAttackDuration()` to `AEnemyBase`, mirroring the existing
`TickControlledDuration` exit-timer pattern used for `Controlled → Alert` exactly:
`AdvanceToAttack()` arms a countdown (base default 2.5s, chosen with a safety margin
over every concrete subclass's longest `AttackTelegraphSeconds`, `ABomberEnemy`'s
2.0s), and `TickAttackDuration()` — called unconditionally from `Tick()` alongside
`TickControlledDuration` — reverts `Attack → Alert` once it elapses, regardless of
whether any per-type telegraph completed or a player applied a control ability. A new
`OnAttackExpired()` virtual hook and `OnEnemyAttackExpired` delegate mirror
`OnControlledExpired()`/`OnEnemyControlledExpired` for parity.

A full state-machine audit (every write-site of `CurrentState` in `EnemyBase.cpp`)
confirmed `Attack` was the only dead-end state — `Idle`, `Alert`, and `Controlled`
already have unconditional or player-independent exits, and `Banked` is deliberately
terminal by design (MISSION.md Hard Invariant 2, already covered by an existing test).

## Acceptance criteria

- [x] **Enemy resumes normal behaviour after player contact, without a new ability
  cast.** `TickAttackDuration()` reverts `Attack → Alert` unconditionally on timeout;
  covered by regression test case `(i5)`/`(i5b)` in `KrowdKontrolEnemyBaseTest.cpp`,
  which drives an enemy into `Attack` and asserts it reverts to `Alert` with no
  `ReceiveControl()` call, then can re-enter `Attack` again if still in range —
  proving it actually resumes its behaviour tree rather than getting stuck at `Alert`.
- [x] **Automated regression test that fails pre-fix, passes post-fix.** New test
  case added to the existing `KrowdKontrol.Unit.EnemyBase` automation test; asserts
  `OnEnemyAttackExpired` fires exactly once and state transitions unconditionally.
- [x] **Full state-machine audit for other dead-end states.** Completed in the
  investigation (every `CurrentState` writer site reviewed) — `Attack` was the only
  dead end; no follow-up issue needed.
- [x] **No balance/damage/UI changes.** Confirmed out of scope and untouched — no
  `AbilityData` values, knockback, or status-bar/indicator UI changed.

## Validation evidence

Full gate (`harness/ci.py --full`): `GATE_OK mode=full` — `UNIT_PASSED tests=119`,
`PIE_PASSED tests=5`, `UE_BUILD_OK`, `UE_AUTOMATION_OK` (`passed=1 total=1`),
`E2E_PASSED steps=1`. Diff scope reviewed and confirmed limited to
`app-source-tracked/Source/KrowdKontrol/` (EnemyBase.h/.cpp + 3 test files); no
protected files touched. Pass-1 review found two `friend class` grants in
`EnemyBase.h` despite an earlier (incorrect) claim that both were already excluded.
Only one was a genuine scope leak: the issue #219 grant
(`FKrowdKontrolTeachingPromptComponentTest`), which belongs to still-open, unmerged
PR #306, and has been removed from both `app/` and the `app-source-tracked/` mirror.
The issue #321 grant (`FKrowdKontrolPostRunSummaryNextLevelButtonTest`) was
confirmed (via `git show origin/main:.../EnemyBase.h`) to already be legitimate,
merged main content from PR #335 - this branch's fork point simply predates that
merge, which made a stale-base diff show it as newly added. It has been kept;
removing it broke the local build against the shared `app/` symlink for no benefit
at actual merge time. The `app-source-tracked/` mirror is byte-for-byte identical
to the live `app/` symlink.

## Follow-up (not blocking, flagged for playtest)

`ATrooperEnemy`'s continuous re-arm-while-in-range attack design means the new 2.5s
base timeout will cause its attack tell/light to replay roughly every 2.5s during
sustained close-range engagement, instead of firing continuously and silently
forever. This is a visible cadence change, not a balance change — flagged for a live
PIE playtest pass before merge; if it reads as janky, the fix is a one-line
`ATrooperEnemy::GetAttackDurationSeconds()` override (not a change to
`AttackTelegraphSeconds`, which is a different, in-scope-protected value).

The cadence itself is still pending that playtest call and is deliberately left
unchanged here, but it is no longer untested while it waits: `(m2)` in
`KrowdKontrolTrooperEnemyTest.cpp` now drives a Trooper through a real `Tick()` loop
past the base timeout with a live in-range player pawn and asserts it lands back in
`Attack` with the ray-fired delegate still firing repeatedly, pinning the
currently-intentional behaviour so a future change to it — the playtest-driven
override above, or an accidental regression — shows up as a named test result.

## Review follow-up (self-fix, 2026-08-27)

Addressed all findings from the code-review and test-coverage review agents:
- Added a cross-subclass safety-margin test (`(i5c)` in `KrowdKontrolEnemyBaseTest.cpp`)
  asserting `GetAttackDurationSeconds() > AttackTelegraphSeconds` for all four concrete
  enemy types, turning the prose invariant `EnemyBase.h`'s `GetAttackDurationSeconds()`
  comment documents into a machine-checked one.
- Added a real-`Tick()`-path regression test (`(i5d)`) for `TickAttackDuration`,
  mirroring the existing `(i2)`/`(i4)` pair for `TickControlledDuration` — guards
  against a future edit accidentally moving the call inside the pawn-gated block below
  it. Extended `KrowdKontrolBomberEnemyTest.cpp`'s existing real-`Tick()` case `(m)`
  with an assertion that state is still `Attack` immediately before the fire
  assertion, proving the base timeout hadn't already preempted the telegraph.
- Added the Trooper cadence pinning test described above.
- Reworded the `EnemyBase.cpp` comment that said re-entry happens "on the very next
  tick" to accurately describe it happening later in the same `Tick()` call.

Not changed: `ATrooperEnemy`'s cadence itself remains exactly as shipped — the
reviews' own recommendation was to resolve that via the already-planned live PIE
playtest, not to guess at a cap value during self-fix.

## Review follow-up (self-fix pass 2, 2026-08-27)

Addressed all findings from validation pass-1:
- Added `AEnemyBase::OnAttackExpired()` overrides to all 4 concrete enemy subclasses
  (`ABomberEnemy`/`ARunnerEnemy`/`ATrooperEnemy`/`ASniperEnemy`), each clearing
  `AttackTellLightComponent`'s intensity, mirroring the existing `OnControlledExpired()`
  pattern exactly. Without this, the Attack-duration timeout reverted `Attack ->
  Alert` but left the attack-tell light lit indefinitely (HIGH finding). Added a
  regression test per subclass (`(l-attack-expired)`) driving the real
  `TickAttackDuration()` path and asserting the tell light clears.
- Updated `EnemyBase.h`'s transition-table comment to list the new `Attack -> Alert`
  timeout edge, which the audit method the PR body itself cites had missed (MEDIUM).
- Filed the Trooper cadence change as its own follow-up issue, per issue #313's
  acceptance criteria: [#337](https://github.com/severinkehding/krowd-kontrol/issues/337)
  (MEDIUM/scope) — previously only recorded inline above.

## Operator resolution of the pass-2 escalation (2026-08-27)

**Ruling (operator, on PR #336):** repeating attacks are the intended enemy
model — the timeout re-entering Attack and re-arming each type's attack is
correct behaviour, not a regression. Fire-once-per-encounter was an artifact of
the pre-#313 freeze bug. **Full disclosure of the resulting cadence changes**
(previously only Trooper's was disclosed): B0-0MR now re-telegraphs and
re-explodes (full contact damage) every window while the player stays in range;
SN-1PR re-fires its shot delegate and replays its tell each window; RU-NNR
re-fires its drain the same way. Damage tuning, if a camped Bomber proves too
lethal in playtests, is a follow-up knob — not part of this fix.

**HIGH finding fixed — duration now derived from the telegraph:** all four
concrete types override `GetAttackDurationSeconds()` with
`max(base floor, AttackTelegraphSeconds + AttackDurationTelegraphMarginSeconds)`
(margin shared on `AEnemyBase`), so a Blueprint-tuned telegraph above the old
2.5s constant can no longer silently reintroduce the shot-suppression dead-end.
Test (i5c) now also pins the tuned case (telegraph raised above the floor at
runtime), not just C++ defaults.

**MEDIUM finding fixed — (m2) cadence assertion made real:** it now snapshots
the fire count at the expiry boundary and asserts a post-boundary increase;
the old `CallCount > 1` passed even with the cycle permanently silent.

**LOW finding fixed:** `OnAttackExpired()`'s doc no longer claims subclasses
reset fire guards there (none do — `OnAttackEntry()` owns those; the doc now
says why that split is deliberate).

Verified: clean UBT build, full unit suite 121/121 headless.
