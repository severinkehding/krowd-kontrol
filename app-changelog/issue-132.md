# Issue #132: Wire HUD widgets (ability tray, energy meter) into the playable levels

Every HUD widget shipped so far (`UAbilityCooldownTrayWidget` from #66/#68/#129,
`UEnergyMeterWidget` from #64) was fully implemented and unit-tested but never
actually reachable by a player: no `APlayerController`/`AHUD`/`AGameModeBase`
subclass existed anywhere in the module, and neither playable level
(`L_FlatCamera3DPrototype`, `L_Paper2DPrototype`) had a GameMode configured. Both
pawns self-possess via `AutoPossessPlayer`, so the engine's stock
`APlayerController` ran movement fine — nobody ever swapped it for a class that
owned HUD state.

This adds `AKrowdKontrolPlayerController` (creates both widgets in `BeginPlay()`,
adds them to the viewport, and binds each pawn's `UAbilityUnlockComponent`/
`UPlayerEnergyComponent` to the tray/meter in `OnPossess()`) and
`AKrowdKontrolGameMode` (whose only job is pointing `PlayerControllerClass` at the
new controller — a `PlayerController` subclass alone would not have been picked up
without this). `Config/DefaultEngine.ini` gets a project-wide
`GlobalDefaultGameMode=/Script/KrowdKontrol.KrowdKontrolGameMode` key so the engine
actually spawns it on both maps.

As a necessary corollary, both `AFlatCamera3DPrototypePawn` and
`APaper2DPrototypePawn` now construct a `UPlayerEnergyComponent` — without it,
`UEnergyMeterWidget::BindToEnergyComponent()` had nothing live to bind to, and
`AEnemyBase::FindPlayerEnergyComponent()` (already-merged enemy-contact-damage code)
was a silent no-op on both playable levels. This makes that existing, already-
reviewed damage path live for the first time; it is not new gameplay logic.

**Scope note**: the issue body also lists "hooks for `APlaceholderTargetZoneActor`
beacons." The OWNER's accepted-triage comment narrowed accepted scope to wiring the
two already-merged widgets, and `APlaceholderTargetZoneActor` has zero existing
UI/widget hook to wire (grep-confirmed) — building one would be net-new widget
feature work (new PRD-13 visual spec, new colour-safety review), not wiring. Left
out of scope here per the investigation's decision; flagged for a separate issue if
still wanted.

## Files changed

All `.h`/`.cpp` files below were written identically to `app/Source/KrowdKontrol/...`
(the real project, gitignored per D-003) and mirrored into `app-source-tracked/` per
D-009, so this is a plain-text copy for review — `app/` itself is unchanged in kind.

| File | Action | What it contains |
|------|--------|-------------------|
| `app/Source/KrowdKontrol/KrowdKontrolPlayerController.h` | CREATE | `AKrowdKontrolPlayerController`: owns `AbilityTrayWidget`/`EnergyMeterWidgetInstance`, `BeginPlay()`/`OnPossess()` overrides |
| `app/Source/KrowdKontrol/KrowdKontrolPlayerController.cpp` | CREATE | `CreateHUDWidgets()` (idempotent construct + `AddToViewport()`), `WireWidgetsToPawn()` (binds unlock/energy components via null-safe `Bind*` calls) |
| `app/Source/KrowdKontrol/KrowdKontrolGameMode.h` | CREATE | `AGameModeBase` subclass, no other overrides |
| `app/Source/KrowdKontrol/KrowdKontrolGameMode.cpp` | CREATE | Constructor sets `PlayerControllerClass = AKrowdKontrolPlayerController::StaticClass()` |
| `app/Source/KrowdKontrol/FlatCamera3DPrototypePawn.h`/`.cpp` | UPDATE | Adds `UPlayerEnergyComponent` subobject, mirroring the existing `AbilityUnlockComponent` pattern |
| `app/Source/KrowdKontrol/Paper2DPrototypePawn.h`/`.cpp` | UPDATE | Same `UPlayerEnergyComponent` addition (no `AbilityUnlockComponent` added here — not requested, tray's `BindAbilityUnlockComponent(nullptr)` no-ops safely) |
| `app/Source/KrowdKontrol/Private/Tests/KrowdKontrolHUDWiringTest.cpp` | CREATE | `KrowdKontrol.Unit.HUDWiring` — spawns pawn + controller, asserts both widgets construct on `BeginPlay()` and the tray reflects bound unlock state (Stun unlocked, Sleep locked) |
| `app/Config/DefaultEngine.ini` | UPDATE | Adds `GlobalDefaultGameMode=/Script/KrowdKontrol.KrowdKontrolGameMode` under `[/Script/EngineSettings.GameMapsSettings]` — **not mirrored to `app-source-tracked/`** (D-009's `.h`/`.cpp`/`.Build.cs`-only scope), so this line is invisible in the tracked diff; called out here explicitly per the investigation's own flag |

## Acceptance criteria

- [x] **A PlayerController (or HUD/GameMode) subclass creates and adds to viewport
      `UAbilityCooldownTrayWidget` (with locked-slot state from #68/#129) and
      `UEnergyMeterWidget`.** `AKrowdKontrolPlayerController::CreateHUDWidgets()`
      does both, idempotently, in `BeginPlay()`.
- [x] **Both playable maps' GameModes/pawns are wired to use it.** Project-global
      `GlobalDefaultGameMode` (no per-map World Settings override exists in either
      `.umap`) applies to both `L_FlatCamera3DPrototype` and `L_Paper2DPrototype`.
- [x] **Corner-anchor placement per the widgets' own already-merged specs.** No new
      positioning code needed or added — both widgets already self-anchor via
      `UCanvasPanelSlot` inside their own `BuildWidgetTree()` (tray bottom-right,
      meter top-left); the controller only constructs and calls `AddToViewport()`.
- [x] **An automation test asserting the widgets are constructed and added on level
      start.** `KrowdKontrol.Unit.HUDWiring` covers construction, `AddToViewport()`
      no-crash, and bound-state correctness (Stun unlocked / Sleep locked).
- [ ] **Target-zone-beacon HUD hooks.** Out of scope per the OWNER's accepted-triage
      comment (see Scope note above) — nothing exists to wire; would be net-new
      widget work requiring its own spec.

## Validation

`python harness/ci.py` (full mode):

```
HARNESS_START mode=full driver=cli
STATIC_SKIPPED no 'static' command in harness.config.json
UNIT_PASSED tests=36
APP_STARTED driver=cli
UE_AUTOMATION_RESULT passed=1 total=1
UE_AUTOMATION_OK
E2E_PASSED steps=1
HOLDOUT_ABSENT no .factory/holdout/run.py
MUTATIONS_ABSENT no harness/mutations/run.py
GATE_OK mode=full
```

36/36 `KrowdKontrol.Unit.*` tests pass (including the new
`KrowdKontrol.Unit.HUDWiring`, up from 33 pre-implementation), plus a real UE
Automation Framework run (1/1) and a full E2E pass (1/1 step) — all against a binary
actually rebuilt from this change's source (confirmed via direct
`UnrealBuildTool.dll` invocation during implementation, not just the harness's
launch-only path, after a stale-binary false-green was caught and ruled out). Hard
invariants #1-#8 reviewed against the diff and not implicated / holding — see
`validation.md` Phase 3.

## Notes

Three real bugs were found and fixed by actually rebuilding the module rather than
trusting the investigation's draft test as written: a `TestNotNull`/`TObjectPtr`
template-deduction compile error (fixed with this codebase's established
`ToRawPtr(...)` wrapper), a `BeginPlay()` state-precondition ensure (fixed by calling
`DispatchBeginPlay()` instead, which also made the draft's `friend class` declaration
unnecessary — removed), and a `CreateWidget()` runtime failure requiring a real
`ULocalPlayer` rather than just the `SetAsLocalPlayerController()` bool flag (fixed
by constructing one via `NewObject<ULocalPlayer>(GEngine)` in the test). No
production-code deviations from the investigation's plan.

The full implementation and validation record for this issue lives in
`/home/severin/.archon/workspaces/severinkehding/krowd-kontrol/artifacts/runs/f9703f816ff3c8ea4435d92912bf374e/implementation.md`
and `validation.md`. `app/` itself (the gitignored symlink to the real Unreal
project, CLAUDE.md's Environment section / `.factory/decisions.md` D-003) is
unchanged in kind by this tracked copy — the files above under
`app-source-tracked/` are a plain-text mirror made at PR-creation time (D-009) so
GitHub has a non-empty diff to open a PR against and reviewers have real source to
check, not a description of it.
