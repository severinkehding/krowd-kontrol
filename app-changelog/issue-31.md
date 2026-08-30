# Issue #31: Onboarding — forced-safe solo Sniper encounter on Level 2's Sleep unlock

Issue #31 asks that each of the four remaining abilities (Sleep, Root, Fear, Snare)
get a forced-safe solo encounter with its colour-matched enemy type immediately after
that ability unlocks, before that enemy type ever appears mixed into a crowd. This PR
implements **Sleep only** (Level 2 → Sniper) — the first/earliest of the four unlocks
— per issue #31's own Notes clause pre-authorizing a per-ability split if the full
scope would exceed FACTORY_RULES.md's 500-line PR cap. Root (Level 3 → Trooper), Fear
(Level 4 → Bomber), and Snare (Level 5 → Runner) are deferred to three follow-up
issues (see "Deferred scope" below).

This is a level-content placement problem, not a runtime one: Sleep unlocks the
instant the player arrives in Level 2 (`AbilityUnlockComponent.cpp`'s
`{2, EAbilitySlot::Sleep}` map entry, fired via
`UAbilityUnlockLevelSubsystem::HandleLevelBegin` → `NotifyLevelReached`), so "the
entrance room" is the first thing the player encounters afterward. No runtime
gating/spawn code, UI, or special-cased AI is added — the Sniper runs its normal
state-machine AI, exactly as the issue requires.

## Investigation finding (Task 1)

Headless `UnrealEditor-Cmd.exe -run=pythonscript` read of the live, shipped
`L_Level02.umap` found the entrance room (`RoomActor_0`, lowest X) held **two**
enemies of the **wrong** types — one `RunnerEnemy` and one `TrooperEnemy` — no Sniper
at all. Every room in the level held exactly 2 enemies (not a density ramp); total
enemy count was 8, matching `KrowdKontrolLevel02Test.cpp`'s existing design-target
assertion.

## Content change (Task 2)

