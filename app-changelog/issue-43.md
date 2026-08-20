# Issue #43: Build hand-authored Level 2 (Alpha, medium difficulty tier)

Authors the second of krowd-kontrol's 5 hand-authored Alpha levels
(`/Game/Maps/L_Level02.umap`) on top of the already-merged `ARoomActor`/
`ADoorConnectorActor` foundation (issue #39, PR #105) and `L_Level01` (issue #42, PR
#150). Level 2 is a 4-room linear chain — strictly more rooms and strictly more
enemies than Level 1's 3 rooms / 5 enemies — carrying 8 static placeholder-density
enemies across all 4 core enemy types (first appearance of `ASniperEnemy`, deliberately
deferred by Level 1), so the difficulty ramp Levels 2-5 build on (MISSION.md P0) now has
a second real data point. Structural scope only: room count, door connectivity, and
target-zone placement — no narrative content, no new mechanics.

**Note on "5, not 3" hand-authored Alpha levels:** the issue body paraphrases PRD 05
REQ-1's stale "3 hand-authored levels" text. `MISSION.md` (line 58) supersedes this with
5, per the operator decision resolving issue #69's discrepancy — this changelog and the
new test's comments say "the next Alpha level after Level 1," never "second of 3."

The level was authored via a headless Unreal Python session
(`UnrealEditor-Cmd.exe -run=pythonscript`), not the MCP plugin's `ProgrammaticToolset`
sandbox, because `ARoomActor::AddTargetZone()` is a genuine `UFUNCTION` call the sandbox
cannot invoke (no `unreal` module, no method-invocation tool) — same technique issue
#42 used. A new `KrowdKontrol.Unit.Level02Structure` Automation test loads both
`L_Level01` and `L_Level02` in one test function and asserts the room-count/enemy-count
comparison directly against the live `L_Level01` asset, not a hardcoded magic number, so
it can't silently drift if Level 1 is ever revised.

## Files changed (all under `app/`, gitignored per D-003 — this is the tracked-repo
record of that change, see the closing note below)

| File | Action | What it contains |
|------|--------|-------------------|
| `app/Content/Maps/L_Level02.umap` | CREATE | Binary map asset (49693 bytes): 4 `ARoomActor` instances in a linear chain, 3 `ADoorConnectorActor` instances wiring adjacent rooms, two target zones per room via `AddTargetZone()` (covering all 4 `EEnemyType` values), and 8 static placeholder-density enemy actors (2x `ARunnerEnemy`, 2x `ATrooperEnemy`, 2x `ABomberEnemy`, 2x `ASniperEnemy`) — no live AI/GameMode wiring |
| `app/Source/KrowdKontrol/Private/Tests/KrowdKontrolLevel02Test.cpp` | CREATE | `KrowdKontrol.Unit.Level02Structure` — loads `L_Level01` then `L_Level02`, asserts Level02's room count (4, strictly > Level01's live count) and door count (3, each connecting two distinct rooms via `ConnectsValidRooms()`), walks the door adjacency graph (BFS) to confirm all 4 rooms are reachable, asserts every room has >=1 target zone and >=1 enemy placeholder, asserts every distinct enemy type placed in a room (matched to its nearest `ARoomActor` by distance) has a target zone of the matching `EEnemyType` in that same room (REQ-2), and asserts Level02's total enemy count (8) is strictly greater than Level01's live count |
| `app-source-tracked/Source/KrowdKontrol/Private/Tests/KrowdKontrolLevel02Test.cpp` | CREATE | Plain-text mirror of the test file above, per Hard Invariant 8's D-009 carve-out, so GitHub has a real diff to open a PR against |

Not mirrored here: `L_Level02.umap` itself is a binary asset, excluded from
`app-source-tracked/` per CLAUDE.md's mirror rules (never `.uasset`/`.umap`/anything
under `Content/`) — only the new `.cpp` test file is mirrored.

## Acceptance criteria

- [x] **A new level map (`L_Level02.umap`) is built using `ARoomActor`/
      `ADoorConnectorActor`** — confirmed on disk, 49693 bytes.
- [x] **Room count strictly greater than Level 1's 3** — 4 rooms, asserted dynamically
      against the live `L_Level01` asset by `KrowdKontrol.Unit.Level02Structure`.
- [x] **Placeholder enemy-density strictly greater than Level 1's 5** — 8 enemies,
      asserted the same way.
- [x] **Every room has >=1 target-zone marker per enemy type placeholder present in
      that room (REQ-2)** — asserted per-room, checked against the actual distinct
      enemy types placed in each room via nearest-room-by-distance matching.
- [x] **Enemy presence is placeholder markers only — no live AI/GameMode wiring** —
      `ARunnerEnemy`/`ATrooperEnemy`/`ABomberEnemy`/`ASniperEnemy` placed as static
      content only, using existing, already-tested classes.
- [x] **`KrowdKontrol.Unit.Level02Structure` confirms the level loads without errors
      and its room count exceeds Level 1's, computed dynamically against the live
      `L_Level01` asset** — passes, 1/1; the test loads both levels in the same
      function and compares live counts, not hardcoded numbers.
- [x] **Level 1-3 validation commands pass with exit 0** — see Validation below.
- [x] **No regressions in existing `KrowdKontrol.Unit.*`/`KrowdKontrol.Smoke.*`
      tests** — 60/60 unit tests pass (59 pre-existing + 1 new).

## Validation

```
$ python harness/ci.py
HARNESS_START mode=full driver=cli
STATIC_SKIPPED no 'static' command in harness.config.json
UNIT_PASSED tests=60
APP_STARTED driver=cli
UE_BUILD_OK
UE_AUTOMATION_RESULT passed=1 total=1
UE_AUTOMATION_OK
E2E_PASSED steps=1
HOLDOUT_ABSENT no .factory/holdout/run.py
MUTATIONS_ABSENT no harness/mutations/run.py
GATE_OK mode=full
```

`UNIT_PASSED tests=60` includes the new `KrowdKontrol.Unit.Level02Structure` test
alongside every pre-existing `KrowdKontrol.Unit.*`/`KrowdKontrol.Smoke.*` test (59
baseline, confirmed live via `python harness/ci.py --quick` before any change was
made) — no regressions. The gate passed on the first run; no fixes were required
during implementation or validation.

MISSION.md Hard Invariants reviewed against the diff: no-kill rule, 5-colour lock,
5-ability roster, and engine/dimensionality lock are all N/A (no such logic touched);
4-type enemy roster (#5) is respected — only existing enemy classes
(`ARunnerEnemy`/`ATrooperEnemy`/`ABomberEnemy`/`ASniperEnemy`) are placed, no new enemy
type introduced, and this is the first level to place `ASniperEnemy` (deliberately
deferred by Level 1's own changelog); `app/` not tracked in git (#8) — all new files
stayed under the untracked `app/` symlink, mirrored here only as a plain-text source
copy per D-009.

---

Source lives under `app/` (gitignored, D-003) — this file is the tracked-repo record
of that change, not a substitute for reading the actual code.
