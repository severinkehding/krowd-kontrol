# Flat-Camera-3D Prototype — Friction Notes

Notes from building `AFlatCamera3DPrototypePawn` and `L_FlatCamera3DPrototype.umap`
(issue #56), the flat-camera-3D half of PRD 14 REQ-1's Paper2D-vs-flat-camera-3D
pipeline comparison. Recorded here for the later human decision comparing this against
the companion Paper2D prototype (issue #55). **This doc does not make that decision —**
it's raw input for it.

## Friction encountered

- **`UFloatingPawnMovement` needs `SetUpdatedComponent()`, not a raw field assignment.**
  Setting `MovementComponent->UpdatedComponent = MeshComponent` directly compiles and
  looks correct, but skips side effects `OnRegister()`'s auto-detection would otherwise
  have handled: `UpdatedPrimitive` doesn't get populated, and the physics-volume-changed
  delegate doesn't get bound. Going through the engine's `SetUpdatedComponent()` setter
  avoids both. Easy to get wrong silently — the pawn still moves in a quick PIE check,
  the gap only shows up in physics-volume-dependent behavior.
- **Legacy `BindAxis` vs. Enhanced Input.** `DefaultInput.ini` already sets
  `DefaultPlayerInputClass`/`DefaultInputComponentClass` to the Enhanced Input classes
  project-wide, but this pawn's `SetupPlayerInputComponent` uses the legacy
  `PlayerInputComponent->BindAxis()` API against classic `AxisMappings` entries in
  `DefaultInput.ini`, and it works — Enhanced Input's classes are backward-compatible
  with legacy axis binding. Left as an open question below rather than resolved here,
  since resolving it isn't in scope for a pipeline-comparison prototype.
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
