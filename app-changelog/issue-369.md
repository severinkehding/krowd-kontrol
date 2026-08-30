# Issue #369: Document the Level 1-5 difficulty ramp (room/enemy/unlock counts)

Read-only documentation pass per `docs/prd-levels-4-5.md` REQ-3: tabulates room count,
door count, total enemy count, and ability-unlock pairing for all five shipped Alpha
levels (`L_Level01`-`L_Level05`) in one place, and states explicitly whether the
intended difficulty ramp holds across the full sequence. No gameplay values, level
content, test files, or unlock mappings are changed - every number below is read
directly from the five tracked `KrowdKontrolLevelNNTest.cpp` structure tests (each
asserts its own level's exact room/door/enemy counts as literal, hand-authored design
targets already enforced by CI) and from `AbilityUnlockComponent.cpp`'s
`GetLevelToAbilityMap()`. This changelog entry is the sole artifact of issue #369 -
there is no corresponding `app/` or `app-source-tracked/` change to make.

## Ramp table

| Level | Rooms | Doors | Total Enemies | Ability Unlocked |
|-------|-------|-------|----------------|--------------------|
| 1 (`L_Level01`) | 3 | 2 | 6 | Stun (unlocked at construction, not via a level trigger) |
| 2 (`L_Level02`) | 4 | 3 | 8 | Sleep |
| 3 (`L_Level03`) | 5 | 4 | 10 | Root |
| 4 (`L_Level04`) | 6 | 5 | 12 | Fear |
| 5 (`L_Level05`) | 7 | 6 | 14 | Snare |

## Finding: ramp is monotonically increasing

The ramp is monotonically increasing across Levels 1-5 in both room count
(3→4→5→6→7, +1 per level) and total enemy count (6→8→10→12→14, +2 per level).
Ability-unlock pacing has no gaps: one new ability unlocks every level from Level 2
onward (Sleep, Root, Fear, Snare), with Stun already available from the start.
No level breaks the ramp - there is no flagged finding to report.

## Source of truth

| Data point | File | Line(s) |
|------------|------|---------|
| Level 1: 3 rooms, 2 doors | `app/Source/KrowdKontrol/Private/Tests/KrowdKontrolLevel01Test.cpp` | 53, 61 |
| Level 1: 6 enemies (1+2+3 per room) | `app/Source/KrowdKontrol/Private/Tests/KrowdKontrolLevel01Test.cpp` | 122-124 |
| Level 2: 4 rooms, 3 doors, 8 enemies | `app/Source/KrowdKontrol/Private/Tests/KrowdKontrolLevel02Test.cpp` | 85, 92, 140 |
| Level 3: 5 rooms, 4 doors, 10 enemies | `app/Source/KrowdKontrol/Private/Tests/KrowdKontrolLevel03Test.cpp` | 84, 91, 130 |
| Level 4: 6 rooms, 5 doors, 12 enemies | `app/Source/KrowdKontrol/Private/Tests/KrowdKontrolLevel04Test.cpp` | 84, 91, 130 |
| Level 5: 7 rooms, 6 doors, 14 enemies | `app/Source/KrowdKontrol/Private/Tests/KrowdKontrolLevel05Test.cpp` | 84, 91, 130 |
| Ability-unlock map (`{2,Sleep} {3,Root} {4,Fear} {5,Snare}`) | `app/Source/KrowdKontrol/AbilityUnlockComponent.cpp` | 8-17 |

Level 1's total enemy count (6) is not asserted as a single named "design target" line
the way Levels 2-5 are (`TestEqual(..., 8)`, `TestEqual(..., 10)`, etc.) - it's the sum
of three separate per-room literal assertions (1+2+3,
`KrowdKontrolLevel01Test.cpp:122-124`).

All room/door/enemy counts above are literal `TestEqual()` design-target assertions
already enforced by CI (`KrowdKontrol.Unit.Level0{1..5}Structure`), not independently
re-measured for this changelog entry - re-read directly from the live `app/` source at
implementation time (not copied from an earlier cached plan) to confirm no drift since
PR #383 (Level 4) / PR #392 (Level 5) merged. This table cannot drift from the shipped
levels without a test failure surfacing it first.

## Acceptance criteria

- [x] **A single table covering Levels 1-5 with room count, total enemy count, and
      ability unlock per level** - see Ramp table above.
- [x] **Explicit statement of whether the ramp is monotonically increasing in both room
      count and enemy count** - see Finding above; confirmed true, stated explicitly.
- [x] **No gameplay values, level content, test files, or unlock mappings changed** -
      `git status` shows only this one new changelog file.
- [x] **Every number transcribed traces back to a specific file:line** - see Source of
      truth table above.

## Not building

- **No rebalancing of any level's room count, enemy count, or enemy placement** - out
  of scope per the issue body and REQ-3's own "recorded in the changelog" framing.
- **No changes to `AbilityUnlockComponent.cpp`'s unlock mapping** - read and reported
  only.
- **No new or modified automation tests** - the five existing
  `KrowdKontrolLevelNNTest.cpp` files are the source of truth and are not touched.
- **No `app-source-tracked/` mirror** - this task makes no `app/` change, so there is
  nothing to mirror; `app-changelog/` is itself already git-tracked.
- **No re-litigation of the Level 4/Level 5 authoring decisions** - locked per
  PR #383/#392; this entry only reports on them.

---

This task made no change under `app/` - the five `KrowdKontrolLevelNNTest.cpp` files
and `AbilityUnlockComponent.cpp` cited above are read-only source-of-truth references,
already tracked via their existing `app-source-tracked/` mirrors from their own
original issues (#42/#43/#45/#367/#368). This changelog entry is the sole artifact of
issue #369.
