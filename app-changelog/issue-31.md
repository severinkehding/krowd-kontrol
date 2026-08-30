# Issue #31: Onboarding — forced-safe solo encounters for Sleep/Root/Fear/Snare

Issue #31 asks that each of the four remaining abilities (Sleep, Root, Fear, Snare)
get a forced-safe solo encounter with its colour-matched enemy type immediately after
that ability unlocks, before that enemy type ever appears mixed into a crowd. This PR
now implements all four: Sleep (Level 2 → Sniper), Root (Level 3 → Trooper), Fear
(Level 4 → Bomber), and Snare (Level 5 → Runner).

**Pass-1 update:** the original version of this PR shipped Sleep only, deferring
Root/Fear/Snare to follow-up issues per issue #31's own Notes clause (pre-authorizing
a per-ability split if the full scope would exceed FACTORY_RULES.md's 500-line PR
cap). Pass-1 validation (behavioral + E2E, both scoring "partially"/"too_narrow")
determined the full four-ability scope should ship in this PR instead, so
Root/Fear/Snare were added here rather than split out.

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

### Level 3 (Root → Trooper), Level 4 (Fear → Bomber), Level 5 (Snare → Runner)

Same headless-Editor Python investigation/edit mechanism, applied to the three
follow-up levels (pass-1 update — see top of this file):

- **Level 3**: entrance room already held one `TrooperEnemy` alongside a `RunnerEnemy`
  — simplest case, no import needed. Relocated the `RunnerEnemy` into the second room
  (chain order), added the now-missing `RU_NNR` target zone there. No marker-origin
  fix needed (second room isn't at the world origin). Total enemy count unchanged
  (10).
- **Level 4**: entrance room held a `RunnerEnemy` and a `TrooperEnemy`, no `BomberEnemy`
  at all. Relocated the closest existing `BomberEnemy` (second room) into the entrance
  room, its previous occupants into the second room, added the missing `RU_NNR` zone
  (second room) and `B0_0MR` zone (entrance room). The entrance-room zone hit the same
  world-origin marker regression `L_Level02` did (`RoomActor_0` sits at (0,0,0) in
  every level) — fixed the same way, repositioned to the ±700/1200-unit Y-offset
  convention. Total enemy count unchanged (12).
- **Level 5**: entrance room already held one `RunnerEnemy` alongside a `TrooperEnemy`
  — same simple case as Level 3. Relocated the `TrooperEnemy` into the second room,
  which already had a matching `TR_UPR` zone, so no new target zone was needed at all.
  Total enemy count unchanged (14).

Every level's final entrance-room state was re-verified via a headless read: exactly
one enemy, of the correct countered type, no other enemy actor present. Room/door/
total-enemy counts are unchanged from each level's existing design targets.

## Test change (Task 3)

Added `KrowdKontrolLevelTestUtils::CheckSoloEncounterForCounteredType` — a reusable
helper in `LevelStructureTestUtils.h`, following `CheckEnemyDensityRamp`'s existing
doc-comment/signature style — asserting the entrance room (lowest X, same
`SortRoomsByX` definition the density-ramp check already uses) contains exactly one
enemy, and that enemy is of the given `CounteredType`. Wired into all four of
`KrowdKontrolLevel02Test.cpp` (`EEnemyType::SN_1PR`), `KrowdKontrolLevel03Test.cpp`
(`EEnemyType::TR_UPR`), `KrowdKontrolLevel04Test.cpp` (`EEnemyType::B0_0MR`), and
`KrowdKontrolLevel05Test.cpp` (`EEnemyType::RU_NNR`), each immediately after the
existing `CheckRoomTargetZonesAndDensity` call. The helper takes rooms/enemy-count/
enemy-type maps already built by each existing test body, so no new `TActorIterator`
walk was added.

### Live Alert-state coverage (pass-1 medium-severity follow-up)

`CheckSoloEncounterForCounteredType` only proves static placement at level load,
since `KrowdKontrolLevelNNTest.cpp`'s `FAutomationEditorCommonUtils::LoadMap` never
dispatches `BeginPlay()`. Added `KrowdKontrolPIESoloEncounterAlertTest.cpp` — four new
`KrowdKontrol.PIE.SoloEncounterAlert.L_LevelNN` tests (mirrors
`KrowdKontrolPIESerializedPlacedActorHealthTest.cpp`'s `AutomationOpenMap` shape) that
open each level in a real PIE session and poll, on a wall-clock timeout (not a fixed
frame count), until the entrance room's sole enemy transitions `Idle`→`Alert`, then
asserts it's the correct countered type. The wall-clock poll (not a short fixed-frame
wait) is necessary because `AEnemyBase::TickCheckDetection`'s `Idle`→`Alert` branch is
gated on `!OwningRoom->IsActivationPending()`, and `ARoomActor`'s first-entry
countdown (`RoomActivationCountdownSeconds`, 3.0s default) keeps that pending for a
few real seconds after the player first enters — confirmed empirically: an initial
version of this test using a 5-frame wait failed on all four levels (found the correct
enemy/type but `GetEnemyState() != Alert` yet); switching to a 10s wall-clock poll
(mirroring `KrowdKontrolPIESniperRangeBreakChaseTest.cpp`'s own `WaitForRoomActivated`
phase, which hits the same gate) passed on all four.

## Acceptance criteria

- [x] Immediately after each ability unlocks (Sleep/Root/Fear/Snare, Levels 2-5),
      exactly one enemy of the matching countered type spawns alone in that level's
      entrance room, with no other enemy actor present in that room.
- [x] Every entrance-room enemy uses its normal state-machine AI — confirmed by
      construction (no new runtime component/special-cased behavior anywhere in this
      change) and by the new live-PIE `KrowdKontrol.PIE.SoloEncounterAlert.*` tests,
      which prove it actually reaches `EEnemyState::Alert` through real per-tick
      `TickCheckDetection`, not a direct/friend call.
- [x] No text box or paused UI is shown around any of these encounters — confirmed by
      construction: no widget/UI code is touched.
- [x] `KrowdKontrol.Unit.Level0{2,3,4,5}Structure`'s extended assertion (via
      `CheckSoloEncounterForCounteredType`) confirms exactly one enemy of the correct
      type is present/placed in each level's entrance room at load.
