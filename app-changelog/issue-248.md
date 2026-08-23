# Issue #248: Room-State Line on the Quest Tracker

Extends `UQuestTrackerWidget` (issue #247, PR #271; issue #249, PR #285) with a third
line showing the state of the room the player is currently working through, e.g.
`"Room 2 — 1 robot left"` while it has un-banked owned enemies, switching to
`"DOOR OPEN"` once every room in chain order is cleared. Driven entirely by
`ARoomActor::OnRoomClearedStateChanged` (event-driven, no `Tick`/polling added,
matching this widget's established invariant).

`RefreshRoomStateDisplay()` recomputes on every fire: sweeps every live `ARoomActor`,
sorts by ascending world X (the codebase's established chain/entrance-order
convention — the same heuristic `ADoorConnectorActor::BeginPlay()`'s `GatingRoom`
pick and `KrowdKontrolLevelTestUtils::SortRoomsByX` already use), finds the first
room in that order that is not yet cleared, and shows its 1-based chain position and
remaining-enemy count (singular/plural). If every room is cleared, shows
`"DOOR OPEN"`. With zero `ARoomActor` in the world (e.g. the pre-existing
widget-only test cases), the line stays blank rather than claiming a false
`"DOOR OPEN"`.

`ARoomActor` gains a new public accessor, `GetRemainingEnemyCount()` — the loop
`IsRoomCleared()` used to run inline, now extracted so `IsRoomCleared()` is
implemented in terms of it (`return GetRemainingEnemyCount() == 0;`) and the two can
never drift. Behavior is unchanged; existing `IsRoomCleared()` coverage in
`KrowdKontrol.Unit.RoomActor`/`KrowdKontrol.Unit.RoomActorDoorGating` passes
unmodified.

`QuestTrackerWidget`'s panel height (`TrackerHeightPx`) grew from 56px to 80px to fit
the third row; width is unchanged, so the existing resolution-safety test (which only
checks width footprint) needed no changes.

## Deviation from plan

The new test case (`FKrowdKontrolQuestTrackerWidgetRoomStateTest`) drives a plain
`AEnemyBaseTestActor` through the private `TickCheckDetection()`, same as every other
`KrowdKontrol.Unit.*` test that needs to move an enemy Idle→Alert. The investigation
plan didn't call out that this requires its own `friend class` grant on `AEnemyBase`
(each test *class* needs its own explicit friendship — an existing grant for
`FKrowdKontrolQuestTrackerWidgetTest` doesn't cover the new, differently-named test
class). Added `friend class FKrowdKontrolQuestTrackerWidgetRoomStateTest;` to
`EnemyBase.h`, alongside the existing grant list, following the exact same pattern
issue #249's pass-2 review already established for this same widget's suggested-ability
test. Confirmed via a failed build (`C2248: cannot access private member`) before the
fix, and a clean build after.

## Acceptance criteria

- [x] Quest tracker panel gains a third line showing current-room state, sourced from
      `ARoomActor::OnRoomClearedStateChanged` (event-driven, no `Tick`/polling added)
- [x] While the focus room has un-banked owned enemies: line reads
      `"Room {N} — {count} robot(s) left"` with correct singular/plural
- [x] Once every room in chain order is cleared: line reads `"DOOR OPEN"`
- [x] Panel chrome (`RoomStateText`'s colour) uses `HUDChromeColours::GetText()` — no
      reserved gameplay colour introduced
- [x] `KrowdKontrol.Unit.QuestTrackerWidgetRoomState` fires
      `OnRoomClearedStateChanged` via real `AddOwnedEnemy()`/banking calls (not a
      synthetic `Broadcast()`) and asserts the text changes, including the door-open
      case
- [x] All pre-existing `KrowdKontrol.Unit.QuestTrackerWidget` cases (1)-(15) still
      pass unchanged
- [x] `app/` and `app-source-tracked/` copies of every changed file are identical
      (`diff` clean)
- [x] `python harness/ci.py` reports `GATE_OK`
- [x] `app-changelog/issue-248.md` written (this file)

## Validation evidence

Direct build + targeted-suite invocation (drives a real `UnrealBuildTool` compile of
`KrowdKontrolEditor Win64 Development` plus `harness/run_ue_automation.sh`):

```
UE_AUTOMATION_RESULT passed=2 total=2   (KrowdKontrol.Unit.QuestTrackerWidget*)
UE_AUTOMATION_RESULT passed=4 total=4   (KrowdKontrol.Unit.RoomActor)
UE_AUTOMATION_RESULT passed=1 total=1   (KrowdKontrol.Unit.RoomActorDoorGating)
UE_AUTOMATION_RESULT passed=1 total=1   (KrowdKontrol.Unit.Level01)
```

`harness/ci.py --quick`:

```
HARNESS_START mode=quick driver=cli
STATIC_SKIPPED no 'static' command in harness.config.json
UNIT_PASSED tests=100
GATE_OK mode=quick
```

MISSION.md Hard Invariants reviewed against this diff: chrome uses `HUDChromeColours`
exclusively (`RoomStateText` gets `TextColor` at construction, no reserved-colour
tint); no per-frame polling added; no kill-rule, ability-roster, enemy-roster,
engine/dimensionality, networking, or `app`-tracking invariant is touched.

---

## Review follow-up (self-fix pass, 2026-08-23)

Addressed every finding from the code-review/test-coverage/docs-impact review of PR
#289:

- **CRITICAL** — `FKrowdKontrolQuestTrackerWidgetRoomStateTest` only ever spawned one
  `ARoomActor`, so `Rooms.Sort()`'s ascending-X chain order was never actually
  exercised (a 1-element sort is a no-op). Added a second, independent `World` with
  two rooms spawned in reversed X order, proving focus advances "Room 1" → "Room 2" by
  X position (not spawn order) once Room 1 clears.
- **HIGH** — the zero-`ARoomActor` blank-text branch had no assertion. Added one to
  the main test's pre-existing case (1), whose `World` already has zero rooms.
- **HIGH** — `RoomStateText`'s chrome-colour compliance (Hard Invariant 3) was
  untestable: the room-state test class wasn't a friend of `UQuestTrackerWidget` and
  no accessor existed for this line. Added `friend class
  FKrowdKontrolQuestTrackerWidgetRoomStateTest;` to `QuestTrackerWidget.h` (mirroring
  the existing `FKrowdKontrolQuestTrackerWidgetTest` grant) and asserted directly
  against `HUDChromeColours::GetText()`, matching the main test's case (6) pattern.
- **HIGH** (docs) — `docs/prd-mission-briefing-tracker.md` REQ-2 still read "🟡
  partially implemented ... current-room-state line still open" and its example
  showed a `"DOOR OPEN — move east"` directional cue that's actually REQ-3, still
  open. Flipped to "✅ implemented", added the issue #248/PR #289 citation, and
  stripped the direction-cue text with a one-line pointer to REQ-3.
- **MEDIUM** — `ARoomActor::GetRemainingEnemyCount()` had no direct `ARoomActor`-level
  test (only incidental 2→1→0 coverage via the widget test). Added
  `FKrowdKontrolRoomActorRemainingEnemyCountTest` to `KrowdKontrolRoomActorTest.cpp`:
  3 owned enemies, one banked, one destroyed (not banked, proving the
  `IsActorBeingDestroyed()` filter), one banked to reach 0. Required its own
  `friend class FKrowdKontrolRoomActorRemainingEnemyCountTest;` grant on
  `EnemyBase.h` (same per-test-class pattern this issue's own pass-1 deviation
  already used) since `KrowdKontrolRoomActorTest.cpp` had never previously needed
  private `AEnemyBase` access.
- **LOW** — `QuestTrackerWidget.h`'s class-level doc comment still described the
  room-state line as unbuilt future work. Updated the trailing sentence to describe
  issue #248 as delivered, matching the file's existing per-issue narrative style.

Validation: full rebuild (`UnrealBuildTool KrowdKontrolEditor Win64 Development`) —
`Result: Succeeded`. `harness/run_ue_automation.sh "KrowdKontrol.Unit."` —
`UE_AUTOMATION_RESULT passed=101 total=101`.

---

The real Unreal project stays under `app/` (gitignored, D-003) — this changelog and
its matching `app-source-tracked/` copy are the tracked-repo record of that change,
per D-009. Not a substitute for reading `app-source-tracked/` directly.
