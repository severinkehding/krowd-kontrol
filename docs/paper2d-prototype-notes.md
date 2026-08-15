# Paper2D Prototype Notes (Issue #55)

> 🚧 **PARTIAL.** `docs/flat-camera-3d-prototype-notes.md`, referenced throughout this
> doc, does not yet exist on `main` - issue #56's PR (#99) was closed, not merged. Same
> for the `MoveForward`/`MoveRight` `DefaultInput.ini` axis mappings this prototype's
> input binding assumes are already present. Treat every "same as flat-camera-3D"
> comparison below as provisional until #56 is redone and actually merged.

Friction/iteration-speed notes from building the Paper2D half of PRD 14 REQ-1's
Paper2D-vs-flat-camera-3D pipeline comparison. For side-by-side reading against
`docs/flat-camera-3d-prototype-notes.md` (issue #56) — see caveat above.

## What was built, and how long it took

- `APaper2DPrototypePawn` (header + constructor): sprite root, `UFloatingPawnMovement`,
  spring-arm-mounted orthographic top-down camera, WASD/arrow input binding. Component
  wiring closely mirrors `AFlatCamera3DPrototypePawn`'s existing pattern, so no new
  structural approach had to be worked out. Roughly 15-20 minutes of authoring, in line
  with the flat-camera-3D pawn's own estimate - the extra work here (orthographic
  projection setup, sprite-plane rotation) roughly offset not needing a
  `ConstructorHelpers::FObjectFinder` call at all (see asset-pipeline gap below).
- Input mapping (`DefaultInput.ini`): no change needed - the `MoveForward`/`MoveRight`
  `+AxisMappings=` entries issue #56 added are reused verbatim. **Unverified from
  tracked state** - #56's PR was closed, not merged, so these mappings aren't
  confirmable from this repo alone; see caveat at the top of this doc.
- Automation test: ~10 minutes, entirely mechanical, mirroring
  `KrowdKontrolFlatCamera3DPipelineSmokeTest.cpp`'s `CreateNewMap()` + `SpawnActor`
  pattern almost line-for-line, plus one new assertion for orthographic projection.

Total headless-buildable portion (Tasks 1-4): under an hour, comparable to the
flat-camera-3D prototype's timing - no engine-specific surprises specific to Paper2D
itself, beyond the two concrete gaps below.

## Asset-pipeline gap: no engine-default Paper2D sprite

This is the sharpest concrete difference from flat-camera-3D found during this pass.
`AFlatCamera3DPrototypePawn` gets a renderable mesh for free via
`ConstructorHelpers::FObjectFinder<UStaticMesh>(TEXT("/Engine/BasicShapes/Cube.Cube"))`
- one line, no content authoring. Paper2D ships **no equivalent built-in sprite
asset**: there is no content-only `UPaperSprite` in the base engine to `FObjectFinder`
against. `APaper2DPrototypePawn`'s `SpriteComponent` is therefore left with no
assigned sprite in C++ - getting real pixels on screen requires either a live-Editor
content-import step (import a texture, then generate a `UPaperSprite` asset from it)
or hand-authoring one, neither of which is a pure-code, headlessly-buildable step.
This is a genuine, concrete data point for the pipeline comparison: flat-camera-3D's
placeholder visuals are effectively free; Paper2D's are not.

## Sprite-plane-orientation gotcha

`UPaperSpriteComponent` defaults to a vertical XZ-plane orientation, meant for
side-view (platformer-style) cameras. Under a straight-down orthographic camera this
renders edge-on - invisible - unless corrected. `APaper2DPrototypePawn`'s constructor
applies `SpriteComponent->SetRelativeRotation(FRotator(-90.0f, 0.0f, 0.0f))` to lay the
sprite flat in the ground (XY) plane. This is a Paper2D-specific friction point
flat-camera-3D's static cube mesh never had - a cube looks correct from any angle
without extra rotation, so this kind of orientation bug has no flat-camera-3D
equivalent to compare against. Note this rotation is independent of `CameraBoom`'s own
`-90.0f` pitch (used to point the camera straight down) - the two values coincide
numerically but address unrelated problems; changing one doesn't imply changing the
other.

