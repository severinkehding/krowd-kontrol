# Issue #83: Add optional obstacle-routing metadata to ATargetZone

Extends `ATargetZone` (`app/Source/KrowdKontrol/TargetZone.h`) with two metadata-only
fields — `bool bRequiresRouting` (`EditDefaultsOnly`, default `false`) and
`TObjectPtr<AActor> RoutingObstacleActor` (`EditInstanceOnly`, default `nullptr`) — so
level designers can flag a target zone as requiring the player to route an enemy
around an obstacle rather than reach it via a straight line (PRD 01 REQ-5, P1). This
is level-authoring metadata only: no AI/pathfinding logic reads or enforces either
field in this issue, and `OnActorBanked`'s existing firing behavior is provably
unchanged.

## Acceptance criteria

- [x] `bRequiresRouting` (`EditDefaultsOnly bool`, default `false`) and
      `RoutingObstacleActor` (`EditInstanceOnly TObjectPtr<AActor>`, default
      `nullptr`) compile onto `ATargetZone` in both `app/` and `app-source-tracked/`.
      Confirmed via `diff` between the two trees (no output) and a real
      `UnrealBuildTool` invocation.
- [x] `OnActorBanked`'s existing firing conditions are unchanged — no edit to
      `TargetZone.cpp`'s `HandleZoneOverlap()` or constructor; only `TargetZone.h`
      gained two new inline-initialized fields.
- [x] `KrowdKontrol.Unit.TargetZoneRouting` exists, compiles, and passes, confirming
      both new fields are settable/gettable and that setting them does not change
      `OnActorBanked`'s firing conditions (matched-actor-fires-once,
      mismatched-actor-never-fires, both re-run with the new fields set).
- [x] `app/` and `app-source-tracked/` copies of both changed/new files are identical
      (verified via `diff`, re-confirmed at PR-creation time).
- [x] `python harness/ci.py --quick` / full `harness/ci.py` report `GATE_OK` with the
      unit test count incremented by 1 versus baseline (`UNIT_PASSED tests=50`).
- [x] No regressions in `KrowdKontrol.Unit.TargetZone` (the existing, unmodified
      test) — re-run alongside the new test, `UE_AUTOMATION_RESULT passed=2 total=2`.

## Validation evidence

Full gate (`python harness/ci.py`, mode=full):

```
HARNESS_START mode=full driver=cli
STATIC_SKIPPED no 'static' command in harness.config.json
UNIT_PASSED tests=50
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

Passed on the first run — no fixes required. Targeted filter runs during
implementation additionally confirmed `KrowdKontrol.Unit.TargetZoneRouting`
(`passed=1 total=1`) and no regression in the pre-existing `KrowdKontrol.Unit.TargetZone`
test (`passed=2 total=2` when both are run together).

MISSION.md Hard Invariants reviewed against this diff: no kill-rule, colour-lock,
ability-roster, enemy-roster, engine/dimensionality, networking, or `app`-tracking
invariant is touched — the two new fields are level-authoring metadata only, unread by
any runtime logic.

---

The real Unreal project stays under `app/` (gitignored, D-003) — this changelog and
its matching `app-source-tracked/` copy are the tracked-repo record of that change,
per D-009. Not a substitute for reading `app-source-tracked/` directly.