- [x] `KrowdKontrol.PIE.SoloEncounterAlert.L_Level0{2,3,4,5}` confirms that same
      entrance-room enemy actually goes live-Alert during a real PIE session.
- [x] All pre-existing `KrowdKontrolLevel0{2,3,4,5}Test.cpp` assertions still pass
      (room/door/total-enemy counts, target-zone coverage, reachability, self-heal).
- [x] `python harness/ci.py` full mode exits `GATE_OK`.

## Validation evidence

Full gate (`python harness/ci.py`, mode=full), re-run after the pass-1 update above:

```
HARNESS_START mode=full driver=cli
STATIC_SKIPPED no 'static' command in harness.config.json
UNIT_PASSED tests=138
PIE_PASSED tests=12
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

`PIE_PASSED` went from 8 (original Sleep-only pass) to 12: the four new
`KrowdKontrol.PIE.SoloEncounterAlert.L_Level0{2,3,4,5}` tests. The `PIE` rung also
still includes `KrowdKontrol.PIE.SerializedPlacedActorHealth.L_Level02` — this failed
on the first pass (new target-zone marker landed at the world origin, see "Content
change" above) and passed after the marker was repositioned, so this run directly
exercises the fix for that regression, not just the new placement. The Level 4
entrance-room marker hit the same regression and was fixed the same way (see "Content
change" above), but has no dedicated `SerializedPlacedActorHealth` test today (that
test file only covers L_Level01/L_Level02 - out of scope for this PR to extend).

`app/` and `app-source-tracked/` copies of every changed/added file are identical
(verified via `diff`, re-confirmed at PR-creation time).

MISSION.md Hard Invariants reviewed against this diff: no kill-rule, colour-lock,
ability-roster, enemy-roster, engine/dimensionality, networking, or `app`-tracking
invariant is touched — this is enemy/target-zone placement inside four already-shipped
levels plus new structural and live-PIE test assertions.
