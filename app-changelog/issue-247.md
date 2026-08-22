# Issue #247: Persistent Quest Tracker Panel (Banked-Count Line)

Adds `UQuestTrackerWidget` (`app/Source/KrowdKontrol/QuestTrackerWidget.h/.cpp`), a
new C++-only UMG HUD widget showing a single always-visible line —
`"Robots penned: X/Y"` — anchored to the top-right corner of the screen. `X` (banked
count) updates live, event-driven only, from every `ATargetZone::OnActorBanked`
broadcast in the level; `Y` (the level's total enemy count) is captured once via a
`TActorIterator<AEnemyBase>` sweep when `ULevelLifecycleSubsystem::OnLevelBegin`
fires — not from `NativeOnInitialized()`/`CreateHUDWidgets()` timing, since
`ATargetZone` instances aren't guaranteed to exist yet at that point
(`ARoomActor::BeginPlay()`'s `EnsureBankingZonesWired()` is what spawns/wires them).
The widget is created and added to the viewport from
`AKrowdKontrolPlayerController::CreateHUDWidgets()`, alongside the three existing
persistent HUD widgets (`UEnergyMeterWidget`, `UAbilityCooldownTrayWidget`,
`UOnScreenPromptWidget`). This is the foundational widget for PRD "Mission Briefing &
Live Quest Tracker" REQ-2 — the "current room state" and "suggested ability" lines are
explicitly out of scope, deferred to follow-up issues attaching to this same widget
class.

## Acceptance criteria

- [x] New compact, corner-anchored UMG widget class (`UQuestTrackerWidget`) created
      and shown from `CreateHUDWidgets()`, alongside the level's existing HUD widgets.
- [x] Concrete, documented max-size envelope: 184px (160px width + 24px margin) ≈
      14.4% of a 1280px-wide reference viewport, under the issue's ~15% ceiling —
      documented in `QuestTrackerWidget.h`'s constants comment, enforced by the
      resolution-safety test.
- [x] `"Robots penned: X/Y"` line, `X` from `ATargetZone::OnActorBanked`, `Y` the
      level's total enemy count.
- [x] Event-driven only, no per-frame polling — no `NativeTick()` override, matches
      `UEnergyMeterWidget`'s convention.
- [x] Chrome uses `HUDChromeColours`, no reserved gameplay colours used — verified by
      the new test's in-file chrome assertion.
- [x] Automation test fires `OnActorBanked` N times, asserts displayed count reaches
      N (`KrowdKontrol.Unit.QuestTrackerWidget`).
- [x] Level 1-3 validation commands pass with exit 0.
- [x] No regressions in existing tests (`HUDWiring`, and the wider suite — see
      evidence below).
- [x] `app/` and `app-source-tracked/` copies identical (`diff` clean).
- [x] `app-changelog/issue-247.md` written (this file).

## Deviation from plan.md

`plan.md`'s Task 3 code block for `KrowdKontrolQuestTrackerWidgetTest.cpp` omitted
`#include "Blueprint/WidgetTree.h"`, needed for the full `UWidgetTree` definition
before `Widget->WidgetTree->RootWidget` compiles (`UserWidget.h` only forward-declares
`UWidgetTree`). `KrowdKontrolEnergyMeterWidgetTest.cpp` (the plan's own mirror
target) already includes it. Added the same include; no other change from the plan.

`app/`'s live `KrowdKontrolPlayerController.cpp` and
`Private/Tests/KrowdKontrolHUDWiringTest.cpp` also carry in-progress edits from two
other concurrent tasks sharing the same `app/` symlink target — a
`bShowMouseCursor = true;` block (issue #262, branch `archon/task-fix-issue-262`,
not on `main`) and a `PunishmentLockout`/`GetSlotState` assertion change (issue
#261, commit `c6ae0a7`, not on `main`). Neither is part of this issue. The
`app-source-tracked/` mirror for both files intentionally omits those hunks — it
mirrors only this issue's own diff (the `QuestTrackerWidgetInstance` wiring in the
first file, the single new `TestNotNull` line in the second) against each file's
last-committed `app-source-tracked/` content, not a byte-for-byte copy of `app/`'s
current live state. `diff -r app/ app-source-tracked/` on these two specific files
is therefore expected to show the other tasks' unrelated pending work, not a
parity failure of this issue's own change.

## Validation evidence

Direct `UnrealBuildTool` invocation (`KrowdKontrolEditor Win64 Development`):
`Result: Succeeded`.

Targeted Automation runs during implementation:
```
KrowdKontrol.Unit.QuestTrackerWidget -> UE_AUTOMATION_RESULT passed=1 total=1
KrowdKontrol.Unit.HUDWiring          -> UE_AUTOMATION_RESULT passed=1 total=1
```

`python harness/ci.py --quick`:
```
HARNESS_START mode=quick driver=cli
STATIC_SKIPPED no 'static' command in harness.config.json
UNIT_PASSED tests=89
GATE_OK mode=quick
```

MISSION.md Hard Invariants reviewed against this diff: chrome uses
`HUDChromeColours` exclusively (Hard Invariant 3, the 5-colour lock, is untouched —
no reserved gameplay colour is read or displayed by this widget); no kill-rule,
ability-roster, enemy-roster, engine/dimensionality, networking, or `app`-tracking
invariant is touched.

---

The real Unreal project stays under `app/` (gitignored, D-003) — this changelog and
its matching `app-source-tracked/` copy are the tracked-repo record of that change,
per D-009. Not a substitute for reading `app-source-tracked/` directly.
