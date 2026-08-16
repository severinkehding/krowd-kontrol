# Issue #34: Onboarding: brief non-blocking on-screen prompt system (max ~2s, no pause)

Adds `UOnScreenPromptWidget`, a reusable UMG `UUserWidget` that displays a short text
cue during live gameplay via a single `ShowPrompt(FText)` call, auto-dismissing after a
hard-capped ~2 seconds without ever pausing the game or intercepting player input (PRD
09 REQ-4). It mirrors the `NativeOnInitialized`/`Initialize`/`NativeTick` lifecycle and
per-frame countdown pattern already used by `UAbilityCooldownTrayWidget`, and is now
covered by the `ReservedGameplayColours` chrome-colour audit. Per the issue's own
stated scope boundary, this lands only the prompt widget itself — not what triggers a
prompt; that's explicitly deferred to future onboarding work (e.g. PRD 09 REQ-5).

## Files changed

| File | Action | What it contains |
|------|--------|-------------------|
| `app-source-tracked/Source/KrowdKontrol/OnScreenPromptWidget.h` | CREATE | `UOnScreenPromptWidget` declaration — `ShowPrompt`/`AdvanceDismissTimer`/state accessors, `MaxPromptDurationSeconds = 2.0f`, friend classes for test access. |
| `app-source-tracked/Source/KrowdKontrol/OnScreenPromptWidget.cpp` | CREATE | Widget tree construction (top-center anchored `UBorder`+`UTextBlock`, reserved-colour-safe chrome palette copied from `UAbilityCooldownTrayWidget`), `ShowPrompt`/`AdvanceDismissTimer` implementation, and the dev-only `KrowdKontrol.ShowOnScreenPrompt` console command. |
| `app-source-tracked/Source/KrowdKontrol/Private/Tests/KrowdKontrolOnScreenPromptWidgetTest.cpp` | CREATE | `KrowdKontrol.Unit.OnScreenPromptWidget` — idle state, show/dismiss timing, cap enforcement, negative/oversized duration clamping, no-stacking replace semantics, `NativeTick` wiring, lifecycle guard (both call orders), unbuilt-tree safety, `AddToViewport` smoke check. |
| `app-source-tracked/Source/KrowdKontrol/Private/Tests/KrowdKontrolReservedGameplayColoursTest.cpp` | UPDATE | Extended the existing chrome-colour audit with a new block for `UOnScreenPromptWidget->PromptBorder`/`PromptText`, per the file's own "add any future HUD widget's chrome to this list" doc-comment. |

## Acceptance criteria

- [x] **A reusable on-screen prompt widget (`UOnScreenPromptWidget`) can display a
      short text cue during live gameplay via `ShowPrompt(FText)`.** Implemented as a
      standalone `UUserWidget`, following the same standalone-class-plus-dev-console-
      command precedent as every other HUD widget in this module (no
      `AHUD`/`APlayerController` exists yet to wire into).
- [x] **Showing a prompt never pauses the game or blocks player input.** The widget's
      chrome only ever uses `ESlateVisibility::Collapsed` (idle) or
      `HitTestInvisible` (showing) — never `Visible` — which structurally guarantees it
      can't intercept input. Verified by test case (c).
- [x] **Each prompt auto-dismisses at the ~2 second cap; no caller can make it persist
      or stack past it.** `ShowPrompt()` clamps `DurationSeconds` to
      `[0, MaxPromptDurationSeconds]` (2.0f) unconditionally, and re-triggering while a
      prompt is showing replaces rather than queues (single message slot). Verified by
      test cases (e)/(g)/(i).
- [x] **The prompt widget's chrome does not use any of the five reserved gameplay
      colours.** Chrome background `FLinearColor(0.05, 0.05, 0.05, 0.92)` and text
      colour `FLinearColor(0.85, 0.85, 0.85, 1.0)` — copied verbatim from
      `UAbilityCooldownTrayWidget`'s already-reviewed palette — don't match
      Purple/Teal/Orange/Blue/White. Now covered by an automated assertion, not just
      inspection: `KrowdKontrolReservedGameplayColoursTest.cpp`'s new block.
