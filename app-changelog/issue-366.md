# Issue #366 — Banking-radius ring honesty regression test

Adds `KrowdKontrol.Unit.BankingRadiusIndicatorHonesty`, a regression test proving the
pylon ground-ring's rendered radius is genuinely derived from `ATargetZone::
GetBankingRadiusUnits()` rather than a hardcoded constant that happens to match. The
two existing tests touching this code path (`KrowdKontrolPlaceholderTargetZoneActorTest.cpp`,
`KrowdKontrolRoomActorBankingWiringTest.cpp`) both use single-radius scenarios, so
neither could catch a hardcoded value. No production code changes.

## Acceptance criteria

- [x] New test file under `Source/KrowdKontrol/Private/Tests/`, `#if
      WITH_DEV_AUTOMATION_TESTS`-guarded, named `KrowdKontrol.Unit.BankingRadiusIndicatorHonesty`
- [x] Test reads the ground-ring's actual configured radius on a spawned
      `APlaceholderTargetZoneActor` and compares it against `ATargetZone::GetBankingRadiusUnits()`
- [x] Constructs two differently-configured zone instances (150.0f default, 300.0f
      resized) and asserts the ring tracks each — including an explicit inequality
      assertion proving the two values actually differ
- [x] `harness/run_ue_automation.sh KrowdKontrol.Unit.BankingRadiusIndicatorHonesty` passes
- [x] Full unit rung (`harness/run_ue_automation.sh KrowdKontrol.Unit.`) passes with no
      regressions
- [x] No production code (`TargetZone.h/.cpp`, `PlaceholderTargetZoneActor.h/.cpp`,
      `RoomActor.h/.cpp`) modified

## Validation evidence

`python harness/ci.py` (full mode):

```
HARNESS_START mode=full driver=cli
STATIC_SKIPPED no 'static' command in harness.config.json
UNIT_PASSED tests=132
PIE_PASSED tests=8
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

No protected files touched. Diff scope is a single new file:
`app-source-tracked/Source/KrowdKontrol/Private/Tests/KrowdKontrolBankingRadiusIndicatorHonestyTest.cpp`
(mirror of the identical file under `app/`).
