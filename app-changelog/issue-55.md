# Issue #55: Fix Paper2D prototype pawn's camera composed-rotation bug

Adds the Paper2D half of PRD 14 REQ-1's Paper2D-vs-flat-camera-3D pipeline comparison:
`APaper2DPrototypePawn`, a minimal top-down prototype pawn with a genuine orthographic
top-down camera, `UFloatingPawnMovement`-driven WASD/arrow input in world space, an
Automation Framework smoke test confirming the wiring, and a corrected
`L_Paper2DPrototype.umap` containing a placed pawn instance.

Two prior attempts (PR #100, PR #106) built this pawn with `CameraBoom` attached to
`SpriteComponent` (the pawn's root, itself rotated `-90°` pitch to lay the sprite flat
into the ground plane). `CameraBoom`'s own `-90°` relative pitch, intended as an
independent straight-down camera, instead composed with the sprite's tilt — verified by
direct quaternion multiplication of UE's actual `FRotator::Quaternion()` formula, two
`-90°` pitches through a rotated parent compose to a full `180°` rotation about world Y,
i.e. the camera ends up facing roughly horizontally backward-and-upside-down, not
straight down. No prior test caught this because every rotation assertion checked
`GetRelativeRotation()` (correct in isolation) rather than the composed world rotation a
player actually sees.

## Files changed

| File | Action | What it contains |
|------|--------|-------------------|
| `app-source-tracked/Source/KrowdKontrol/Paper2DPrototypePawn.h` | CREATE | Pawn declaration: `PawnRoot` (new, `USceneComponent`, unrotated root), `SpriteComponent` (`UPaperSpriteComponent`), `MovementComponent` (`UFloatingPawnMovement`), `CameraBoom` (`USpringArmComponent`), `TopDownCamera` (`UCameraComponent`), `SetupPlayerInputComponent()` override |
| `app-source-tracked/Source/KrowdKontrol/Paper2DPrototypePawn.cpp` | CREATE | Constructor building the fixed hierarchy: `PawnRoot` as `RootComponent` (identity rotation); `SpriteComponent` and `CameraBoom` attached to it as siblings, each independently `-90°` pitch, no longer composing through each other; `MovementComponent->SetUpdatedComponent(PawnRoot)` so translating the pawn moves sprite and camera together; genuinely orthographic `TopDownCamera` (`OrthoWidth = 1024`); `/Paper2D/DummySprite.DummySprite` assigned as the default sprite asset; `MoveForward`/`MoveRight` bound to world-space `AddMovementInput` |
| `app-source-tracked/Source/KrowdKontrol/KrowdKontrol.Build.cs` | UPDATE | Added `"Paper2D"` to `PublicDependencyModuleNames` — the one line this file needed, no other changes |
| `app-source-tracked/Source/KrowdKontrol/Private/Tests/KrowdKontrolPaper2DPipelineSmokeTest.cpp` | CREATE | `KrowdKontrol.Unit.Paper2DPipelineSmoke` (component wiring, root/attachment checks, both relative *and* `GetComponentRotation()` world-space pitch assertions on `CameraBoom` and `TopDownCamera`, orthographic projection, input-binding delegate invocation), `KrowdKontrol.Unit.Paper2DPipelineMovement` (simulated-tick location-change regression), `KrowdKontrol.Unit.Paper2DPipelineLevelHasConfiguredPawn` (loads the real `.umap`, re-asserts world-space pitch against the placed instance) |
| `docs/paper2d-prototype-notes.md` | CREATE | Friction/iteration notes, headlined by the composed-rotation bug, its fix, and the placed-level repair this pass also needed |
| `app-changelog/issue-55.md` | CREATE | This file |

`app/`'s copy of all four source/test files was updated identically (byte-for-byte,
confirmed via `diff`), since `harness/run_ue_automation.sh` builds and runs against
`app/`, not the tracked mirror — without this the new tests would exist on paper but
never actually execute. `app/`'s `KrowdKontrol.Build.cs` already had `Paper2D` from a
prior attempt (left untouched); the tracked mirror's `Build.cs` was deliberately based
on its own current baseline (still carrying the commented-out `Slate`/`SlateCore`
lines) rather than a copy of `app/`'s real `Build.cs`, which also carries an unrelated,
not-yet-mirrored UMG block for `PostRunSummaryWidget` (issue #74) — copying that over
would have been undisclosed scope creep, the exact HIGH-severity finding that got PR
#100 rejected.

### The placed level also needed a live repair, not just a C++ fix

`L_Paper2DPrototype.umap` already existed in the live `app/` project from a prior
attempt, placed against the old (buggy) class shape. After the C++ fix compiled clean,
`KrowdKontrol.Unit.Paper2DPipelineLevelHasConfiguredPawn` — which loads the real
`.umap` rather than spawning a fresh instance — initially failed where the
`CreateNewMap()`-based tests passed. Unreal MCP was reachable this session
(`scripts/ue_editor_launch_and_wait.sh` brought the Editor up with a responding MCP
endpoint), so this was inspected and fixed live rather than left as an
environment-blocked follow-up:

1. **The placed actor's own transform carried a leftover `-90°` pitch** (likely an
   earlier manual attempt to visually compensate for the broken camera by rotating the
   whole pawn) — composed on top of every child component's own rotation. Reset to
   identity via `ActorTools.set_actor_transform`, keeping the actor's placement
   location.
