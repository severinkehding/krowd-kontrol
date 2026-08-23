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

The real Unreal project stays under `app/` (gitignored, D-003) — this changelog and
its matching `app-source-tracked/` copy are the tracked-repo record of that change,
per D-009. Not a substitute for reading `app-source-tracked/` directly.
