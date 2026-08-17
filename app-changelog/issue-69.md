# Issue #69: Gate CC ability unlocks to level progression, starting with Stun only

Adds `UAbilityUnlockComponent`, tracking per-run which of the 5 crowd-control
abilities (`EAbilitySlot`) are unlocked. A new run starts with only Stun unlocked;
`NotifyLevelReached(LevelIndex)` is the sole entry point that unlocks Sleep (level 2),
Root (level 3), Fear (level 4), and Snare (level 5), exactly once each, broadcasting
`OnAbilityUnlocked`. This resolves the issue's own flagged open question (REQ-1's "first
four levels" implying 5 total levels vs. MISSION.md's then-stated "3 hand-authored
levels" cap) using MISSION.md's operator decision of 2026-08-17, which locked the Alpha
at 5 hand-authored levels specifically to reconcile this ambiguity (MISSION.md:58-60).

The component deliberately does not wire into `UAbilityCooldownComponent` or
`UAbilityCooldownTrayWidget` — both already reserve "is this ability available" gating
for a separate, later mechanic (issue #71) — so this ships as unlock-state bookkeeping
with a public `IsAbilityUnlocked()`/`OnAbilityUnlocked` surface, ready for that later
wiring without further C++ change here. `NotifyLevelReached` takes an explicit level
index rather than depending on a level-progression subsystem, per the issue's own Notes
section (no such subsystem exists yet).

## Files changed (all under `app/`, gitignored per D-003 — this is the tracked-repo
record of that change, see the closing note below)

| File | Action | What it contains |
|------|--------|-------------------|
| `app/Source/KrowdKontrol/AbilityUnlockComponent.h` | CREATE | `UAbilityUnlockComponent`: `IsAbilityUnlocked()`, `NotifyLevelReached()`, `OnAbilityUnlocked` delegate, private `SlotUnlocked` state |
| `app/Source/KrowdKontrol/AbilityUnlockComponent.cpp` | CREATE | Constructor seeds Stun unlocked; level→ability map (2→Sleep, 3→Root, 4→Fear, 5→Snare); exactly-once unlock semantics |
| `app/Source/KrowdKontrol/Private/Tests/AbilityUnlockTestListener.h` | CREATE | Test-only `UObject` listener binding `OnAbilityUnlocked` via `AddDynamic` (dynamic multicast delegates can't bind lambdas) |
| `app/Source/KrowdKontrol/Private/Tests/AbilityUnlockTestListener.cpp` | CREATE | Listener implementation recording unlock order |
| `app/Source/KrowdKontrol/Private/Tests/KrowdKontrolAbilityUnlockSequenceTest.cpp` | CREATE | `KrowdKontrol.Unit.AbilityUnlockSequence` — run-start state, in-order unlocks for levels 2-5, exactly-once semantics, level-1/out-of-range no-ops |

## Acceptance criteria

- [x] **A new run starts with exactly Stun unlocked and castable; Sleep, Root, Fear,
      and Snare are locked until their unlock event fires.** Constructor seeds only
      `EAbilitySlot::Stun` as unlocked; test assertions (a) confirm all four others
      report locked at construction.
- [x] **Each ability's unlock event fires exactly once, driven by a level-progression
      signal, in the order above, and the ability becomes castable immediately after.**
      `NotifyLevelReached(LevelIndex)` maps level 2→Sleep, 3→Root, 4→Fear, 5→Snare,
      broadcasting `OnAbilityUnlocked` on first reach and no-op on repeat calls for an
      already-unlocked level; test assertions (b)-(f) cover in-order unlocking and
      exactly-once semantics via a repeat `NotifyLevelReached(2)` call.
- [x] **A `KrowdKontrol.Unit.AbilityUnlockSequence` Automation Framework test asserts
      only Stun is available at run start, and simulating progression through
      subsequent levels unlocks Sleep/Root/Fear/Snare in the correct order and none
      early.** Implemented in full; also covers level-1 and out-of-range
      (`NotifyLevelReached(6)`) as safe no-ops (assertion (g)).

## Validation

`python harness/ci.py` (full mode, run independently during `dark-factory-validate`):

```
HARNESS_START mode=full driver=cli
STATIC_SKIPPED no 'static' command in harness.config.json
UNIT_PASSED tests=33
APP_STARTED driver=cli
UE_AUTOMATION_RESULT passed=1 total=1
UE_AUTOMATION_OK
E2E_PASSED steps=1
HOLDOUT_ABSENT no .factory/holdout/run.py
MUTATIONS_ABSENT no harness/mutations/run.py
GATE_OK mode=full
```

`GATE_OK mode=full`; new test (`KrowdKontrol.Unit.AbilityUnlockSequence`) passes both
isolated and as part of the full suite (33 total, up from 31 pre-implementation); no
regression to any pre-existing `KrowdKontrol.Unit.*` test. Hard invariants #2 (no-kill
rule), #3 (5-colour lock), and #4 (5-ability roster, no 6th slot) checked by inspection
and confirmed not implicated / holding — see `validation.md` Phase 3 for detail.

## Notes

This component's public surface (`IsAbilityUnlocked`/`NotifyLevelReached`/
`OnAbilityUnlocked`) was sufficient for every test assertion — the plan's optional
`friend class` declaration for test access was not needed and was omitted.

The full implementation and validation record for this issue lives in
`/home/severin/.archon/workspaces/severinkehding/krowd-kontrol/artifacts/runs/adc4a4953674e06f4e91ca2a7bf85da6/implementation.md`
and `validation.md`. `app/` itself (the gitignored symlink to the real Unreal project,
CLAUDE.md's Environment section / `.factory/decisions.md` D-003) is unchanged by this
tracked copy — the files above under `app-source-tracked/` are a plain-text mirror made
at PR-creation time (D-009) so GitHub has a non-empty diff to open a PR against and
reviewers have real source to check, not a description of it.
