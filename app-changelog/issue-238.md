# Issue #238: Add KrowdKontrol.PIE.LifecycleLiveFire scenario test

Adds `KrowdKontrol.PIE.LifecycleLiveFire`, the third and final REQ-3 scenario test from
`docs/prd-functional-pie-tests.md` and issue #234's acceptance test. One new file,
`Private/Tests/KrowdKontrolPIELifecycleLiveFireTest.cpp` (`app/Source/KrowdKontrol/...`,
mirrored byte-identical to `app-source-tracked/Source/KrowdKontrol/...` per D-009).

The test opens `L_Level01` in a real PIE session, asserts real level-begin fired
(`ULevelLifecycleSubsystem::HasLevelBegun()`), binds a `ULevelLifecycleTestListener` to
the live subsystem's `OnLevelClear` delegate, then drives every enemy in the level to
`Banked` through real state transitions only: a stateful latent command
(`FKrowdKontrolDriveAllEnemiesToBankedCommand`) teleports the player pawn into each room
in turn, waits out that room's real ~3s first-entry activation countdown
(`ARoomActor::IsRoomActivated()`), waits for each owned enemy to reach `Alert`/`Attack`
via real per-frame detection, calls `ReceiveControl(EAbilitySlot::Stun)`, then sweeps the
enemy onto its type-matched `ATargetZone` (resolved per-enemy via
`UEnemyTypeIndicatorComponent`, never a fixed room->zone assumption) to trigger a real
physics overlap and reach `Banked`. Once all enemies are `Banked`, the test polls for
`OnLevelClear` to fire and asserts
`Saved/SaveGames/KrowdKontrol_LevelClearTimes.sav` exists with a recorded clear time
(`GetBestClearTimeSeconds()` keyed on the real PIE-mangled map name).

Issue #234 (the underlying `OnLevelClear` live-fire bug this test pins) is already fixed
and merged (PR #288, `IsTickableWhenPaused()` override + diagnostic logging) — this test
was expected to **pass** on first write, not merely to be authored red, and it does.

## Acceptance criteria

- [x] Test lives in the `KrowdKontrol.PIE.` group, named
      `KrowdKontrol.PIE.LifecycleLiveFire`.
- [x] Opens `L_Level01`, starts a real PIE session, and asserts level-begin observably
      fired via `ULevelLifecycleSubsystem::HasLevelBegun()` (not a direct call).
- [x] Drives every enemy in the level (6 on L_Level01) to `Banked` through real state
      transitions only: player-pawn teleport -> real `Tick()`-driven detection ->
      `ReceiveControl()` -> swept teleport onto the type-matched `ATargetZone` -> real
      overlap event.
- [x] Pumps ticks via latent commands, then asserts `OnLevelClear` fires (via a bound
      `ULevelLifecycleTestListener`) and that
      `Saved/SaveGames/KrowdKontrol_LevelClearTimes.sav` exists
      (`UGameplayStatics::DoesSaveGameExist`) with a recorded time
      (`GetBestClearTimeSeconds() > 0`).
- [x] Test never calls `OnWorldBeginPlay()`, `RefreshLevelClearState()`,
      `TickCheckDetection()`, or `TickChaseMovement()` directly — every state change is
      reached only via the real engine tick during the PIE session.
- [x] `app/` and `app-source-tracked/` copies exist and are byte-identical (`diff`
      clean).
- [x] This changelog created.
- [x] No production (non-test) code changed.

**Not part of this change (explicitly out of scope, see the plan's "NOT Building"
section):** REQ-2 (wiring `KrowdKontrol.PIE.*` into `harness/ci.py`'s ladder) remains
open, separate scope — this test is validated below via the same targeted-filter
pattern every prior `KrowdKontrol.PIE.*` issue has used, not via `ci.py`'s automated
`unit` rung.

## Validation evidence

Targeted `KrowdKontrol.PIE.` run, editor build + execution, live on the Windows-host
`UnrealEditor-Cmd.exe` via `harness/run_ue_automation.sh`:

```
$ harness/run_ue_automation.sh "KrowdKontrol.PIE."
UE_BUILD_START KrowdKontrolEditor Win64 Development
UE_BUILD_OK
UE_AUTOMATION_RESULT passed=3 total=5
UE_AUTOMATION_FAILED KrowdKontrol.PIE.SerializedPlacedActorHealth.L_Level01: state=Fail
UE_AUTOMATION_FAILED KrowdKontrol.PIE.SerializedPlacedActorHealth.L_Level02: state=Fail
```

`KrowdKontrol.PIE.LifecycleLiveFire` itself passed (`Test Completed. Result={Success}
Name={LifecycleLiveFire}` in `app/Saved/Logs/KrowdKontrol.log`), alongside the two other
pre-existing tests (`DefeatRestartRoundTrip`, `RealSessionStartsAndEndsCleanly`). The two
failures are **pre-existing and unrelated** to this change: they are the exact,
already-tracked issue #292 regression documented in `app-changelog/issue-239.md` (the
immediately preceding merged PR, #293) — a marker-actor-at-world-origin self-heal bug in
`APlaceholderTargetZoneActor`/`ARoomActor`, nothing this diff touches. Confirmed by log
order: both failures occur and complete *before* `LifecycleLiveFire` is even started, so
this test cannot be leaking state into them, and re-running with
`KROWD_KONTROL_SKIP_UBT=1` reproduces the identical pass/fail split deterministically
(not a flake).

Light inline gate (`python harness/ci.py --quick`), which exercises the real
`KrowdKontrol.Unit.*` rung (this change adds no new `KrowdKontrol.Unit.*` tests, per the
PRD's tier-separation rule):

```
$ python harness/ci.py --quick
HARNESS_START mode=quick driver=cli
STATIC_SKIPPED no 'static' command in harness.config.json
UNIT_PASSED tests=102
GATE_OK mode=quick
```

MISSION.md Hard Invariants reviewed against this diff: none apply — no enemy roster,
kill-rule, colour-lock, ability, or networking logic is touched; this is a test-only
addition. Invariant #8 (`app/` untracked, `app-source-tracked/` as a plain-text mirror
only) is satisfied by construction: the new file is a `.cpp` under `Source/`, not an
asset.

---

The real Unreal project stays under `app/` (gitignored, D-003) — this changelog and
its matching `app-source-tracked/` copy are the tracked-repo record of that change,
per D-009. Not a substitute for reading `app-source-tracked/` directly.