- [x] **An automation test confirms a triggered prompt is dismissed within the ~2s cap
      and never becomes hit-testable while shown.** `KrowdKontrol.Unit.OnScreenPromptWidget`
      — see validation evidence below.

## Validation evidence

Full gate, run against a real, freshly-rebuilt `UnrealEditor-KrowdKontrol.dll` (see
Deviations below for why a real rebuild was necessary here):

```
HARNESS_START mode=full driver=cli
STATIC_SKIPPED no 'static' command in harness.config.json
UNIT_PASSED tests=25
APP_STARTED driver=cli
UE_AUTOMATION_RESULT passed=1 total=1
UE_AUTOMATION_OK
E2E_PASSED steps=1
HOLDOUT_ABSENT no .factory/holdout/run.py
MUTATIONS_ABSENT no harness/mutations/run.py
GATE_OK mode=full
```

`tests=25` (up from the pre-existing 24) confirms the new
`KrowdKontrol.Unit.OnScreenPromptWidget` test is genuinely discovered and run.
Scoped tests, run explicitly as extra evidence:

```
$ harness/run_ue_automation.sh KrowdKontrol.Unit.OnScreenPromptWidget
UE_AUTOMATION_RESULT passed=1 total=1
UE_AUTOMATION_OK

$ harness/run_ue_automation.sh KrowdKontrol.Unit.ReservedGameplayColours
UE_AUTOMATION_RESULT passed=1 total=1
UE_AUTOMATION_OK
```

Gate passed on first run in the validation pass; no fixes needed. Hard invariant #3
(5 reserved gameplay colours locked) verified by inspection + the automated audit
above; invariants #1/#2/#4/#5/#6/#7/#8 not implicated (pure UI widget, no
governance/ability/enemy/roster/engine/networking change, `app/` tracking handled
correctly per D-009).

## Deviations from plan

- `ShowPrompt`'s default parameter could not be `MaxPromptDurationSeconds` as
  originally planned — UnrealHeaderTool only accepts a literal for a `UFUNCTION`
  default parameter, not a symbolic static-member reference (`C++ Default parameter
  not parsed`). Changed the header's default to a literal `2.0f` with an explanatory
  comment tying it back to `MaxPromptDurationSeconds`; the actual cap is still
  structurally enforced by `ShowPrompt()`'s `FMath::Clamp`, and test case (c) asserts
  the default really does resolve to `2.0f`.
- Discovered (not part of the plan, and independently also hit by issue #56's
  implementation) that this repo's harness silently validates against a stale,
  never-rebuilt DLL — `harness/ci.py`/`run_ue_automation.sh` only launch the
  already-built Editor binary, they don't trigger a `UnrealBuildTool` compile. The
  first `ci.py --quick` run after adding these files reported the pre-existing
  `tests=24` with the new test silently absent. Forced a real rebuild (engine-bundled
  `dotnet.exe`, since system `dotnet` was only 8.x and `UnrealBuildTool.dll` needs
  .NET 10) + `UnrealBuildTool.dll KrowdKontrolEditor Win64 Development`, which is what
  surfaced the UHT default-parameter error above. After the fix, a real rebuild
  succeeded and the gate above reflects `tests=25`. Not fixing the harness itself here
  (out of scope for this issue) — flagging it so downstream validation doesn't trust a
  `GATE_OK` from a stale build without re-running a real compile first.

Otherwise implementation matched the plan exactly: same class shape, same lifecycle
mirroring `UAbilityCooldownTrayWidget`, same chrome palette, same top-center anchor
placement, same dev console command pattern (`KrowdKontrol.ShowOnScreenPrompt`), same
test coverage.

## Closing note on `app-source-tracked/`

`app/` itself is a gitignored symlink to the real Unreal project on the Windows host
(D-003) — binary assets can't live in this repo without LFS set up ahead of time. Per
D-009, this PR's tracked-repo diff is the `app-source-tracked/` mirror of the four
files above, copied verbatim at PR-creation time, plus this changelog — `app/` stays
exactly as-is and this mirror is what gives GitHub something to hang a PR on and
reviewers real code to check.
