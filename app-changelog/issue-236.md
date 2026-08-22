# Issue #236: Add KrowdKontrol.PIE automation test group with real PIE session support

Stands up the `KrowdKontrol.PIE.*` automation test group (REQ-1 of
`docs/prd-functional-pie-tests.md`) — a tier distinct from `Smoke`/`Unit` whose tests
drive a real in-editor PIE session (real begin-play, real subsystem ticks, real PIE
map-name mangling) instead of the `CreateNewMap()`/`LoadMap()` worlds every existing
`KrowdKontrol.Unit.*` test uses, which never start play. Adds one new file,
`Private/Tests/KrowdKontrolPIESessionTest.cpp` (`app/Source/KrowdKontrol/...`, mirrored
byte-identical to `app-source-tracked/Source/KrowdKontrol/...` per D-009), containing a
single minimal test, `FKrowdKontrolPIESessionStartsTest`
(`KrowdKontrol.PIE.RealSessionStartsAndEndsCleanly`): it opens `/Game/Maps/L_Level01`
via `AutomationOpenMap`, asserts the live PIE world's map name carries the
`UEDPIE_0_` mangling prefix (proof of a genuine PIE session, per issue #223's finding),
pumps 5 engine frames, re-asserts, then ends the session via `FEndPlayMapCommand()`. No
lifecycle method is ever called directly — every assertion is driven by latent
commands reached through the real PIE session's own engine tick. This does not add any
of the PRD's three scenario tests (REQ-3) — those are tracked separately and depend on
this mechanism existing first, per the issue text.

## Acceptance criteria

- [x] A new test group distinct from `Smoke`/`Unit`, following the
      `KrowdKontrol.<Group>.<Name>` naming convention: `KrowdKontrol.PIE.<Name>`.
- [x] At least one minimal test that opens a real shipped map, starts a PIE session via
      Unreal's Automation latent commands (`AutomationOpenMap`, `FEndPlayMapCommand`,
      `FWaitForEngineFramesCommand`), pumps a handful of frames, asserts the PIE
      session is genuinely active via the `UEDPIE_0_` map-name prefix (issue #223),
      and cleanly ends the session.
- [x] No test in this group calls lifecycle functions (e.g. `OnWorldBeginPlay`,
      `RefreshLevelClearState`) directly/synchronously — all lifecycle behavior is
      driven by the real engine tick during the PIE session, via
      `FKrowdKontrolAssertPIESessionActiveCommand`'s latent `Update()`.
- [x] `harness/run_ue_automation.sh` can select and run `KrowdKontrol.PIE.*` headlessly
      via its existing `<TestNameFilter>` argument — no script change needed, verified
      directly (see Validation evidence).
- [x] `-nullrhi` compatibility checked: starting a PIE session did not in practice need
      the issue's reserved rendering fallback, so `run_ue_automation.sh`'s `-nullrhi`
      line and its "future Screenshot.* group" comment were left untouched — the
      reserved filter-branch hook remains available but unused for this group.

## Validation evidence

Targeted run, confirms the new group in isolation:

```
$ KROWD_KONTROL_SKIP_UBT=1 harness/run_ue_automation.sh "KrowdKontrol.PIE."
UE_AUTOMATION_RESULT passed=1 total=1
UE_AUTOMATION_OK
```

Full gate (`python harness/ci.py`, mode=full), run twice for determinism:

```
UE_BUILD_OK
UE_AUTOMATION_RESULT passed=92 total=93
UE_AUTOMATION_FAILED KrowdKontrol.Unit.PlayerControllerShowsMouseCursor: state=Fail
GATE_FAILED: unit
```

The one failure, `KrowdKontrol.Unit.PlayerControllerShowsMouseCursor`, is confirmed
pre-existing and unrelated to this diff: it belongs to the not-yet-landed cursor/aiming
foundation feature (tracked separately as issue #262), asserting
`Controller->bShowMouseCursor` is true after `BeginPlay()` — no call site anywhere in
the live `app/` copy of `KrowdKontrolPlayerController.{h,cpp}` sets that field yet. This
issue's diff (one new file) never touches `bShowMouseCursor`,
`KrowdKontrolCursorWorldPositionTest.cpp`, or `KrowdKontrolPlayerController.*`, and the
same failure was independently confirmed pre-existing in two other validation runs the
same day. Per the validation workflow's own pre-existing-failure exception, this is
noted rather than "fixed" — doing so would mean building the unrelated cursor/aiming
feature out of scope for #236.

MISSION.md Hard Invariants reviewed against this diff: none apply — no enemy roster,
kill-rule, colour-lock, ability, or networking logic is touched. Invariant #8 (`app/`
untracked, `app-source-tracked/` as a plain-text mirror only) is satisfied by
construction: the new file is a `.cpp` under `Source/`, not an asset.

---

The real Unreal project stays under `app/` (gitignored, D-003) — this changelog and
its matching `app-source-tracked/` copy are the tracked-repo record of that change,
per D-009. Not a substitute for reading `app-source-tracked/` directly.
