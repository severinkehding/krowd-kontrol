# Issue #52: Mid-boss 3 — dual-zone enrage boss

New `ADualZoneBoss` (`ABossBase` subclass, first concrete one in the codebase) owns
two `EditInstanceOnly` `ATargetZone` references (`ZoneA`/`ZoneB`), marks itself
`SetIsSplit(true)` at construction, advances to `Armed` immediately in `BeginPlay()`,
and tracks each zone's banked-actor count independently. When
`abs(BankedCountA - BankedCountB)` exceeds `EnrageImbalanceThreshold` (placeholder
`EditDefaultsOnly` value, default 3), it calls the inherited `SetIsEnraged(true)`.
No un-enrage path, no Vulnerable/Banked wiring, no new enum/ability/colour/enemy
type — see `plan.md` for the full scope rationale.

## Files changed

| File | Action | What it contains |
|------|--------|-------------------|
| `Source/KrowdKontrol/DualZoneBoss.h` | CREATE | `ADualZoneBoss` declaration: `ZoneA`/`ZoneB` refs, `EnrageImbalanceThreshold`, `GetBankedCountA/B()`, handler/recheck method decls |
| `Source/KrowdKontrol/DualZoneBoss.cpp` | CREATE | Constructor (`SetIsSplit(true)`), `BeginPlay()` (binds handlers, `AdvanceToArmed()`), `HandleZoneA/BBanked`, `RecheckImbalance()` |
| `Source/KrowdKontrol/Private/Tests/KrowdKontrolDualZoneBossTest.cpp` | CREATE | `KrowdKontrol.Unit.DualZoneBoss`: split+armed at fight start, balanced banking (incl. exact-threshold boundary) never enrages, lopsided banking enrages, per-zone counters stay independent |

## Acceptance criteria

- [x] `ADualZoneBoss` exposes `ZoneA`/`ZoneB`, marks itself split via `SetIsSplit(true)` (AC #1)
- [x] Tracks per-zone banked count, enters Enrage when imbalance exceeds `EnrageImbalanceThreshold` (AC #2)
- [x] Reaches `Armed` immediately in `BeginPlay()`, well within 10s (AC #3)
- [x] Only ever defeated via `Banked` — no alternate defeat path exists (AC #4, structural)
- [x] `KrowdKontrol.Unit.DualZoneBoss` confirms Enrage triggers on lopsided banking, not on balanced banking (AC #5)
- [x] Level 1-3 validation commands pass with exit 0
- [x] Code mirrors existing patterns (`ADoorConnectorActor::RoomA/RoomB` for `EditInstanceOnly` refs, `ABossBaseTestActor` for concrete-subclass shape)
- [x] No regressions in `KrowdKontrol.Unit.BossBase` / `KrowdKontrol.Unit.TargetZone`
- [x] No new enum value, ability, colour, or enemy type introduced

## Validation evidence

```
$ python harness/ci.py
HARNESS_START mode=full driver=cli
STATIC_SKIPPED no 'static' command in harness.config.json
UNIT_PASSED tests=55
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

Test count went from 54 to 55 (one new top-level `KrowdKontrol.Unit.DualZoneBoss`
test). A single manual re-run outside `ci.py` reported one flaky failure
(`passed=54 total=55`); five subsequent consecutive re-runs plus the `ci.py` run
itself were all clean, and the failure never implicated `DualZoneBoss` by name —
read as a one-off headless-Editor boot/timing flake, not a regression. Full details
in `validation.md`.
