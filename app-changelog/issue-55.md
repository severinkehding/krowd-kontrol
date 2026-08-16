# Issue #55: Prototype: minimal Paper2D top-down movement test scene

Builds the Paper2D half of PRD 14 REQ-1's Paper2D-vs-flat-camera-3D pipeline
comparison. Adds `APaper2DPrototypePawn` (`UPaperSpriteComponent` root laid flat
into the top-down ground plane, `UFloatingPawnMovement`, a spring-arm-mounted
**orthographic** top-down `UCameraComponent`, legacy `BindAxis` WASD/arrow input
reusing the `MoveForward`/`MoveRight` axis mappings issue #56 added), a
smoke test proving the wiring, and a friction/timing notes doc for later human
comparison against the companion flat-camera-3D prototype (issue #56). Does not
itself decide Paper2D vs. flat-camera-3D — that stays an explicit human call per
MISSION.md Hard Invariant #6.

**Correction (post-review):** issue #56's PR (#99) was **closed, not merged** — the
companion `AFlatCamera3DPrototypePawn`, its smoke test, `docs/flat-camera-3d-prototype-notes.md`,
and the `DefaultInput.ini` `MoveForward`/`MoveRight` mappings this pawn's input
depends on do not exist anywhere in this repo's tracked history. See
`docs/paper2d-prototype-notes.md`'s caveat for detail; this pawn's input bindings are
unverified from tracked state until #56 is re-landed or the mappings are otherwise
confirmed.

## Files changed (all under `app/`, gitignored per D-003 — this is the tracked-repo
record of that change; `app-source-tracked/` holds the real copied source)

| File | Action | What it contains |
|------|--------|-------------------|
| `app/Source/KrowdKontrol/KrowdKontrol.Build.cs` | UPDATE | Adds `"Paper2D"` to `PublicDependencyModuleNames` — Paper2D is `EnabledByDefault` in its `.uplugin`, so no `.uproject` `Plugins` array entry is needed, only this module dependency |
| `app/Source/KrowdKontrol/Paper2DPrototypePawn.h` | CREATE | Declares `APaper2DPrototypePawn`: `SpriteComponent`/`MovementComponent`/`CameraBoom`/`TopDownCamera`, `SetupPlayerInputComponent` override, private `MoveForward`/`MoveRight` |
| `app/Source/KrowdKontrol/Paper2DPrototypePawn.cpp` | CREATE | Constructor wiring (sprite rotated -90° into the ground plane, movement bound to the sprite root via `SetUpdatedComponent`, camera boom pitched -90° with `bDoCollisionTest = false`, camera set to `ProjectionMode = Orthographic`), input binding mirroring `AFlatCamera3DPrototypePawn` |
| `app/Source/KrowdKontrol/Private/Tests/KrowdKontrolPaper2DPipelineSmokeTest.cpp` | CREATE | `KrowdKontrol.Unit.Paper2DPipelineSmoke` — spawns the pawn into a real `UWorld` via `CreateNewMap()`, asserts sprite-as-root, movement-drives-sprite, sprite plane-correction rotation, orthographic projection with a sane `OrthoWidth`, locked (non-player-controlled) camera rotation, and that both axis bindings actually register |
| `docs/paper2d-prototype-notes.md` | CREATE (git-tracked directly, not mirrored) | Friction/timing notes for the Paper2D vs. flat-camera-3D comparison, plus the shared-`app/`-concurrency test-failure writeup below |

## Acceptance criteria

- [x] **`KrowdKontrol.Build.cs` enables the Paper2D module/plugin dependency.**
      `"Paper2D"` added to `PublicDependencyModuleNames`.
- [x] **A minimal test level exists containing one Paper2D pawn instance.**
      **Done, post-review** — `L_Paper2DPrototype.umap` created via live Unreal MCP
      (operator, 2026-08-16): duplicated `/Engine/Maps/Templates/Template_Default`
      to `/Game/Maps/L_Paper2DPrototype`, placed one `APaper2DPrototypePawn`
      instance, saved. Confirmed on disk. Not part of this PR's tracked diff —
      `Content/` binary assets are deliberately never git-tracked (D-003/D-009), so
      this criterion is satisfied by the asset's real existence in the shared
      project, the same way the level's own `.umap` file always would be.
- [x] **A unit test confirms the prototype pawn spawns and is wired correctly.**
      `KrowdKontrol.Unit.Paper2DPipelineSmoke`, passing (see Validation below).
- [x] **Setup learnings documented.** `docs/paper2d-prototype-notes.md`, written to
      read side-by-side with `docs/flat-camera-3d-prototype-notes.md` (issue #56) —
      note: #56's PR was closed, not merged, so that companion doc isn't actually in
      this tracked repo; see the caveat at the top of `docs/paper2d-prototype-notes.md`.

## Validation

Original run:

```
$ python harness/ci.py
HARNESS_START mode=full driver=cli
STATIC_SKIPPED no 'static' command in harness.config.json
UE_AUTOMATION_RESULT passed=15 total=16
UE_AUTOMATION_FAILED KrowdKontrol.Unit.StationPowerUpComponent: state=Fail
GATE_FAILED: unit
```

This issue's own new test, `KrowdKontrol.Unit.Paper2DPipelineSmoke`, **passed**.

**Correction (post-review) on the `StationPowerUpComponent` failure's cause.** The
diagnosis above — a concurrent-Editor DLL/hot-reload collision — was wrong. Verified
independently (operator, 2026-08-16): the failure was 100% deterministic regardless
of whether the Editor was open or closed, and traced to a real, unrelated code bug —
`AddExpectedError`'s `IsRegex` parameter defaults to `true`, and the pattern
`"OrderedLights[1] is null"` was being parsed as regex (`[1]` = a character class,
not literal brackets), so it could never match the literal logged text. Fixed by
passing `IsRegex=false` on both `AddExpectedError` calls in
`KrowdKontrolStationPowerUpComponentTest.cpp` (issue #60's test, unrelated to this
PR's own changes) — see `.factory/decisions.md` D-012. Re-run after that fix:

```
$ python harness/ci.py
HARNESS_START mode=full driver=cli
STATIC_SKIPPED no 'static' command in harness.config.json
UNIT_PASSED tests=16
APP_STARTED driver=cli
UE_AUTOMATION_RESULT passed=1 total=1
UE_AUTOMATION_OK
E2E_PASSED steps=1
GATE_OK mode=full
```

Clean `GATE_OK`, all 16 unit tests passing, no unrelated failures. MISSION.md Hard
Invariant #6 (Paper2D-vs-flat-camera-3D lock) reviewed by inspection: this diff adds
the Paper2D comparison prototype the invariant explicitly permits, it does not
revisit the lock itself.

---

The real Unreal project stays under `app/` (gitignored, D-003) — this changelog and
its matching `app-source-tracked/` copy are the tracked-repo record of that change,
per D-009. Not a substitute for reading `app-source-tracked/` directly.
