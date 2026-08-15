# Issue #55: Prototype: minimal Paper2D top-down movement test scene

Builds the Paper2D half of PRD 14 REQ-1's Paper2D-vs-flat-camera-3D pipeline
comparison. Adds `APaper2DPrototypePawn` (`UPaperSpriteComponent` root laid flat
into the top-down ground plane, `UFloatingPawnMovement`, a spring-arm-mounted
**orthographic** top-down `UCameraComponent`, legacy `BindAxis` WASD/arrow input
reusing the `MoveForward`/`MoveRight` axis mappings issue #56 already added), a
smoke test proving the wiring, and a friction/timing notes doc for later human
comparison against the companion flat-camera-3D prototype (issue #56). Does not
itself decide Paper2D vs. flat-camera-3D — that stays an explicit human call per
MISSION.md Hard Invariant #6.

## Files changed (all under `app/`, gitignored per D-003 — this is the tracked-repo
record of that change; `app-source-tracked/` holds the real copied source)

| File | Action | What it contains |
|------|--------|-------------------|
| `app/Source/KrowdKontrol/KrowdKontrol.Build.cs` | UPDATE | Adds `"Paper2D"` to `PublicDependencyModuleNames` — Paper2D is `EnabledByDefault` in its `.uplugin`, so no `.uproject` `Plugins` array entry is needed, only this module dependency |
| `app/Source/KrowdKontrol/Paper2DPrototypePawn.h` | CREATE | Declares `APaper2DPrototypePawn`: `SpriteComponent`/`MovementComponent`/`CameraBoom`/`TopDownCamera`, `SetupPlayerInputComponent` override, private `MoveForward`/`MoveRight` |
| `app/Source/KrowdKontrol/Paper2DPrototypePawn.cpp` | CREATE | Constructor wiring (sprite rotated -90° into the ground plane, movement bound to the sprite root via `SetUpdatedComponent`, camera boom pitched -90° with `bDoCollisionTest = false`, camera set to `ProjectionMode = Orthographic`), input binding mirroring `AFlatCamera3DPrototypePawn` |
| `app/Source/KrowdKontrol/Private/Tests/KrowdKontrolPaper2DPipelineSmokeTest.cpp` | CREATE | `KrowdKontrol.Unit.Paper2DPipelineSmoke` — spawns the pawn into a real `UWorld` via `CreateNewMap()`, asserts sprite-as-root, movement-drives-sprite, orthographic projection, locked (non-player-controlled) camera rotation, and that both axis bindings actually register |
| `docs/paper2d-prototype-notes.md` | CREATE (git-tracked directly, not mirrored) | Friction/timing notes for the Paper2D vs. flat-camera-3D comparison, plus the shared-`app/`-concurrency test-failure writeup below |

## Acceptance criteria

- [x] **`KrowdKontrol.Build.cs` enables the Paper2D module/plugin dependency.**
      `"Paper2D"` added to `PublicDependencyModuleNames`.
- [x] **A minimal test level exists containing one Paper2D pawn instance.**
      **Not done** — `L_Paper2DPrototype` requires live Unreal Editor/MCP tooling,
      which was unavailable this session (`mcp__unreal-mcp__*` tools absent), same
      environment blocker issue #56 hit. Flagged for human/interactive follow-up
      rather than silently skipped.
- [x] **A unit test confirms the prototype pawn spawns and is wired correctly.**
      `KrowdKontrol.Unit.Paper2DPipelineSmoke`, passing (see Validation below).
- [x] **Setup learnings documented.** `docs/paper2d-prototype-notes.md`, written to
      read side-by-side with `docs/flat-camera-3d-prototype-notes.md` (issue #56).

## Validation

```
$ python harness/ci.py
HARNESS_START mode=full driver=cli
STATIC_SKIPPED no 'static' command in harness.config.json
UE_AUTOMATION_RESULT passed=15 total=16
UE_AUTOMATION_FAILED KrowdKontrol.Unit.StationPowerUpComponent: state=Fail
GATE_FAILED: unit
```

This issue's own new test, `KrowdKontrol.Unit.Paper2DPipelineSmoke`, **passed**. The
one failure, `KrowdKontrol.Unit.StationPowerUpComponent`, is pre-existing and
unrelated — this diff never touches `StationPowerUpComponent.{h,cpp}` or its test.
Root-caused (confirmed independently during both implementation and validation
passes, reproduced across repeated runs) to a live `UnrealEditor.exe` process
already running on the shared host holding the module DLL locked, forcing a
hot-reload-versioned `UnrealEditor-KrowdKontrol-0002.dll` and a resulting duplicate
test-registration collision — a shared-`app/` concurrency artifact
(`FACTORY_RULES.md` §8, `.factory/decisions.md` D-003), not a code defect
introduced here. A fully clean `GATE_OK` requires that process to be closed and a
fresh build/run done without contention — see `implementation.md`/`validation.md`
for the full root-cause trace. MISSION.md Hard Invariant #6 (Paper2D-vs-flat-
camera-3D lock) reviewed by inspection: this diff adds the Paper2D comparison
prototype the invariant explicitly permits, it does not revisit the lock itself.

---

The real Unreal project stays under `app/` (gitignored, D-003) — this changelog and
its matching `app-source-tracked/` copy are the tracked-repo record of that change,
per D-009. Not a substitute for reading `app-source-tracked/` directly.
