# Paper2D Prototype — Friction Notes

Notes from landing `APaper2DPrototypePawn` and `L_Paper2DPrototype.umap` (issue #55),
the Paper2D half of PRD 14 REQ-1's Paper2D-vs-flat-camera-3D pipeline comparison.
Recorded here for the later human decision comparing this against the companion
flat-camera-3D prototype (issue #56, merged). **This doc does not make that decision —**
it's raw input for it.

## Friction encountered

- **Paper2D ships no engine-default sprite asset.** Unlike flat-camera-3D's
  `/Engine/BasicShapes/Cube.Cube`, there is nothing built into the engine for
  `FObjectFinder` to grab for `SpriteComponent`. The pawn's constructor leaves the
  sprite unassigned — a real gap this prototype doesn't paper over, not an oversight.
- **Paper2D sprites default to a vertical (XZ-plane) orientation meant for side-view
  cameras**, which renders edge-on (effectively invisible) under a top-down camera.
  `Paper2DPrototypePawn.cpp` rotates `SpriteComponent` by `-90°` pitch to lay it flat
  into the ground (XY) plane instead — a Paper2D-specific step flat-camera-3D's
  3D-mesh pawn never needed.
- **This prototype uses a genuinely orthographic camera** (`ProjectionMode =
  Orthographic`, `OrthoWidth = 1024`), vs. flat-camera-3D's perspective camera at a
  steep but still-perspective `-80°` pitch. This is a real, structural difference
  between the two prototypes worth weighing directly in the human comparison, not
  something either prototype's notes should try to resolve on their own.
- **The placed level had drifted from the pawn's class defaults, and the level-placement
  test (added this pass, see below) caught it.** `L_Paper2DPrototype.umap`'s placed
  `APaper2DPrototypePawn` instance had a `SpriteComponent` relative rotation that did
  not match the class default (`-90°` pitch) — a stale per-instance override, most
  likely left over from PR #100's original (rejected-for-unrelated-reasons) build. The
  new `KrowdKontrol.Unit.Paper2DPipelineLevelHasConfiguredPawn` test failed honestly
  against the live `.umap` on first run (`CameraBoom` pitch, `bUsePawnControlRotation`,
  and `TopDownCamera` projection all passed; only the sprite's own rotation was wrong),
  which is exactly the class of regression a `CreateNewMap()`-only smoke test cannot
  catch. Fixed by resetting the placed instance's `SpriteComponent`/`CameraBoom`
  rotations to the class defaults and resaving the level; the test now passes against
  the corrected, persisted `.umap` (confirmed by a second, independent headless run).
- **MCP was not actually usable for this fix, despite the endpoint being reachable.**
  The plan for this issue specified verifying/fixing the level via live Unreal MCP
  (`load_level` → `find_actors` → `get_properties` → `CaptureViewport`), following
  issue #56's own successful pattern. `http://127.0.0.1:8000/mcp` did respond (HTTP 405,
  the same signal `scripts/ue_editor_launch_and_wait.sh` uses to confirm readiness) once
  the Editor GUI was launched, and WSL2 mirrored networking was confirmed active
  (`eth0` on the host's real LAN address, not a NAT range) — so this is not a repeat of
  the WSL2↔Windows networking gap `CLAUDE.md` documents as fixed. The gap was narrower:
  no `unreal-mcp`-prefixed tool ever became callable in this interactive session, even
  after the server came up. Rather than leave a genuine level defect unfixed and the
  new test failing, the fix was made directly through a temporary, local-only Editor
  automation fixture (`UnrealEditor-Cmd.exe` running a one-off `IMPLEMENT_SIMPLE_AUTOMATION_TEST`
  that loaded the map, corrected the drifted actor, and called
  `UEditorLoadingAndSavingUtils::SaveMap`), which was deleted immediately after running
  once. That fixture is not part of this PR — it never touched `app-source-tracked/`
  and no longer exists in `app/` either. This is a real, worth-flagging gap in this
  session's MCP tooling, not a claim that Unreal MCP itself is broken.

## Iteration speed

- No comparison numbers exist for build/iteration time — this pass was a verify-and-land
  effort against already-written pawn code (see the issue's own plan), not a
  from-scratch build, so timing it would not be a meaningful data point against
  flat-camera-3D's from-scratch numbers. Noting that explicitly rather than inventing a
  number.

## Open questions for the flat-camera-3D comparison

- **Legacy `BindAxis` vs. Enhanced Input — still open, not resolved this pass.**
  Flat-camera-3D's smoke test now constructs its test `UInputComponent` against
  `UInputSettings::GetDefaultInputComponentClass()` (the project's actual configured
  class). Paper2D's smoke test still constructs a bare `NewObject<UInputComponent>(Pawn)`
  — this pass added the delegate-invocation assertion (bound axes actually fire and
  accumulate world-space input) but did not switch the input-component class, so the
  question flat-camera-3D's own notes raise — whether legacy `BindAxis` is reliable
  against the project's real `UEnhancedInputComponent`, not just the legacy base class —
  remains genuinely open for Paper2D. Don't read the delegate assertion as having closed
  this; it didn't.
- Is the orthographic-vs-perspective camera difference between the two prototypes
  something the human comparison should treat as a deciding factor, or as a
  configuration choice either pipeline could adopt regardless of Paper2D-vs-3D-mesh?
- Once both prototypes' notes exist side by side (they now do), what does "friction"
  mean quantitatively for a fairer comparison than qualitative notes alone — flat-camera
  -3D's own notes ask the same question and got no answer from that pass either.