Edited `L_Level02.umap` via the same headless-Editor Python mechanism (no live MCP
needed, per this repo's established `-run=pythonscript` content-authoring pattern):

- Relocated the closest existing `ASniperEnemy` (from `RoomActor_2`, the third room)
  into the entrance room (`RoomActor_0`), replacing its previous occupants.
- Relocated the entrance room's previous occupants (`RunnerEnemy_0`, `TrooperEnemy_0`)
  into the second room in chain order (`RoomActor_1`) rather than deleting them, so
  the level's total enemy count stays exactly 8.
- Added a missing `SN_1PR` target zone to `RoomActor_0` and a missing `RU_NNR` target
  zone to `RoomActor_1`, since `CheckRoomTargetZonesAndDensity` requires every enemy
  type placed in a room to have a matching target zone in that same room. Pre-existing
  target zones were left untouched (harmless if unused by the check).
- Fixed a resulting regression: `ARoomActor::AddTargetZone()` spawns its marker at the
  world origin before snapping onto the owning room's actor transform — harmless for
  every other room, but `RoomActor_0` itself sits exactly at world-origin (0,0,0), so
  the new `SN_1PR` marker landed there too, tripping
  `KrowdKontrol.PIE.SerializedPlacedActorHealth.L_Level02`'s "marker should not be
  left at the world origin" check (issue #199 regression guard). Repositioned the
  marker to the same ±700/1200-unit Y-offset convention every other room's markers
  already use.

Final entrance-room state, re-verified via a second headless read: exactly one enemy,
`ASniperEnemy`, no other enemy actor present. Room count (4), door count (3), and
total enemy count (8) are unchanged.

## Test change (Task 3)

Added `KrowdKontrolLevelTestUtils::CheckSoloEncounterForCounteredType` — a reusable
helper in `LevelStructureTestUtils.h`, following `CheckEnemyDensityRamp`'s existing
doc-comment/signature style — asserting the entrance room (lowest X, same
`SortRoomsByX` definition the density-ramp check already uses) contains exactly one
enemy, and that enemy is of the given `CounteredType`. Wired into
`KrowdKontrolLevel02Test.cpp` immediately after the existing
`CheckRoomTargetZonesAndDensity` call, passing `EEnemyType::SN_1PR` (Sleep's
countered type, `AbilityData.cpp`'s `GetSleep()`). The helper takes rooms/
enemy-count/enemy-type maps already built by the existing test body, so no new
`TActorIterator` walk was added.

## Acceptance criteria (scoped to Sleep only)

- [x] Immediately after Sleep unlocks (Level 2 arrival), exactly one Sniper spawns
      alone in the entrance room, with no other enemy actor present in that room.
- [x] The Sniper uses its normal state-machine AI — confirmed by construction: no new
      runtime component or special-cased behavior is added anywhere in this change.
- [x] No text box or paused UI is shown around this encounter — confirmed by
      construction: no widget/UI code is touched.
- [x] A new automation test (`KrowdKontrol.Unit.Level02Structure`'s extended
      assertion, via `CheckSoloEncounterForCounteredType`) confirms exactly one
      Sniper is present/placed in Level 2's entrance room.
- [x] All pre-existing `KrowdKontrolLevel02Test.cpp` assertions still pass (room
      count 4, door count 3, total enemy count 8, target-zone coverage, reachability,
      self-heal).
- [x] `python harness/ci.py` full mode exits `GATE_OK`.
- [x] Root/Fear/Snare are deferred to three follow-up issues (see below) — this PR
      covers Sleep only, per issue #31's own split-if-oversized instruction.

On "active/alerted" in the AC: this repo's existing `KrowdKontrolLevelNNTest.cpp`
family never dispatches `BeginPlay()` (`FAutomationEditorCommonUtils::LoadMap`
doesn't start play), so no automation test in this codebase can observe a live
`EEnemyState::Alert` transition against a *loaded real map*. This PR satisfies the
AC's intent via the same structural-placement convention every existing level test
already uses: exactly one enemy present, alone, is the thing that would go Alert when
the player enters.

## Deferred scope

Per issue #31's own Notes clause and this plan's scope cut, three follow-up issues
should be filed, each reusing `CheckSoloEncounterForCounteredType` exactly as this PR
does, just targeting a different level/type pair:

- "Onboarding: forced-safe solo Trooper encounter after Root unlock (Level 3)" —
  `L_Level03Test.cpp` / `EEnemyType::TR_UPR`
- "Onboarding: forced-safe solo Bomber encounter after Fear unlock (Level 4)" —
  `L_Level04Test.cpp` / `EEnemyType::B0_0MR`
- "Onboarding: forced-safe solo Runner encounter after Snare unlock (Level 5)" —
  `L_Level05Test.cpp` / `EEnemyType::RU_NNR`

Each level's `.umap` edit (if needed) is an independent, isolated Editor-content
change with its own risk of breaking that level's own already-passing structure test,
so bundling all four into one PR/issue was avoided.

## Validation evidence

Full gate (`python harness/ci.py`, mode=full):

```
HARNESS_START mode=full driver=cli
STATIC_SKIPPED no 'static' command in harness.config.json
UNIT_PASSED tests=138
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

The `PIE_PASSED` rung includes `KrowdKontrol.PIE.SerializedPlacedActorHealth.L_Level02`
— this failed on the first pass (new target-zone marker landed at the world origin,
see "Content change" above) and passed after the marker was repositioned, so this run
directly exercises the fix for that regression, not just the new placement.

`app/` and `app-source-tracked/` copies of both changed files are identical (verified
via `diff`, re-confirmed at PR-creation time).

MISSION.md Hard Invariants reviewed against this diff: no kill-rule, colour-lock,
ability-roster, enemy-roster, engine/dimensionality, networking, or `app`-tracking
invariant is touched — this is enemy/target-zone placement inside one already-shipped
level plus a new structural test assertion.
