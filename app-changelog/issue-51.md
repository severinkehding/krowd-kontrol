# Issue #51: Build room-pool + procedural connector shuffler system

Adds `URoomPoolShufflerComponent`, a placeable `UActorComponent` (same "reusable
component, no opinion on when/where it's triggered" pattern as
`URoomEnemyBudgetController`/`UWaveSpawnerComponent`) implementing the P1 room-pool
shuffler from PRD `05-level-design-and-progression.md` (REQ-4/REQ-6). Given a pool of
hand-authored `ARoomActor` instances (each optionally tagged with a
`URoomMetadataComponent`) and a target `ERoomDifficultyTier`, `ShuffleRooms(RoomPool,
TargetTier, Seed)` filters the pool to exact tier matches, shuffles the result with a
seeded Fisher-Yates (`FRandomStream(Seed)` — deterministic per seed), and spawns a
linear chain of `ADoorConnectorActor` instances connecting each consecutive pair,
mirroring the topology the hand-authored Alpha levels already use. Ability-gating
(REQ-5) is explicitly out of scope per the issue's own Notes — `RequiredAbility` is
never read.

## Files changed

All paths are under `app/` (gitignored per D-003) — this table and the matching
`app-source-tracked/` copy are the tracked-repo record of that change; see the
closing note below.

| File | Action | What it contains |
|------|--------|-------------------|
| `app/Source/KrowdKontrol/RoomPoolShufflerComponent.h` | CREATE | `URoomPoolShufflerComponent` public interface: `ShuffleRooms()`, `GetSpawnedDoors()` |
| `app/Source/KrowdKontrol/RoomPoolShufflerComponent.cpp` | CREATE | Filter (by `DifficultyTier` match) → seeded Fisher-Yates shuffle → door-chain spawn implementation |
| `app/Source/KrowdKontrol/Private/Tests/KrowdKontrolRoomPoolShufflerComponentTest.cpp` | CREATE | `KrowdKontrol.Unit.RoomPoolShufflerComponent` — covers tier filtering (including untagged-room exclusion), connected door-chain wiring, same-seed reproducibility, and different-seed divergence |

## Acceptance criteria

- [x] **Filters a room pool to those tagged `TargetTier` and sequences them into a
      connected layout using spawned `ADoorConnectorActor` instances.** Test case (a)
      confirms only the 6 Easy-tagged rooms are returned out of an 8-room pool (2
      Hard-tagged + 1 untagged excluded); test case (b) confirms 5 spawned doors chain
      the returned 6-room sequence in order, each `ConnectsValidRooms()`.
- [x] **Runs the shuffler twice with different seeds over the same pool/tier and
      asserts the resulting room orderings differ.** Test case (d): `Seed=1` vs.
      `Seed=987654321` over the same 6-room Easy pool produce different orderings
      (asserted index-by-index) while containing the identical room set. Test case (c)
      separately confirms same-seed reproducibility.
- [x] **No new room/tile/prop art assets introduced or required.** The shuffler only
      reorders/reconnects existing `ARoomActor` instances handed to it.
- [x] **`URoomMetadataComponent::RequiredAbility` never read.** Ability-gating (REQ-5)
      stays out of scope, tracked separately per the issue's Notes.

## Validation

```
$ python harness/ci.py         # full mode
HARNESS_START mode=full driver=cli
STATIC_SKIPPED no 'static' command in harness.config.json
UNIT_PASSED tests=51
APP_STARTED driver=cli
UE_BUILD_START KrowdKontrolEditor Win64 Development
UE_BUILD_OK
UE_AUTOMATION_RESULT passed=1 total=1
UE_AUTOMATION_OK
E2E_PASSED steps=1
HOLDOUT_ABSENT no .factory/holdout/run.py
MUTATIONS_ABSENT no harness/mutations/run.py
GATE_OK mode=full

$ bash harness/run_ue_automation.sh "KrowdKontrol.Unit."   # full unit suite, independently re-run
UE_AUTOMATION_RESULT passed=51 total=51

$ bash harness/run_ue_automation.sh "KrowdKontrol.Unit.RoomPoolShufflerComponent"   # isolated
UE_BUILD_OK
UE_AUTOMATION_RESULT passed=1 total=1
UE_AUTOMATION_OK
```

No regressions in any pre-existing `KrowdKontrol.Unit.*` test (51/51 passed, both
before and after — additive-only change). No Hard Invariant from `MISSION.md` is
touched by this change.

## Post-review fix (self-fix, 2026-08-18)

Code review and test-coverage agents converged on a HIGH bug: `ShuffleRooms()`
documented that repeat calls "reset any doors spawned by a previous call," but the
implementation only cleared the `SpawnedDoors` tracking array, never calling
`Destroy()` on the actors themselves — every second call on the same component
instance leaked the previous call's `ADoorConnectorActor` chain into the level.
Fixed by destroying each tracked door before clearing the array
(`RoomPoolShufflerComponent.cpp`). Added regression coverage in
`KrowdKontrolRoomPoolShufflerComponentTest.cpp`:
- (e) live-door-count assertion after a repeat `ShuffleRooms()` call, catching the
  leak directly.
- (f)/(g) zero-match (`Medium` tier, no tagged rooms) and single-match (one-room
  pool) boundary cases for the door-chain spawn loop (MEDIUM finding).
- A `nullptr` entry added to the test pool, exercising the filter loop's existing
  null-guard (LOW finding).

Two LOW docs-impact findings (missing `app-source-tracked/`/`app-changelog/` Repo
Layout entries, and the still-TBD Conventions section) target `CLAUDE.md`, a
protected path — left as standing follow-up for a human editor, not fixed here.

Re-validated: `harness/ci.py` full mode → `GATE_OK`; isolated component test →
`UE_AUTOMATION_RESULT passed=1 total=1`; full unit suite → `passed=51 total=51`.

---

The real Unreal project stays under `app/` (gitignored, D-003) — this changelog and
its matching `app-source-tracked/` copy are the tracked-repo record of that change,
per D-009. Not a substitute for reading `app-source-tracked/` directly.
