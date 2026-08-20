# Issue #185: Add playable spawn (PlayerStart + pawn) to L_Level01 and L_Level02 with a regression test

`L_Level01` and `L_Level02` — the two shipped gameplay maps — had no `PlayerStart` and
no player pawn. Because `AKrowdKontrolGameMode` intentionally has no
`DefaultPawnClass` (playable pawns self-possess via `AutoPossessPlayer`, per issue
#132), PIE on either map silently fell back to the engine's invisible free-fly
`DefaultPawn`. Every system that looks for a `UPlayerEnergyComponent`-carrying pawn
(`AEnemyBase::FindPlayerEnergyComponent()`, `ARootSurgeBoss::FindPlayerEnergyComponent()`,
ability casting, the HUD wiring from #132) was a silent no-op on both maps, with no
test guarding against it — confirmed live in a PIE session and by the PRD
(`docs/prd-level-playability-presentation.md` REQ-1).

This is a content gap, not a code defect: the pawn class
(`AFlatCamera3DPrototypePawn`), the component it needs (`UPlayerEnergyComponent`), and
the GameMode contract were already correct (proven by the existing
`FKrowdKontrolFlatCamera3DPipelineLevelTest` on `L_FlatCamera3DPrototype`) — the two
gameplay maps simply never had a spawn placed. No production C++ logic changed.

## Approach

1. Placed one `PlayerStart` and one `AFlatCamera3DPrototypePawn` at each map's entrance
   room (the `ARoomActor` at the lowest X location — world origin `(0, 0, 0)` in both
   `L_Level01` and `L_Level02`), with a +75 Z offset, via a headless
   `UnrealEditor-Cmd.exe -run=pythonscript` session (this repo's established
   headless-authoring pattern from issues #42/#43 — live `unreal-mcp` tools were not
   reachable this session). Each map was re-queried after saving to confirm the actors
   were actually present, not just that the spawn call succeeded.
2. Added `KrowdKontrol.Unit.GameplayLevelsHavePlayableSpawn`
   (`KrowdKontrolGameplayLevelPlayableSpawnTest.cpp`), which loads each map in
   `GameplayLevelMapPaths` (currently `L_Level01`, `L_Level02` — a single array, so
   future `L_Level03`-`05` maps are a one-line addition) and fails if the world
   contains no `PlayerStart` or no placed pawn carrying a `UPlayerEnergyComponent`.

## Files changed (all under `app/`, gitignored per D-003 — this is the tracked-repo record of that change)

| File | Action | What it contains |
|------|--------|-------------------|
| `app/Content/Maps/L_Level01.umap` | UPDATE (binary, not tracked) | Added one `PlayerStart` + one `AFlatCamera3DPrototypePawn` at (0, 0, 75), the entrance room's location |
| `app/Content/Maps/L_Level02.umap` | UPDATE (binary, not tracked) | Same, also at (0, 0, 75) — L_Level02's entrance room is independently also at world origin |
| `app/Source/KrowdKontrol/Private/Tests/KrowdKontrolGameplayLevelPlayableSpawnTest.cpp` | CREATE | New `KrowdKontrol.Unit.GameplayLevelsHavePlayableSpawn` test — loads each gameplay map and asserts a `PlayerStart` and a `UPlayerEnergyComponent`-carrying pawn both exist |
| `app-source-tracked/Source/KrowdKontrol/Private/Tests/KrowdKontrolGameplayLevelPlayableSpawnTest.cpp` | CREATE | Plain-text mirror of the above, per Hard Invariant 8's D-009 carve-out |

No `.h`/`.cpp` production logic changed — `AKrowdKontrolGameMode`,
`AFlatCamera3DPrototypePawn`, `UPlayerEnergyComponent`, `AEnemyBase`, and
`ARootSurgeBoss` were all already correct and required no edits.

## Acceptance criteria

- [x] **`L_Level01` contains a `PlayerStart` and a placed, self-possessing pawn at its
      entrance** — placed at (0, 0, 75), verified by
      `KrowdKontrol.Unit.GameplayLevelsHavePlayableSpawn`.
- [x] **`L_Level02` contains a `PlayerStart` and a placed, self-possessing pawn at its
      entrance** — same, also at (0, 0, 75).
- [x] **A regression test fails if the world contains no pawn with a
      `UPlayerEnergyComponent`** — `KrowdKontrol.Unit.GameplayLevelsHavePlayableSpawn`,
      extensible to future `L_Level*` maps via its `GameplayLevelMapPaths` array.
      Confirmed to fail against `main`'s unfixed map state
      (`UE_AUTOMATION_FAILED KrowdKontrol.Unit.GameplayLevelsHavePlayableSpawn:
      state=Fail`) before the map fix landed, and pass afterward.
- [x] **No regressions in existing tests** — 69/69 unit tests pass (up from 68 before
      this issue's new test was added).

## Validation

```
$ python harness/ci.py
GATE_OK mode=full
UNIT_PASSED tests=69
```

`KrowdKontrol.Unit.GameplayLevelsHavePlayableSpawn` passes for both maps. MISSION.md
Hard Invariants reviewed against the diff: no-kill rule, 5-colour lock, 5-ability
roster, 4-enemy-type roster, engine/dimensionality lock, and no-networking are all N/A
(no such logic touched — pure level-content placement of an already-correct pawn
class); Invariant #8 (`app/` not git-tracked) is respected — the map edits live
entirely under `app/`, mirrored here only via the `app-source-tracked/` copy path.

### Independently-checkable evidence for the .umap edits

The `PlayerStart`/pawn placement itself is a binary `.umap` change and, per D-009,
will never appear in this repo's tracked diff — no text diff can confirm it. What
*is* independently checkable, by anyone re-running the command below (not just
reading this prose), is the automation test's pass/fail state, which only flips to
pass once the maps actually contain the spawn:

```
$ bash harness/run_ue_automation.sh "KrowdKontrol.Unit.GameplayLevelsHavePlayableSpawn"
UE_BUILD_START KrowdKontrolEditor Win64 Development
UE_BUILD_OK
UE_AUTOMATION_RESULT passed=1 total=1
UE_AUTOMATION_OK
```

Before this fix (`main`, pre-#185), the same command against the unfixed maps
reports the test's failure explicitly by name:
`UE_AUTOMATION_FAILED KrowdKontrol.Unit.GameplayLevelsHavePlayableSpawn: state=Fail`.
That before/after flip — not the PR description — is the falsifiable claim: it
fails on the un-fixed maps and passes on these ones, and any reviewer with access
to the Editor can reproduce both states themselves.

---

Source lives under `app/` (gitignored, D-003) — this file is the tracked-repo record
of that change, not a substitute for reading the actual code. `.umap` binary edits are
invisible to this diff by design (D-009) — the entrance-room coordinates used are
called out above since they can't appear in a text diff.
