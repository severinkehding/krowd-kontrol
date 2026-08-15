# Flat-Camera-3D Prototype Notes (Issue #56)

Friction/iteration-speed notes from building the flat-camera-3D half of PRD 14
REQ-1's Paper2D-vs-flat-camera-3D pipeline comparison. For later comparison against
`docs/paper2d-prototype-notes.md` once issue #55 lands.

## What was built, and how long it took

- `AFlatCamera3DPrototypePawn` (header + constructor): mesh root, `UFloatingPawnMovement`,
  spring-arm-mounted top-down camera, WASD/arrow input binding. Straightforward - the
  component wiring closely mirrors `APlaceholderCubeActor`'s existing
  `ConstructorHelpers::FObjectFinder` cube-mesh pattern, so no new asset-loading
  approach had to be worked out. Roughly 15-20 minutes of authoring, almost all of it
  spent getting the spring-arm/camera relative-rotation and `bDoCollisionTest=false`
  values right rather than fighting the engine.
- Input mapping (`DefaultInput.ini`): 8 lines, ~5 minutes including re-reading the
  existing `+AxisConfig=` append convention already in the file to match syntax.
- Automation test: ~10 minutes, entirely mechanical once
  `KrowdKontrolRoomEnemyBudgetControllerTest.cpp`'s `CreateNewMap()` + `SpawnActor`
  pattern was in hand as precedent - no new test infrastructure needed.

Total headless-buildable portion (Tasks 1-4): under an hour, no engine-specific
surprises, no new module/plugin dependency.

## Enhanced Input vs. legacy input finding

The project's `DefaultInput.ini` sets `DefaultPlayerInputClass=EnhancedPlayerInput`
and `DefaultInputComponentClass=EnhancedInputComponent`, which raised the question of
whether legacy `BindAxis`/`AxisMappings` would fire at all. This pass could not
directly confirm the answer in PIE (no live Editor session was available - see below),
so this is an unresolved finding: the plan's assumption, based on Epic's documented
`EnhancedInputComponent`-is-a-superset-of-`UInputComponent` behavior, is that legacy
`BindAxis` against a `+AxisMappings=` entry works unmodified under Enhanced Input's
default classes. The code was written on that assumption (no `EnhancedInput` module
dependency added, no `UInputMappingContext`/`UInputAction` assets authored). This
still needs a human or live-Editor pass to actually confirm by pressing WASD in PIE -
flagged here rather than claimed as verified.

## Level authoring (Task 5) - blocked on environment

Could not be completed in this pass. Per `.claude/skills/unreal-agent-harness/SKILL.md`,
the Unreal MCP server is wired up but not started by default; this implementation
session had no live Editor/MCP connection (`mcp__unreal-mcp__*` tools were unavailable,
and a direct `curl` to `http://127.0.0.1:8000/mcp` timed out with no response). This is
a real, expected data point for the Paper2D-vs-flat-camera-3D comparison, not a gap
specific to this pipeline: **any** in-Editor level-authoring step in this repo is
currently blocked in a fully headless factory dispatch, regardless of which 2D
pipeline is chosen. `L_FlatCamera3DPrototype` still needs to be created (new empty
level, one `AFlatCamera3DPrototypePawn` instance placed, saved) in a follow-up
human/interactive-session pass. Tasks 1-4 (the C++ pawn class and its automation test)
are independently complete and harness-green without it.

## Camera-lock / movement setup

Easier than expected. `USpringArmComponent` with `bUsePawnControlRotation = false` and
a fixed `SetRelativeRotation` gives a genuinely locked top-down angle with no extra
code - no need to fight player-look input or override a controller rotation each
tick. `UFloatingPawnMovement` needing its `UpdatedComponent` set explicitly (it does
not default to the pawn's root component) was the one real gotcha; the automation
test's `MovementComponent->UpdatedComponent == MeshComponent` assertion exists
specifically to catch a regression on this point, since it's the kind of thing that
would otherwise fail silently (no crash, no error - the pawn would just not move).
