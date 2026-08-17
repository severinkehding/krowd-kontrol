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
| `app/Source/KrowdKontrol/FlatCamera3DPrototypePawn.h` | MODIFY | Adds `AbilityUnlockComponent` subobject property (fix pass, see below) |
| `app/Source/KrowdKontrol/FlatCamera3DPrototypePawn.cpp` | MODIFY | Constructs `UAbilityUnlockComponent` on the pawn (fix pass, see below) |

## Acceptance criteria

- [x] **A new run starts with exactly Stun unlocked; Sleep, Root, Fear, and Snare are
      locked until their unlock event fires.** Constructor seeds only
      `EAbilitySlot::Stun` as unlocked; test assertions (a) confirm all four others
      report locked at construction.
- [x] **Each ability's unlock event fires exactly once, driven by a level-progression
      signal, in the order above.** `NotifyLevelReached(LevelIndex)` maps level
      2→Sleep, 3→Root, 4→Fear, 5→Snare, broadcasting `OnAbilityUnlocked` on first
      reach and no-op on repeat calls for an already-unlocked level; test assertions
      (b)-(f) cover in-order unlocking and exactly-once semantics via a repeat
      `NotifyLevelReached(2)` call.
- [ ] **"Castable" / hidden-as-active-tray-slot until unlocked.** NOT implemented.
      There is no cast-execution path anywhere in this codebase yet — `TryStartCooldown`
      (the documented future cast-gating point) has zero production callers, and
      `UAbilityCooldownTrayWidget` is never added to any viewport in production code;
      both are explicitly reserved placeholders for issue #71. Wiring unlock state into
      either would mean building issue #71's cast-execution system inside this issue,
      and `UAbilityCooldownComponent.h` explicitly forbids adding new public mutators to
      preserve that separation. Deferred to issue #71, which owns cast execution and
      tray gating.
- [x] **A `KrowdKontrol.Unit.AbilityUnlockSequence` Automation Framework test asserts
      only Stun is available at run start, and simulating progression through
      subsequent levels unlocks Sleep/Root/Fear/Snare in the correct order and none
      early.** Implemented in full; also covers level-1 and out-of-range
      (`NotifyLevelReached(6)`) as safe no-ops (assertion (g)).
- [x] **The unlock system is reachable during real play.** Fix pass: attached
      `UAbilityUnlockComponent` to `AFlatCamera3DPrototypePawn`, the only pawn placed
      in the project's actual playable level (`L_FlatCamera3DPrototype`).

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

### Fix pass (validation pass-1 feedback)

Addressed: attached `UAbilityUnlockComponent` to `AFlatCamera3DPrototypePawn` so the
system is reachable in the project's only playable level (previously the pawn had no
unlock tracking attached at all); corrected `NotifyLevelReached`'s header comment,
which claimed both level 1 and out-of-range levels were silent no-ops when only level
1 actually suppresses the warning; corrected this changelog's acceptance-criteria
checkboxes, which had marked "castable"/tray-visibility as done when only unlock-state
bookkeeping was implemented.

Not addressed: wiring cast-permission checks and ability-tray slot visibility to
unlock state. There is no cast-execution system in this codebase to wire into —
confirmed by inspection, `UAbilityCooldownComponent::TryStartCooldown` (the documented
"a future cast system gates its actual cast on" method) has zero production callers,
and `UAbilityCooldownTrayWidget` is never added to any viewport outside its own unit
test. Both classes' own header comments explicitly reserve this wiring for issue #71,
and `UAbilityCooldownComponent.h` explicitly forbids adding new public mutators to
that class specifically to keep unlock/lockout gating out of it. Building the missing
cast-execution and tray-integration systems to satisfy this would mean implementing
issue #71 inside a fix pass for issue #69, contradicting this codebase's own prior,
reviewed architecture — left for a human to decide whether to fold into #71 or rescope
#69.

The full implementation and validation record for this issue lives in
`/home/severin/.archon/workspaces/severinkehding/krowd-kontrol/artifacts/runs/adc4a4953674e06f4e91ca2a7bf85da6/implementation.md`
and `validation.md`. `app/` itself (the gitignored symlink to the real Unreal project,
CLAUDE.md's Environment section / `.factory/decisions.md` D-003) is unchanged by this
tracked copy — the files above under `app-source-tracked/` are a plain-text mirror made
at PR-creation time (D-009) so GitHub has a non-empty diff to open a PR against and
reviewers have real source to check, not a description of it.