## Camera-lock / orthographic setup

`USpringArmComponent` with `bUsePawnControlRotation = false` and a fixed
`SetRelativeRotation(FRotator(-90.0f, 0.0f, 0.0f))` gives a genuinely locked
straight-down angle, same as flat-camera-3D's approach. The one addition this issue's
acceptance criteria required beyond that prototype: `TopDownCamera->ProjectionMode =
ECameraProjectionMode::Orthographic` plus an explicit `OrthoWidth`, to get a genuinely
orthographic (not just steeply-pitched-perspective) view. Trivial once the `UCameraComponent`
property was located - no fighting the engine here either.

Same `UFloatingPawnMovement` gotcha as flat-camera-3D: `UpdatedComponent` does not
default to the pawn's root and must be set explicitly
(`MovementComponent->SetUpdatedComponent(SpriteComponent)`), or movement silently does
nothing. The automation test's `MovementComponent->UpdatedComponent == SpriteComponent`
assertion exists specifically to catch a regression here, mirroring the flat-camera-3D
test's own equivalent assertion.

## Enhanced Input vs. legacy input finding

Same open question as flat-camera-3D, not re-litigated here: `DefaultInput.ini` sets
`EnhancedPlayerInput`/`EnhancedInputComponent` as the defaults, and this pass could not
confirm in PIE whether legacy `BindAxis` against the existing `+AxisMappings=` entries
actually fires (no live Editor session - see below). The code inherits the same
documented assumption issue #56's notes already recorded (Epic's
`EnhancedInputComponent`-is-a-superset-of-`UInputComponent` behavior). The automation
test verifies binding *registration*, not actual PIE movement - same limitation as the
flat-camera-3D companion test.

## Level authoring (Task 6) - blocked on environment

Could not be completed in this pass, for the same reason issue #56's implementation
hit: no `mcp__unreal-mcp__*` tools were available in this session, so there was no live
Unreal Editor/MCP connection to create a new level, place an `APaper2DPrototypePawn`
instance, and save it as `L_Paper2DPrototype`. This is not a gap specific to Paper2D -
it's the same headless-dispatch limitation flat-camera-3D's notes already documented,
confirmed to recur. `L_Paper2DPrototype` still needs to be created in a follow-up
human/interactive-session pass. Tasks 1-4 (the C++ pawn class and its automation test)
are independently complete and harness-green without it.

## Build/validation note (applies to both prototypes, not Paper2D-specific)

Confirmed the same stale-DLL gotcha issue #56's implementation already flagged:
`harness/run_ue_automation.sh` launches `UnrealEditor-Cmd.exe` against whatever is
already compiled in `app/Binaries/Win64/` - it does not rebuild the module itself. A
real `UnrealBuildTool` compile (via the bundled .NET 10 runtime at
`Engine/Binaries/ThirdParty/DotNet/10.0/win-x64/dotnet.exe`, since the system-installed
`dotnet.exe` only had .NET 8 runtimes) was required before this issue's new pawn/test
code was actually exercised by the automation run. Separately, this pass observed a
live `UnrealEditor.exe` process already running on the host during validation, which
forced the build to produce a hot-reload-versioned `UnrealEditor-KrowdKontrol-0002.dll`
and caused one pre-existing, unrelated test (`KrowdKontrol.Unit.StationPowerUpComponent`,
issue #60/#97) to fail with a "test already registered" warning - a shared-`app/`
concurrency artifact (see `FACTORY_RULES.md` §8), not a regression introduced by this
issue. `KrowdKontrol.Unit.Paper2DPipelineSmoke` itself passed cleanly.
