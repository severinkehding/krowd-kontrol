# Issue #40: Onboarding — additional-help nudge for repeated non-matched ability use

Wires a new `UAbilityMatchupNudgeComponent` into `AFlatCamera3DPrototypePawn`'s
constructor, bound to the already-merged
`UAbilityMatchupSignalComponent::OnAbilityMatchupSignal` delegate (issue #37). It
counts consecutive non-colour-matched successful casts, resets the counter on any
matched cast, and — once 3 consecutive misses are reached, one time only per pawn
instance — calls the already-merged `UOnScreenPromptWidget::ShowPrompt()` (issue
#34/PR #113) with a short reminder. Also gives `AKrowdKontrolPlayerController` a
production-owned `OnScreenPromptWidgetInstance` (created in `CreateHUDWidgets()`,
same as `AbilityTrayWidget`/`EnergyMeterWidgetInstance`) since `UOnScreenPromptWidget`
previously had no production owner — only a dev console command created one.

**Threshold decision** (per the issue's own instruction to note it): **3 consecutive
non-colour-matched successful casts** triggers the nudge, resetting to 0 on any
colour-matched cast, firing at most once per pawn instance.

## Files changed (all under `app/`, gitignored per D-003 — this is the tracked-repo
record of that change)

| File | Action | What it contains |
|------|--------|-------------------|
| `app/Source/KrowdKontrol/AbilityMatchupNudgeComponent.h` | CREATE | `UAbilityMatchupNudgeComponent` declaration - `HandleAbilityMatchupSignal` public handler, `ResolvePromptWidget()` private resolver, one-shot/counter/warn-once state, `NonMatchedCastThreshold = 3` |
| `app/Source/KrowdKontrol/AbilityMatchupNudgeComponent.cpp` | CREATE | Counts consecutive non-matched casts, resets on a matched cast, fires `ShowPrompt()` once at threshold via a controller-resolved `UOnScreenPromptWidget`, mirroring `UGizmoFirstContactComponent::ResolveNarrativeSubsystem()`'s lazy-resolve/cache/warn-once pattern |
| `app/Source/KrowdKontrol/FlatCamera3DPrototypePawn.h` | UPDATE | Forward-declares `UAbilityMatchupNudgeComponent`, adds its `TObjectPtr` property |
| `app/Source/KrowdKontrol/FlatCamera3DPrototypePawn.cpp` | UPDATE | `#include`, `CreateDefaultSubobject`, `AddDynamic` bind to `AbilityMatchupSignalComponent->OnAbilityMatchupSignal` — immediately after the existing `AbilityMatchupSignalComponent` create+bind pair |
| `app/Source/KrowdKontrol/KrowdKontrolPlayerController.h` | UPDATE | Forward-declares `UOnScreenPromptWidget`, adds `OnScreenPromptWidgetInstance` property (no `WireWidgetsToPawn()` entry — the nudge component reaches out to the controller instead) |
| `app/Source/KrowdKontrol/KrowdKontrolPlayerController.cpp` | UPDATE | `#include "OnScreenPromptWidget.h"`, idempotent create + `AddToViewport()` in `CreateHUDWidgets()`, mirroring the `EnergyMeterWidgetInstance` block verbatim |
| `app/Source/KrowdKontrol/Private/Tests/KrowdKontrolAbilityMatchupNudgeComponentTest.cpp` | CREATE | `KrowdKontrol.Unit.AbilityMatchupNudgeComponent` — 6 cases: below threshold, threshold reached, one-shot no-repeat, matched-cast resets the counter, missing-widget defensive, real-pawn constructor wiring |

## Acceptance criteria

- [x] 3 consecutive non-colour-matched successful casts trigger exactly one
      `UOnScreenPromptWidget::ShowPrompt()` call — not before, not more than once per
      pawn instance (cases a/b)
- [x] The nudge never pauses the game or blocks input — enforced transitively by
      reusing `UOnScreenPromptWidget::ShowPrompt()` unmodified
- [x] The nudge does not repeat indefinitely — `bHasShownNudge` one-shot guard,
      verified by case (c)
- [x] A matched cast in the middle of a near-miss streak resets progress instead of
      combining toward the threshold (case d)
- [x] No `AKrowdKontrolPlayerController`/widget resolvable — no crash, warns once
      (case e)
- [x] Real pawn constructor wiring confirmed (case f)
- [x] All pre-existing `KrowdKontrol.Unit.*` tests still pass

## Validation

`python harness/ci.py --quick`: `GATE_OK` (`STATIC_SKIPPED`, `UNIT_PASSED tests=61`),
including the new `KrowdKontrol.Unit.AbilityMatchupNudgeComponent` test and no
regression in any pre-existing test.

Note (test-harness-only finding, not a production issue): `FAutomationEditorCommonUtils::CreateNewMap()`'s
World never runs `World->InitializeActorsForPlay()`, so `PostInitializeComponents()` —
and therefore `UWorld::AddController()` — never fires automatically for actors spawned
into it under this Automation harness, unlike real gameplay (PIE/packaged). The new
test calls `World->AddController(Controller)` explicitly to match what the engine does
automatically outside this harness, so `UAbilityMatchupNudgeComponent::ResolvePromptWidget()`'s
`World->GetFirstPlayerController()` lookup (unmodified, real production code) can be
exercised under test.

---

Source lives under `app/` (gitignored, D-003) — this file is the tracked-repo record
of that change, not a substitute for reading the actual code.
