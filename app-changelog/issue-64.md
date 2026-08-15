# Issue #64: Add corner-anchored energy meter HUD widget

Adds `UEnergyMeterWidget`, a UMG widget built entirely in C++ (no Widget Blueprint)
that shows the player's energy level as a top-left-corner-anchored bar/gauge with a
numeric readout, per PRD 13 REQ-1/REQ-2 (diagonally opposite the ability tray from
issue #66). No real player-energy gameplay system existed at the time this issue was
scoped, so the widget seeds a self-demonstrating placeholder value (72/100) on
construction; `BindToEnergyComponent(UPlayerEnergyComponent*)` is the real wiring
point, extending the existing energy primitive from issue #78 rather than adding a
second, unrelated placeholder value.

## Files changed (all under `app/`, gitignored per D-003 — this is the tracked-repo
record of that change, see the closing note below)

| File | Action | What it contains |
|------|--------|-------------------|
| `app/Source/KrowdKontrol/EnergyMeterWidget.h` | CREATE | `UEnergyMeterWidget` declaration: builds its own `UCanvasPanel`/`UProgressBar`/`UTextBlock` tree in `NativeOnInitialized()`, top-left-anchored, plus `BindToEnergyComponent(UPlayerEnergyComponent*)` for live wiring to the real energy primitive. |
| `app/Source/KrowdKontrol/EnergyMeterWidget.cpp` | CREATE | Implementation: lazy widget-tree construction guard, placeholder seeding (72/100), clamping, `OnEnergyChanged`-driven live updates, chrome colours restricted to near-black background + light-gray text (no reserved gameplay-info colours). |
| `app/Source/KrowdKontrol/Private/Tests/KrowdKontrolEnergyMeterWidgetTest.cpp` | CREATE | `KrowdKontrol.Unit.EnergyMeterWidget` — covers placeholder/clamping, live `OnEnergyChanged`-driven updates via a real `ApplyContactDamage()` call, rebind isolation, corner anchoring, chrome-colour compliance, and lifecycle safety (unbuilt-tree null-safety paths). |
| `app/Source/KrowdKontrol/PlayerEnergyComponent.h` | UPDATE | Added a second `friend class` line so the new test can directly seed `CurrentEnergy`, alongside the existing friend declaration. |

Also fixes two pre-existing bugs discovered while getting a real build+test cycle to
run (both blocked validation for the whole project, not just this issue — see
`implementation.md` for the full account):

| File | Action | What it fixes |
|------|--------|-------------------|
| `app/Source/KrowdKontrol/PostRunSummaryWidget.cpp` | UPDATE | `EnsureWidgetTreeBuilt()` crashed the Editor when `NativeOnInitialized()` ran before a real `WidgetTree` existed — added the same lazy-init guard `AbilityCooldownTrayWidget` (issue #66) already has. No tracked-source mirror exists for this file (its own issue, #74, never merged), so this fix lives only in `app/`. |
| `harness/run_ue_automation.sh` | UPDATE | Pass-count parser summed `succeeded` but not `succeededWithWarnings`, undercounting real passes (e.g. this issue's own test, which deliberately exercises a warning-logging null-safety path). |

## Acceptance criteria

- [x] **A new HUD widget (UMG) displays the player's current energy level as a
      bar/gauge-style meter.** `UEnergyMeterWidget` builds a `UProgressBar` fill +
      numeric `UTextBlock` readout in C++.
- [x] **Anchored to one screen corner; does not overlap the central play area.**
      Top-left-anchored via `UCanvasPanel`, covered by the test's corner-anchoring
      assertion.
- [x] **Wired to a minimal placeholder/test energy value demonstrating live
      updates, without creating a new gameplay system.** Seeds a placeholder 72/100
      on construction; `BindToEnergyComponent()` wires to the existing
      `UPlayerEnergyComponent` (issue #78) as the real update source, exercised in
      the test via a real `ApplyContactDamage()` call.
- [x] **Chrome (border, background) avoids the five reserved gameplay-information
      colours (Purple/Teal/Orange/Blue/White).** Background is near-black
      `(0.05, 0.05, 0.05, 0.92)`, text is light-gray `(0.85, 0.85, 0.85, 1.0)`; the
      only saturated colour is the green energy-fill bar, which is informational
      fill, not chrome. Confirmed by inspection in validation Phase 3.
- [x] **An Automation Framework test confirms the widget is added to the viewport
      and its fill visually responds to the backing value changing.**
      `KrowdKontrol.Unit.EnergyMeterWidget` covers this plus clamping, rebind
      isolation, and lifecycle safety.

## Validation evidence

```
HARNESS_START mode=full driver=cli
STATIC_SKIPPED no 'static' command in harness.config.json
UNIT_PASSED tests=9
APP_STARTED driver=cli
UE_AUTOMATION_RESULT passed=1 total=1
UE_AUTOMATION_OK
E2E_PASSED steps=1
HOLDOUT_ABSENT no .factory/holdout/run.py
MUTATIONS_ABSENT no harness/mutations/run.py
GATE_OK mode=full
```

Full mode, first run, no fixes needed (`validation.md`). Hard invariants checked by
inspection (5-colour lock, no-kill rule, no networking) — no regressions found.

## Closing note on `app-source-tracked/`

`app/` itself is a gitignored symlink to the real Unreal project on the Windows host
(D-003) — binary assets can't live in this repo without LFS set up ahead of time.
`app-source-tracked/` mirrors the plain-text `.h`/`.cpp` files touched by this issue
into the tracked repo (D-009) so this PR carries real, reviewable source instead of
just a description of it. `app/` stays exactly as-is; this is a copy for review, not a
new live link.
