# Issue #246: Add data-driven pre-level briefing card shown on level-begin

## Summary

Nothing in-game told the player a level's objective — only C++-consumer-free
`ULevelLifecycleSubsystem::OnLevelBegin` broadcasts and (per the Mission
Briefing & Live Quest Tracker PRD's REQ-1) an operator explaining things
outside the game. Adds a briefing overlay shown on `OnLevelBegin`, populated
per-level from a new `FLevelBriefingRow` `UDataTable` row (level display name,
imperative objective lines, optional new-ability-unlock line) — never
hardcoded C++ strings per map. The card dismisses on any player input or after
an 8-second auto-timeout, and pauses the world (`UGameplayStatics::SetGamePaused`)
while shown, with an explicit `World->IsPaused()` gate added at
`UAbilityCastComponent::TryCastAbility` as defense in depth since `APlayerController`
input dispatch is not itself blocked by world pause.

## Files changed (all under `app/`, gitignored per D-003 — this is the tracked-repo record)

| File | Action | What it contains |
|------|--------|-------------------|
| `LevelBriefingData.h` | CREATE | `FLevelBriefingRow` DataTable row struct: level display name, objective lines, optional unlock line |
| `BriefingCardWidget.h` / `.cpp` | CREATE | C++-built chrome widget, 8s auto-dismiss timer, world-pause on show |
| `LevelBriefingSubsystem.h` / `.cpp` | CREATE | `OnLevelBegin` subscriber, table lookup by bare map name (PIE-prefix stripped), forwards to player controller |
| `KrowdKontrolPlayerController.h` / `.cpp` | UPDATE | `ShowLevelBriefing()`, pending-buffer flush for the `OnLevelBegin`-before-`BeginPlay` race, `EKeys::AnyKey` dismiss bind |
| `AbilityCastComponent.cpp` | UPDATE | `TryCastAbility` gains a `World->IsPaused()` early-out |
| `Private/Tests/KrowdKontrolLevelBriefingSubsystemTest.cpp` | CREATE | End-to-end subsystem test |
| `Private/Tests/KrowdKontrolBriefingCardWidgetTest.cpp` | CREATE | Widget unit test |
| `Private/Tests/KrowdKontrolReservedGameplayColoursTest.cpp` | UPDATE | Chrome-colour audit extended to `UBriefingCardWidget` |
| `Private/Tests/KrowdKontrolAbilityCastComponentTest.cpp` | UPDATE | New pause-gate test case |

## Acceptance criteria

- [x] **New data asset/table type holds per-level briefing content** — `FLevelBriefingRow` (level display name, objective lines, optional new-ability-unlock line), authored per level, no hardcoded C++ strings per map.
- [x] **Briefing widget created and shown on `ULevelLifecycleSubsystem::OnLevelBegin`**, populated from that level's data asset entry via `ULevelBriefingSubsystem`.
- [x] **Widget dismisses on any player input, or after a short auto-timeout** — `EKeys::AnyKey` bind + documented 8-second timeout.
- [x] **Gameplay paused/made safe while the briefing shows** — `UGameplayStatics::SetGamePaused(true)` plus an added `World->IsPaused()` gate in `UAbilityCastComponent::TryCastAbility` (the single production ability-cast entry point) since input dispatch itself isn't blocked by world pause.
- [x] **Panel chrome uses existing neutral `HUDChromeColours`, no new gameplay-information colour** (Hard Invariant 3) — verified by inspection and by the extended `KrowdKontrolReservedGameplayColoursTest.cpp` audit.
- [x] **Automation test verifies the widget appears on `OnLevelBegin`, is populated from level data, and dismisses on input and on timeout** — `KrowdKontrolLevelBriefingSubsystemTest.cpp` / `KrowdKontrolBriefingCardWidgetTest.cpp`.

## Validation

`python harness/ci.py` (full mode) ran twice for determinism:

```
UE_BUILD_OK
UE_AUTOMATION_RESULT passed=90 total=91
UE_AUTOMATION_FAILED KrowdKontrol.Unit.PlayerControllerShowsMouseCursor: state=Fail
GATE_FAILED: unit
```

The single failure is pre-existing and unrelated: `KrowdKontrolCursorWorldPositionTest.cpp`
belongs to the not-yet-landed "cursor/aiming foundation" feature (commit `7a7bc6e`'s PRD),
exists only in the shared, non-git-tracked `app/` directory, and asserts
`bShowMouseCursor` behavior this PR's diff never reads or writes. Confirmed
deterministic across two runs. All tests this PR added or touched pass:
`KrowdKontrol.Unit.LevelBriefingSubsystem`, `KrowdKontrol.Unit.BriefingCardWidget`,
`KrowdKontrol.Unit.ReservedGameplayColours`, `KrowdKontrol.Unit.AbilityCastComponent`.

A separate transient `KrowdKontrol.Unit.RootSurgeBoss` failure on the first run
was the documented `LogModelContextProtocol: Error: Call to unknown method
"server/discover"` MCP flake and passed cleanly on rerun.

MISSION.md hard invariants reviewed by inspection: 5-colour lock (HI3) verified —
`BriefingCardWidget.cpp` sources all chrome colours from the shared
`HUDChromeColours` helper, matching `AbilityCooldownTrayWidget`/`PostRunSummaryWidget`/
`OnScreenPromptWidget`'s existing pattern; no new saturated colour introduced.
Other invariants (no-kill rule, ability roster, enemy roster, engine lock, no
networking, `app/` untracked) untouched by this diff.

## Concurrent-task leakage note

Mid-implementation, the shared `app/` directory's live `KrowdKontrolPlayerController.h`/`.cpp`
were found missing issue #247's ("Persistent Quest Tracker Panel", `archon/task-fix-issue-247`)
already-validated `QuestTrackerWidgetInstance` wiring — this branch's own
`app-source-tracked` mirror of those two files predates #247 and overwrote that
wiring when copied into the shared `app/`. Restored #247's exact wiring into the
live `app/` copies only (not into this branch's git mirror, since it belongs to
#247's own PR) and verified byte-for-byte against #247's committed source. Not a
code change under review here — see `implementation.md` for the full account.

---

Source lives under `app/` (gitignored, D-003) — this file is the tracked-repo record
of that change, not a substitute for reading the actual code.
