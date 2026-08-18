# Issue #37: Onboarding — instrument ability-vs-enemy matchup usage signal

Adds a read-only instrumentation signal that fires every time the player lands a
successful crowd-control ability cast, classifying whether the ability was
colour-matched to the target enemy's type (per MISSION.md Hard Invariant 3's locked
5-colour channel). This is the detection half of PRD 09 REQ-5 / PRD 02 REQ-4's
onboarding "additional help" system — a later, separate issue will consume this signal
to actually nudge the player. New `UAbilityMatchupSignalComponent` subscribes to the
existing `UAbilityCastComponent::OnAbilityCastApplied` delegate, exactly as
`UGizmoFirstContactComponent`/`UAbilityCastVFXComponent` already do, classifies the
cast using a new `AbilityData::CounteredEnemyType` field, and broadcasts its own
`FOnAbilityMatchupSignal` delegate. No UI, no nudge logic, no new gameplay behavior.

## Files changed (all under `app/`, gitignored per D-003 — this is the tracked-repo
record of that change)

| File | Action | What it contains |
|------|--------|-------------------|
| `app/Source/KrowdKontrol/AbilityData.h` | UPDATE | `#include "EnemyType.h"` + new `EEnemyType CounteredEnemyType` field on `FAbilityData`; updated `bIsColourNeutral`'s stale comment that used to claim no enemy-type field existed anywhere |
| `app/Source/KrowdKontrol/AbilityData.cpp` | UPDATE | `CounteredEnemyType` set in `GetSleep()`→SN_1PR, `GetRoot()`→TR_UPR, `GetFear()`→B0_0MR, `GetSnare()`→RU_NNR, per `ReservedGameplayColours.h`'s documented pairing; `GetStun()` left untouched (default-inits, inert since `bIsColourNeutral == true`) |
| `app/Source/KrowdKontrol/AbilityMatchupSignalComponent.h` | CREATE | `FOnAbilityMatchupSignal` 3-param dynamic multicast delegate + `UAbilityMatchupSignalComponent` declaration, mirroring `UGizmoFirstContactComponent`'s shape |
| `app/Source/KrowdKontrol/AbilityMatchupSignalComponent.cpp` | CREATE | Classification logic: resolves the target's `EEnemyType` via `FindComponentByClass<UEnemyTypeIndicatorComponent>()`, compares to `AbilityData::Get(Ability).CounteredEnemyType` (short-circuited by `bIsColourNeutral`), broadcasts. Missing-indicator targets degrade safely (one-shot warning, no broadcast) rather than guessing |
| `app/Source/KrowdKontrol/FlatCamera3DPrototypePawn.h` | UPDATE | Forward-declares `UAbilityMatchupSignalComponent`, adds a `TObjectPtr<UAbilityMatchupSignalComponent>` property |
| `app/Source/KrowdKontrol/FlatCamera3DPrototypePawn.cpp` | UPDATE | `#include`, `CreateDefaultSubobject`, `OnAbilityCastApplied.AddDynamic(...)` — same two-line wiring pattern as the pawn's other `OnAbilityCastApplied` subscribers |
| `app/Source/KrowdKontrol/EnemyBase.h` | UPDATE | Added `friend class FKrowdKontrolAbilityMatchupSignalComponentTest;` to the existing per-test-class friend grant list, required for the new test's `TickCheckDetection()` calls to compile (not called out in the plan's own Files-to-Change list — see implementation.md's Deviations section) |
| `app/Source/KrowdKontrol/Private/Tests/AbilityMatchupSignalTestListener.h` / `.cpp` | CREATE | Dynamic-delegate listener for `FOnAbilityMatchupSignal`, mirroring `AbilityCastAppliedTestListener.h`'s shape |
| `app/Source/KrowdKontrol/Private/Tests/KrowdKontrolAbilityMatchupSignalComponentTest.cpp` | CREATE | `KrowdKontrol.Unit.AbilityMatchupSignalComponent` — 5 cases: matched cast, mismatched cast, Stun-never-matched, missing-indicator defensive, real-pawn constructor wiring |
| `app/Source/KrowdKontrol/Private/Tests/KrowdKontrolAbilityDataTest.cpp` | UPDATE | Added `CounteredEnemyType` assertions for Sleep/Root/Fear/Snare against the locked pairing |

Note: `app/`'s live `FlatCamera3DPrototypePawn.h`/`.cpp` and `EnemyBase.h` also carry
other, unrelated, concurrently-in-flight tasks' edits at the time this mirror was made
(issue #29's `FirstStunBeaconComponent` wiring; issue #48's
`FKrowdKontrolSleepShieldBossTest` friend grant) — `app/` is a single shared,
non-git-tracked symlink every factory worktree writes through (D-003). Only this
issue's own documented change is included in the mirror above.

## Acceptance criteria

- [x] **A signal fires each time the player casts an ability on an enemy, indicating
      whether the ability was colour-matched to that enemy type** — `KrowdKontrol.Unit.AbilityMatchupSignalComponent` cases (a)/(b)/(c) cover matched, mismatched, and the Stun-is-never-matched invariant.
- [x] **The signal is observable via a `BlueprintAssignable` delegate that other systems can subscribe to; no UI/player-facing response implemented** — `OnAbilityMatchupSignal` is `UPROPERTY(BlueprintAssignable, ...)`; no UI, no nudge logic added.
- [x] **`KrowdKontrol.Unit.AbilityMatchupSignalComponent` confirms the signal fires correctly for both a matched and a mismatched case** — cases (a)/(b), plus (c)/(d)/(e) for the neutral-ability, missing-indicator, and real-pawn-wiring edge cases.
- [x] No regressions in `KrowdKontrol.Unit.AbilityCastComponent`, `KrowdKontrol.Unit.AbilityData`, `KrowdKontrol.Unit.GizmoFirstContactComponent`, or any other pre-existing test.

## Validation

`harness/ci.py` full mode: `GATE_OK` (`STATIC_SKIPPED`, `UNIT_PASSED tests=58`,
`UE_AUTOMATION_OK passed=1 total=1`, `E2E_PASSED steps=1`). No fixes needed, gate
passed clean on the first run. Hard invariants #2–#5 verified by inspection (ability
roster stays at 5 with Stun still colour-neutral; enemy roster stays at 4 types; no new
UI/colour usage; no kill/damage logic touched). Known pre-existing gap, not introduced
by this issue: `ATrooperEnemy` has no `UEnemyTypeIndicatorComponent`, so live TR-UPR
casts on a Trooper won't classify until a separate follow-up adds it (documented in
`plan.md`'s Risks section).

---

Source lives under `app/` (gitignored, D-003) — this file is the tracked-repo record
of that change, not a substitute for reading the actual code.
