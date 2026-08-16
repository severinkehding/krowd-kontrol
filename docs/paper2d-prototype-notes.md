# Paper2D Prototype — Friction Notes

Notes from building `APaper2DPrototypePawn` and fixing `L_Paper2DPrototype.umap`
(issue #55), the Paper2D half of PRD 14 REQ-1's Paper2D-vs-flat-camera-3D pipeline
comparison. Recorded here for the later human decision comparing this against the
companion flat-camera-3D prototype (issue #56, merged as PR #102;
`docs/flat-camera-3d-prototype-notes.md`). **This doc does not make that decision —**
it's raw input for it.

## Friction encountered

- **The headline finding: relative rotation composes through a rotated parent, and
  `GetRelativeRotation()`-only assertions can't see that.** Two prior attempts (PR
  #100, PR #106) built `APaper2DPrototypePawn` with `SpriteComponent` as the pawn's
  root (rotated `-90°` pitch to lay the sprite flat into the ground plane) and
  `CameraBoom` as its child, also set to `-90°` relative pitch, intending an
  independent straight-down camera. Those two rotations don't add — composing
  child-relative-then-parent through UE's actual `FRotator::Quaternion()` formula
  (`WorldQuat_boom = WorldQuat_sprite * RelativeQuat_boom`) turns two `-90°` pitches
  into a full `180°` rotation about world Y: the camera ends up facing roughly
  horizontally backward-and-upside-down, not straight down. Every existing automation
  assertion checked `CameraBoom->GetRelativeRotation().Pitch`, which was genuinely
  `-90°` and passed — the relative number was always correct, it just wasn't the
  number a player standing behind the camera actually sees. The fix: give the pawn an
  unrotated `USceneComponent` root (`PawnRoot`, matching the `RoomActor`/
  `DoorConnectorActor` convention already used elsewhere in this module) and attach
  `SpriteComponent` and `CameraBoom` to it as siblings, so each one's relative
  rotation is also its true world rotation again. Every rotation assertion touching
  `CameraBoom` or `TopDownCamera` in the smoke test now includes at least one
  `GetComponentRotation()` (world-space) check specifically so this bug class can't
  silently recur.
- **A placed level instance can carry stale rotation/attachment data that
  outlives a C++ class fix, and only opening the real level (not just the class) can
  catch it.** `L_Paper2DPrototype.umap` already existed from a prior attempt, placed
  against the old (buggy) class shape. After the C++ fix compiled clean, the new
  `KrowdKontrol.Unit.Paper2DPipelineLevelHasConfiguredPawn` test — which loads the
  real `.umap` rather than spawning a fresh instance — failed where the
  `CreateNewMap()`-based tests passed. Live inspection via Unreal MCP
  (`editor_toolset.toolsets.actor.ActorTools`/`.object.ObjectTools`) turned up two
  independent leftovers on the placed actor, not one:
  1. The actor's own placement transform had a `-90°` pitch baked in (likely an
     earlier manual attempt to compensate for the visually-broken camera by rotating
     the whole pawn), which composed on top of every child component's rotation.
  2. `CameraBoom`'s `AttachParent` was still serialized as `SpriteComponent` — the
     *old* attachment target — even though the freshly-compiled class's constructor
     now attaches it to `PawnRoot`. Per-instance component attachment on a placed
     native actor is sticky across a C++ class change; it does not silently follow the
     new CDO shape on its own.
  Fixed both live (`set_actor_transform` to zero the actor's own rotation,
  `set_parent_component` to reparent `CameraBoom` from `SpriteComponent` to
  `PawnRoot`, then `set_properties` to restore `CameraBoom`'s relative pitch to
  `-90°` since reparenting preserves world transform rather than relative transform),
  verified with `get_properties` after each step, and saved via
  `AssetTools.save_assets`. `KrowdKontrol.Unit.Paper2DPipelineLevelHasConfiguredPawn`
  passes against the resaved level. **Lesson for the flat-camera-3D comparison and any
  future placed-actor fix: a C++ class change to a component's default attachment or
  rotation is not guaranteed to propagate to an already-placed instance of that
  class — verify the live level, don't assume the CDO fix was enough.**
- **Paper2D ships a default sprite asset, just not where the obvious analogy
  suggests.** Unlike the flat-camera-3D sibling's `/Engine/BasicShapes/Cube.Cube`,
  there's no equally-discoverable Paper2D equivalent in the editor UI — but the engine
  does ship one, at `/Paper2D/DummySprite.DummySprite`
  (`Engine/Plugins/2D/Paper2D/Content/DummySprite.uasset`), found by inspecting the
  installed engine's plugin content rather than official docs. `SpriteComponent` now
  assigns it via `ConstructorHelpers::FObjectFinder`, closing the "no default sprite
  to assign" gap a prior attempt left open.
- **This prototype uses a genuinely orthographic camera** (`TopDownCamera->
  ProjectionMode = ECameraProjectionMode::Orthographic`, `OrthoWidth = 1024.0f`),
  unlike the flat-camera-3D sibling's perspective spring-arm camera (`-80°` pitch, no
  explicit projection mode override). That's a real, deliberate structural difference
  between the two prototypes worth flagging for the human PRD comparison, not an
  oversight to reconcile — see Open questions below.

## Iteration speed

- Once WSL2 mirrored networking was confirmed live (`CLAUDE.md`'s Environment
  section) and `scripts/ue_editor_launch_and_wait.sh` brought up a real Editor session
  with a responding MCP endpoint, live level inspection and repair
  (`load_level` → `find_actors` → `get_properties`/`get_parent_component` →
  `set_actor_transform`/`set_parent_component`/`set_properties` → `save_assets`) took
  on the order of a dozen tool calls and well under a minute of wall-clock time, no
  manual Editor-side steps. Diagnosing the *specific* leftover (actor-level rotation
  plus a stale component attachment, not just one or the other) took longer than the
  fix itself — direct property/attachment inspection was what made that diagnosable at
  all; the automation test's pass/fail alone only said "wrong," not "wrong how."
- No comparable number exists yet for the flat-camera-3D side's own level-authoring
  pass since that issue (#56) didn't hit this specific failure mode — noting that
  asymmetry explicitly rather than inventing a comparable number.

## Open questions for the flat-camera-3D comparison

- Is a genuinely orthographic top-down camera (this prototype) meaningfully different
  in "feel" from a steeply-pitched perspective spring-arm camera (the flat-camera-3D
  sibling), for the purposes of this PRD's pipeline decision — or is the difference
  only visible in edge-of-frame parallax that a quick comparison pass might miss?
- Does legacy `BindAxis` behave identically for both prototypes against the project's
  actual `UEnhancedInputComponent` (`DefaultInput.ini`'s configured default)? This
  pass's smoke test invokes the bound delegates directly rather than through a live
  PIE input event, same limitation the flat-camera-3D notes already flagged for its
  own prototype.
- Now that both prototypes exist, "friction" can be compared on a shared basis: this
  pass needed a live-Editor level repair the flat-camera-3D pass didn't; the
  flat-camera-3D pass's own friction notes were dominated by MCP reachability instead.
  Neither prototype's gameplay C++ itself needed more than one real engineering
  iteration once the composed-rotation bug was understood.
