# Flat-Camera-3D Prototype — Friction Notes

Notes from building `AFlatCamera3DPrototypePawn` and `L_FlatCamera3DPrototype.umap`
(issue #56), the flat-camera-3D half of PRD 14 REQ-1's Paper2D-vs-flat-camera-3D
pipeline comparison. Recorded here for the later human decision comparing this against
the companion Paper2D prototype (issue #55). **This doc does not make that decision —**
it's raw input for it.

## Friction encountered

- **`UFloatingPawnMovement`'s explicit `SetUpdatedComponent()` call is defensive, not
  strictly required.** `MeshComponent` is already `RootComponent`, so
  `OnRegister()`'s auto-detection reaches the same `UpdatedComponent` via the same
  setter either way — verified against UE 5.8 engine source
  (`Engine/Source/Runtime/Engine/Private/Components/MovementComponent.cpp`):
  `OnRegister()` unconditionally calls `SetUpdatedComponent()` again in a game world,
  even correcting a prior raw field write. Kept the explicit call anyway so the wiring
  is visible at the call site rather than implicit.
- **Legacy `BindAxis` vs. Enhanced Input.** `DefaultInput.ini` already sets
  `DefaultPlayerInputClass`/`DefaultInputComponentClass` to the Enhanced Input classes
  project-wide, but this pawn's `SetupPlayerInputComponent` uses the legacy
  `PlayerInputComponent->BindAxis()` API against classic `AxisMappings` entries in
  `DefaultInput.ini`. It appears to work in a quick PIE check, and Enhanced Input's
  classes are documented as backward-compatible with legacy axis binding — but this
  wasn't verified against the project's actual `UEnhancedInputComponent` until the
  smoke test was extended to construct against `UInputSettings::GetDefaultInputComponentClass()`
  instead of a hardcoded `UInputComponent`; see "Open questions" below for what that
  still doesn't cover.
- **MCP reachability was the actual blocker across all three attempts at this issue,
  not the gameplay code.** The pawn, test, and input mappings were already correct
  after the first attempt. The first two attempts couldn't verify or author the level
  itself because `127.0.0.1:8000` (Unreal's MCP HTTP server) was unreachable from WSL2
  — connection refused/timeout, not an application-level error. This attempt landed
  after `CLAUDE.md`'s WSL2 mirrored-networking fix (`networkingMode=mirrored` in
  `.wslconfig`, requiring a genuine `wsl --shutdown` from a Windows-side shell to take
  effect) made `127.0.0.1` resolve identically on both sides. Once that was in place,
  `find_actors`, `get_properties`, and `CaptureViewport` all worked against the live
  Editor session without further changes.

## Iteration speed

- Confirming the level via MCP (`load_level` → `find_actors` → `get_properties` on the
  `CameraBoom` → `CaptureViewport`) took a handful of tool calls and no Editor-side
  manual steps once the connection was live — on the order of seconds per call, not
  minutes.
- No comparison numbers exist yet for the Paper2D side (issue #55 not yet built at the
  time of writing) — noting that explicitly rather than inventing a number.

## Open questions for the Paper2D comparison

- Is legacy `BindAxis` against `UEnhancedInputComponent` actually reliable in a live PIE
  session, or did this prototype get lucky in a quick manual check? The smoke test now
  constructs the same input component class the project actually configures
  (`UInputSettings::GetDefaultInputComponentClass()`) and confirms the axis bindings
  register against it, which is stronger evidence than the original bare-`UInputComponent`
  check — but it still doesn't invoke the bound delegates through a live PIE input event,
  only through the automation test's direct delegate call.
- Does the Paper2D prototype hit the same legacy-`BindAxis`-vs-Enhanced-Input question,
  or does 2D sprite/tile tooling push toward Enhanced Input more directly?
- Is a `USpringArmComponent` pitch lock (`-80°`, no collision test, no pawn-control
  rotation) a reasonable proxy for "camera feel" comparison against Paper2D's typically
  orthographic camera setup, or does the perspective-vs-orthographic difference make
  camera-feel comparison between the two prototypes not apples-to-apples?
- Once both prototypes exist side by side, what does "friction" mean quantitatively —
  build/iteration time, lines of pawn code, or something else? This doc's own
  iteration-speed section above didn't have a baseline to compare against; the Paper2D
  side's notes should use the same categories so the two are comparable.