2. **`CameraBoom`'s `AttachParent` was still serialized as `SpriteComponent`** — the
   *old* attachment target from before this fix — even though the freshly-compiled
   class's constructor now attaches it to `PawnRoot`. A C++ class's default-attachment
   change does not automatically propagate to an already-placed instance. Reparented
   live via `ActorTools.set_parent_component`, then restored `CameraBoom`'s relative
   pitch to `-90°` via `ObjectTools.set_properties` (reparenting preserves world
   transform, not relative transform, so the relative value needed resetting
   afterward).

Every step was verified with `ObjectTools.get_properties`/`ActorTools.get_parent_component`
before saving via `AssetTools.save_assets`, and the Editor was closed
(`scripts/ue_editor_close.sh`) before the next headless build/test run per
`CLAUDE.md`/D-013. `KrowdKontrol.Unit.Paper2DPipelineLevelHasConfiguredPawn` passes
against the resaved level — see Validation below.

## Acceptance criteria

- [x] **`KrowdKontrol.Build.cs` enables the Paper2D module dependency** — confirmed in
      both `app-source-tracked/` (this PR) and `app/` (already present).
- [x] **`L_Paper2DPrototype` level contains one Paper2D sprite-based Pawn movable via
      WASD/arrow input, with a camera genuinely, world-space-verifiably locked to an
      orthographic top-down view** — confirmed by
      `KrowdKontrol.Unit.Paper2DPipelineLevelHasConfiguredPawn` against the live,
      repaired `.umap`, asserting `GetComponentRotation()` (not just relative
      rotation) on both `CameraBoom` and `TopDownCamera`.
- [x] **`KrowdKontrol.Unit.Paper2DPipelineSmoke` confirms the pawn class exists,
      spawns correctly, and asserts the camera's composed world-space pitch, not only
      its relative pitch** — passes; see Validation.
- [x] **`docs/paper2d-prototype-notes.md` records real setup friction, headlined by the
      composed-rotation bug and its fix** — also documents the placed-level repair.
- [x] **Level 1-3 validation commands pass with exit 0** — see Validation below.
- [x] **No file under `.archon/` is touched** — `git status`/`git diff` for this change
      touch only `app-source-tracked/`, `docs/`, and `app-changelog/`.
- [x] **This changelog makes no claim not backed by this pass's actual output** —
      including the level fix, which was live-verified via MCP `get_properties`/
      `get_parent_component` calls after each write, not assumed.

## Validation

```
$ python harness/ci.py
HARNESS_START mode=full driver=cli
STATIC_SKIPPED no 'static' command in harness.config.json
UNIT_PASSED tests=24
APP_STARTED driver=cli
UE_AUTOMATION_RESULT passed=1 total=1
UE_AUTOMATION_OK
E2E_PASSED steps=1
HOLDOUT_ABSENT no .factory/holdout/run.py - NOTHING above the independence line ran.
MUTATIONS_ABSENT no harness/mutations/run.py - this gate has never been shown to fail.
GATE_OK mode=full
```

`UNIT_PASSED tests=24` covers all three new `KrowdKontrol.Unit.Paper2DPipeline*` tests
(`Smoke`, `Movement`, `LevelHasConfiguredPawn`) alongside every other pre-existing
`KrowdKontrol.Unit.*`/`KrowdKontrol.Smoke.*` test — no regressions.

## Not addressed

- **Legacy `BindAxis` vs. the project's configured `UEnhancedInputComponent`.** As with
  the flat-camera-3D sibling, this pass's smoke test invokes the bound delegates
  directly rather than through a live PIE input event. Open question carried forward in
  `docs/paper2d-prototype-notes.md`.
- **The Paper2D-vs-flat-camera-3D pipeline decision itself** — out of scope per Hard
  Invariant 6; this issue only makes the Paper2D prototype's camera actually work so
  that comparison can happen.
