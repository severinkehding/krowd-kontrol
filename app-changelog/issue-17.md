# Issue #17: SN-1PR sniper enemy (long-range attack, Sleep weakness telegraph)

Verify-and-mirror issue, not a behavior change. `ASniperEnemy` — the first concrete
core enemy type (PRD 03, MISSION.md Hard Invariant 5), extending `AEnemyBase` (issue
#12) — already exists, is already correct, and already works in `app/`. It was first
built alongside #12 in the combined PR #115, which passed the full validation gate
(`GATE_OK`, 27/27 unit tests) but was rejected solely for exceeding the 500-line PR
cap (898 lines total). This PR does not change `ASniperEnemy`'s behavior at all — it
mirrors the already-correct, already-validated `app/` source into `app-source-tracked/`
for the first time, stacked on #12's PR since that base class is a hard dependency.

## Files changed

| File | Action | Contains |
|------|--------|----------|
| `SniperEnemy.h` | CREATE | `ASniperEnemy : AEnemyBase` declaration — mesh, eye-glow/attack-tell light components, telegraph state |
| `SniperEnemy.cpp` | CREATE | Cone silhouette construction, Blue eye-glow, attack-tell timer, shot-fired delegate |
| `Private/Tests/SniperShotFiredTestListener.h/.cpp` | CREATE | Dynamic-delegate listener for `OnSniperShotFired` |
| `Private/Tests/KrowdKontrolSniperEnemyTest.cpp` | CREATE | `KrowdKontrol.Unit.SniperEnemy`, 12 cases (a)-(l) |

## Acceptance criteria

- [x] **Extends the base Enemy state machine.** `ASniperEnemy : public AEnemyBase`
      (`SniperEnemy.h:21`), depends on issue #12.
- [x] **Distinct silhouette, shape-only readable.** Cone mesh, scaled `(1, 1, 1.8)`
      (`SniperEnemy.cpp:14-25`) — distinct from the other placeholder shapes already
      in this module (cube, cylinder).
- [x] **Visible attack tell precedes the shot, distinct from other enemies.**
      `AttackTellLightComponent` turns on at `OnAttackEntry()` and counts down via
      `AdvanceAttackTelegraph` (`SniperEnemy.cpp:77-100`); explicitly a non-reserved
      placeholder colour pending art direction (see Notes).
- [x] **Movement/attack matches the PRD's long-range sniper behavior.**
      `GetAttackRangeUnits()` returns 1400.0f, close to the inherited 1500.0f
      `DetectionRangeUnits`, so it enters Attack almost immediately after Alert
      without closing distance (`SniperEnemy.cpp:48-57`) — the mechanical definition
      of "long-range" this state machine uses.
- [x] **Sleep-specific glow response; other abilities produce none (REQ-3).**
      `OnControlledEntry` only intensifies `EyeGlowLightComponent` when
      `Ability == EAbilitySlot::Sleep`, and unconditionally clears any in-progress
      attack tell regardless of which ability triggered Controlled
      (`SniperEnemy.cpp:59-75`). Negative case (other abilities: no glow change)
      covered by `KrowdKontrol.Unit.SniperEnemy` case (d).
- [x] **Blue used only as the locked information colour.**
      `EyeGlowLightComponent->SetLightColor(ReservedGameplayColours::GetBlue())`
      (`SniperEnemy.cpp:31`) — the only colour use on this actor drawn from the
      reserved set; `AttackTellLightComponent`'s colour is a separate, explicitly
      non-reserved placeholder (see Notes).
- [x] **No Sleep-ability logic of its own.** `ASniperEnemy` only reacts to
      `ReceiveControl`/`OnControlledEntry` being called by the (not-yet-built)
      ability-cast system — it never implements Sleep's own effect.

## Validation evidence

Already-validated: PR #115's full gate run (`harness/ci.py`, full mode) reported
`GATE_OK` with `UNIT_PASSED tests=27`, including `KrowdKontrol.Unit.SniperEnemy`, and
all applicable MISSION.md hard invariants verified (Hard Invariant 3: Blue used only
via `ReservedGameplayColours::GetBlue()`; Hard Invariant 5: SN-1PR is one of the 4
locked core enemy types). This PR mirrors the identical, unmodified source; re-run the
full gate before merge.

```
$ python3 harness/ci.py
HARNESS_START mode=full driver=cli
STATIC_SKIPPED no 'static' command in harness.config.json
UNIT_PASSED tests=27
APP_STARTED driver=cli
UE_AUTOMATION_RESULT passed=1 total=1
UE_AUTOMATION_OK
E2E_PASSED steps=1
HOLDOUT_ABSENT no .factory/holdout/run.py
MUTATIONS_ABSENT no harness/mutations/run.py
GATE_OK mode=full
```

## Deviations from plan

None — mirrors `app/` verbatim, split from the originally-combined #12+#17 attempt.
Depends on issue #12's PR (base class); see that PR for `AEnemyBase`'s own AC mapping.

## Notes carried over from PR #115

- `AttackTellLightComponent`'s colour (`FLinearColor(1.0f, 0.85f, 0.1f)`) is a
  deliberate placeholder — not one of MISSION.md's 5 reserved gameplay-information
  colours — pending a human art-direction ruling, same caveat pattern as
  `APlaceholderTargetZoneActor`'s beacon colour. Not this PR's decision to make.
- Does not implement the Sleep ability itself; only reacts to `ReceiveControl` being
  called by a future ability-cast system that doesn't exist yet.
