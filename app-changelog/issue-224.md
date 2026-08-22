# Issue #224: Public read accessors for remaining/total Controlled duration on AEnemyBase

Adds two public, read-only, const accessors to `AEnemyBase`
(`app/Source/KrowdKontrol/EnemyBase.h`) — `GetRemainingControlledSeconds()` and
`GetTotalControlledSeconds()` — plus a new private `TotalControlledSeconds` field, set
alongside the existing `RemainingControlledSeconds` in `ReceiveControl()`
(`app/Source/KrowdKontrol/EnemyBase.cpp`) from the same computed override-or-base
duration value. This closes REQ-2 of the Enemy Effect Indicator PRD
(`docs/prd-enemy-effect-indicator.md`), unblocking (but not itself building) the
world-space Controlled-duration indicator tracked as a separate issue. Both accessors
follow the exact "stale read, guarded by state" documentation contract
`GetControllingAbility()` already establishes: neither field is ever reset on
reversion to Alert or on Banked, so a caller reading after either exit simply gets the
last value, never a crash or a reset-to-zero.

## Acceptance criteria

- [x] `AEnemyBase` gains `GetRemainingControlledSeconds()` and
      `GetTotalControlledSeconds()`, both public, const, read-only — no setter, no
      way to write either field from outside the class.
- [x] Both accessors documented with the same "only meaningful while
      `GetEnemyState() == Controlled`" contract `GetControllingAbility()` already
      documents.
- [x] `GetRemainingControlledSeconds() / GetTotalControlledSeconds()` == 1.0
      immediately on entering Controlled — test (g2) in
      `KrowdKontrolEnemyBaseTest.cpp`.
- [x] The fraction decreases monotonically as `TickControlledDuration` advances
      simulated time — test (g2), 3-step loop.
- [x] `GetTotalControlledSeconds()` reflects a `GetControlledDurationOverrideSeconds`
      override (Sniper+Sleep=7s) rather than the ability's base duration — test (s,
      extended) in `KrowdKontrolSniperEnemyTest.cpp`.
- [x] Reading either accessor after `OnEnemyControlledExpired` or after
      `OnEnemyBanked` does not crash and returns a stable last-known value — tests
      (i1b) and (i2b) in `KrowdKontrolEnemyBaseTest.cpp`, both exit edges.
- [x] `python3 harness/ci.py` (full mode) passes, no regressions.

## Validation evidence

Full gate (`python3 harness/ci.py`, mode=full):

```
HARNESS_START mode=full driver=cli
STATIC_SKIPPED no 'static' command in harness.config.json
UNIT_PASSED tests=82
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

Passed on the first run — no fixes required. `unit` (`KrowdKontrol.Unit.*`,
`run_ue_automation.sh`) reported `UNIT_PASSED tests=82`, covering the four new/extended
test cases: (g2) fraction=1.0-at-entry + monotonic decrease, (i1b) stale read after
banking, (i2b) stale read after expiry (all in `KrowdKontrolEnemyBaseTest.cpp`), and
the extended case (s) override assertion in `KrowdKontrolSniperEnemyTest.cpp`.

MISSION.md Hard Invariants reviewed against this diff: no kill-rule, colour-lock,
ability-roster, enemy-roster, engine/dimensionality, networking, or `app`-tracking
invariant is touched — this is a read-only accessor addition plus one new snapshot
field; no existing state-machine transition, decrement, or gating logic changed
(`TickControlledDuration` is untouched).

---

The real Unreal project stays under `app/` (gitignored, D-003) — this changelog and
its matching `app-source-tracked/` copy are the tracked-repo record of that change,
per D-009. Not a substitute for reading `app-source-tracked/` directly.
