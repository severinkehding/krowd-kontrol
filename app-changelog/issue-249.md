# Issue #249: Suggested-Ability Line on the Quest Tracker

Extends `UQuestTrackerWidget` (issue #247, PR #271) with a second line naming the
crowd-control ability best suited to the enemies still alive in the level: a
colour-matched suggestion (e.g. `"SNIPERS → SLEEP (RMB)"`) when the countering
ability is unlocked, or a universal fallback (`"ANY ROBOT → STUN (LMB)"`) when it
isn't. The suggestion is recomputed from live world state — a fresh
`TActorIterator<AEnemyBase>` sweep filtered to non-`Banked` enemies, each enemy's
`EEnemyType` read via `UEnemyTypeIndicatorComponent`, matched against
`AbilityData::GetAll()`'s `CounteredEnemyType`/`bIsColourNeutral` fields — every time
an event this widget already reacts to fires (`OnLevelBegin`,
`ATargetZone::OnActorBanked`, `UWaveSpawnerComponent::OnWaveSpawned`), plus a new bind
to `UAbilityUnlockComponent::OnAbilityUnlocked`. No `NativeTick()`, no polling.

`AbilityData` gains two new public accessors, `GetDisplayName(EAbilitySlot)` and
`GetEnemyPluralDisplayName(EEnemyType)`, moved (not duplicated) from
`AbilityUnlockPromptComponent.cpp`'s previously-private maps — this widget is the
second consumer of the same ALL-CAPS display strings, so those maps now have one
source of truth. `AbilityUnlockPromptComponent`'s own "PRESS %d" prompt behaviour is
unchanged — this was a pure extraction, verified by its own existing test still
passing unmodified.

## Acceptance criteria

- [x] Quest tracker panel gains a second line naming the suggested ability for
      remaining enemy type(s), colour-matched when unlocked (`AbilityData`'s
      `CounteredEnemyType`/`Colour`), universal Stun fallback otherwise
- [x] Unlock state read via `UAbilityUnlockComponent::IsAbilityUnlocked()`
- [x] The suggestion line's ability-name/enemy-type segment uses the ability's real
      reserved colour (Hard Invariant 3's information exception); the rest of the
      panel chrome (border, banked-count line) stays on `HUDChromeColours`
- [x] Updates are event-driven only — bound to `OnLevelBegin`,
      `ATargetZone::OnActorBanked`, `UWaveSpawnerComponent::OnWaveSpawned`, and the
      newly-added `UAbilityUnlockComponent::OnAbilityUnlocked` — no `NativeTick()`
- [x] Automation test covers both required scenarios (unlocked → colour-matched;
      not-unlocked → fallback) — new cases (12)/(13) in
      `KrowdKontrol.Unit.QuestTrackerWidget`
- [x] Level 1-3 validation commands pass with exit 0
- [x] No regressions in `KrowdKontrol.Unit.HUDWiring`,
      `KrowdKontrol.Unit.AbilityUnlockPromptComponent`, or the pre-existing (1)-(11)
      cases in `KrowdKontrol.Unit.QuestTrackerWidget`
- [x] `app/` and `app-source-tracked/` copies identical for every file this issue
      touches (`diff` clean; see Note on concurrent-task leakage below)
- [x] `app-changelog/issue-249.md` written (this file)

## Note on concurrent-task leakage

`app/`'s live `KrowdKontrolPlayerController.cpp` carries an uncommitted
`AbilityCooldownComponent` bind (a `#include` plus a
`AbilityTrayWidget->BindAbilityCooldownComponent(...)` call in `WireWidgetsToPawn()`)
from a different, concurrent task sharing the same `app/` symlink target — not part
of this issue, and not present in this branch's own `app-source-tracked` baseline.
The `app-source-tracked/` mirror for this file was built from this branch's
last-committed baseline plus only this issue's own one-block addition (the
`QuestTrackerWidgetInstance` bind), deliberately omitting that unrelated hunk — same
precedent `app-changelog/issue-247.md` already documents for this exact file.

## Key-binding display

The issue's own illustrative text uses legacy numeric key examples; this plan follows
the newer, explicit 2026-08-23 operator ruling (`gh issue view 260 -c`) that canonical
ability display always uses `AbilityData::KeyBindingLabel` (LMB/RMB/Q/E/MMB), never
the legacy 1-5 numbers. `AbilityUnlockPromptComponent`'s own numeric "PRESS %d"
prompt now technically contradicts that same ruling but is out of scope for this
issue (a quest-tracker change, not an ability-unlock-prompt change) — flagged, not
fixed.

## Validation evidence

Direct `harness/ci.py --quick` invocation (drives a real `UnrealBuildTool` compile of
`KrowdKontrolEditor Win64 Development` plus the full `KrowdKontrol.Unit.` Automation
suite via `harness/run_ue_automation.sh`):

```
HARNESS_START mode=quick driver=cli
STATIC_SKIPPED no 'static' command in harness.config.json
UNIT_PASSED tests=99
GATE_OK mode=quick
```

(An earlier run in the same session reported one unrelated failure,
`KrowdKontrol.Unit.FlatCamera3DPrototypePawnFacingDoesNotAffectMovement` — a test this
issue's diff never touches; a rerun passed clean, confirming it was a flake and not a
regression from this change.)

MISSION.md Hard Invariants reviewed against this diff: chrome uses `HUDChromeColours`
exclusively; the new suggestion line's colour comes from `AbilityData::Get(...).Colour`
(`ReservedGameplayColours`-backed), the documented Hard Invariant 3 information
exception already established by `AbilityCooldownTrayWidget`/`AbilityTooltipWidget`.
No kill-rule, ability-roster, enemy-roster, engine/dimensionality, networking, or
`app`-tracking invariant is touched.

---

## Pass-2: review self-fix (test coverage + docs)

Addressed all 3 HIGH and 2 MEDIUM findings from the PR review (`consolidated-review.md`
— code-review itself found zero defects; all findings were coverage/docs gaps):

- **`KrowdKontrolHUDWiringTest.cpp`**: added an assertion after the first
  controller/pawn's `AbilityTrayWidget`/`EnergyMeterWidgetInstance` checks confirming
  `Controller->QuestTrackerWidgetInstance->GetSuggestedAbilityDisplayText()` reads the
  universal fallback once `WireWidgetsToPawn()` has run — the file's only change to
  `KrowdKontrolPlayerController.cpp` (the `QuestTrackerWidgetInstance` bind) previously
  had zero test coverage through the real pawn/controller path. Needed a new
  `#include "QuestTrackerWidget.h"` (the header was only forward-declared before).
- **`KrowdKontrolQuestTrackerWidgetTest.cpp`** case (14): new case proving the live
  `OnAbilityUnlocked`-while-bound path — binds while only Stun is unlocked, then
  `NotifyLevelReached(2)` broadcasts live, asserting the suggestion flips without any
  re-bind. Cases (12)/(13) only ever proved the bind-time seed path.
- Case (12) extended (12b): after the colour-matched assertion, drives the Sniper
  through a real `TickCheckDetection`→`ReceiveControl`→`TransitionToBanked`
  progression (mirroring `ARoomActor::HandleZoneActorBanked`'s real sequence) plus
  the zone's `OnActorBanked` broadcast, asserting the suggestion recomputes back to
  the Stun fallback — proves `HandleActorBanked()` actually calls
  `RefreshSuggestedAbilityDisplay()`, not just increments the banked count. Required
  adding `FKrowdKontrolQuestTrackerWidgetTest` to `EnemyBase.h`'s existing
  test-friendship list (same established pattern as `FKrowdKontrolHUDWiringTest` etc.)
  to reach the private `TickCheckDetection`.
- New case (15): multi-remaining-enemy-type tie-break — Sniper + Trooper both alive,
  first with only Root unlocked (asserts Root wins, proving a real per-candidate
  `IsAbilityUnlocked()` scan rather than a fixed enum-value pick), then Sleep also
  unlocked live (asserts declaration order picks Sleep instead). Pins down the
  behaviour `ComputeSuggestedAbility()`'s own comment flags as arbitrary-but-untested.
- `docs/prd-mission-briefing-tracker.md` REQ-2: swapped stale numeric key examples
  (`(2)`/`(1)`) for the ratified `(RMB)`/`(LMB)` labels, added the
  `🟡 partially implemented` status line and a `**Ratified (operator, 2026-08-23)**`
  annotation matching `docs/prd-ability-tray-ux.md`'s established shape; also fixed
  REQ-1's same-file drive-by stale `"PRESS 2"` example to `"RMB"`.

All 4 test-file/header edits applied to both `app/` and `app-source-tracked/`
identically (`diff` clean, verified after each edit). Full `harness/ci.py` gate
(build + all 99 `KrowdKontrol.Unit.*` tests + the 1-step E2E smoke) reported
`GATE_OK mode=full` after these changes; an initial `UE_AUTOMATION_FAILED` on
`FlatCamera3DPrototypePawnCursorWorldPosition` was the known
`LogModelContextProtocol: Error: Call to unknown method "server/discover"` flake
(fails whichever test happens to be running when it fires) — a clean rerun confirmed
it was not a regression from this change.

---

The real Unreal project stays under `app/` (gitignored, D-003) — this changelog and
its matching `app-source-tracked/` copy are the tracked-repo record of that change,
per D-009. Not a substitute for reading `app-source-tracked/` directly.
