# Issue #188: Expose camera framing (arm length, pitch, FOV) as tunable properties on AFlatCamera3DPrototypePawn and retune defaults

PRD `Level Playability & Presentation` REQ-4 (P1): the top-down camera's `SpringArm`
was fixed at `TargetArmLength = 800`, pitch −80°, as constructor constants with no
tuning knobs, and FOV was left at the engine's implicit 90° default. This was called
out as a direct cause of "everything feels far away / hard to read" in the PRD's
playtest findings.

`AFlatCamera3DPrototypePawn` gains three clamped `EditAnywhere` `UPROPERTY` floats —
`CameraArmLength` ([300,600], default `450.0f`), `CameraBoomPitch` ([-75,-45], default
`-60.0f`), `CameraFieldOfView` ([60,90], default `75.0f`) — under a new
`FlatCamera3DPrototype|Camera` category. The three previously-inline constructor
statements were extracted into a new `ApplyCameraFraming()` member function, called
once from the constructor and again from a new `WITH_EDITOR`-guarded
`PostEditChangeProperty` override, so a placed level instance's Details-panel edits
reach `CameraBoom`/`TopDownCamera` live, not just the CDO at construction time.

**Default value reasoning:**
- `CameraArmLength: 800 → 450` (~44% closer) — rooms in the current level layout sit
  ~30m apart; 800cm of arm length at a steep pitch pushed gameplay-scale detail
  (enemies, ability telegraphs) too small on screen.
- `CameraBoomPitch: -80° → -60°` — meaningfully less extreme top-down, giving more
  visible vertical surface area on enemies/telegraphs (-80° is nearly straight down,
  flattening everything into silhouettes), while staying within the pre-existing
  `Pitch <= -45.0f` "genuinely top-down, not side-on" assertion.
- `CameraFieldOfView: 90° (implicit) → 75°` — a narrower FOV compensates for the
  shorter arm length, avoiding edge-of-frame stretch/distortion that a closer camera
  paired with a wide FOV tends to introduce.

A new automation test, `KrowdKontrol.Unit.FlatCamera3DPipelineCameraFraming`, asserts
the defaults land within their documented ranges, are strictly closer/less-extreme
than the pre-#188 hardcoded values, are already applied to `CameraBoom`/`TopDownCamera`
at spawn, and that mutating a property + calling `ApplyCameraFraming()` genuinely
updates the corresponding component field (proving the wiring is live, not decorative).

## Files changed (all under `app/`, gitignored per D-003 — mirrored here per D-009)

| File | Action | What it contains |
|------|--------|-------------------|
| `app/Source/KrowdKontrol/FlatCamera3DPrototypePawn.h` | UPDATE | 3 new clamped `EditAnywhere` `UPROPERTY` floats; declares `ApplyCameraFraming()` and `WITH_EDITOR`-guarded `PostEditChangeProperty` override |
| `app/Source/KrowdKontrol/FlatCamera3DPrototypePawn.cpp` | UPDATE | Replaces 3 hardcoded constructor camera lines with `ApplyCameraFraming()`; implements it plus `PostEditChangeProperty` |
| `app/Source/KrowdKontrol/Private/Tests/KrowdKontrolFlatCamera3DPipelineSmokeTest.cpp` | UPDATE | New `FKrowdKontrolFlatCamera3DCameraFramingTest` automation test |
| `app-source-tracked/Source/KrowdKontrol/FlatCamera3DPrototypePawn.h` | UPDATE | Plain-text mirror |
| `app-source-tracked/Source/KrowdKontrol/FlatCamera3DPrototypePawn.cpp` | UPDATE | Mirror |
| `app-source-tracked/Source/KrowdKontrol/Private/Tests/KrowdKontrolFlatCamera3DPipelineSmokeTest.cpp` | UPDATE | Mirror |

## Deviation from the plan

The plan's `TestEqual` calls compared `CameraBoom->GetRelativeRotation().Pitch` (a
`double` in UE 5.8's `FRotator`) directly against the `float` pitch properties. MSVC
rejected this as an ambiguous overload between `TestEqual`'s `(double,double)` and
`(float,float)` variants. Fixed by wrapping the `.Pitch` read in
`static_cast<float>(...)` at both call sites. No other deviation from the plan.

## Acceptance criteria

- [x] `TargetArmLength`, boom pitch, and FOV are `EditAnywhere` `UPROPERTY`s, not
      constructor constants.
- [x] Defaults retuned closer/less-extreme than 800cm/−80° for readability, reasoning
      stated above.
- [x] New automation test asserts defaults land within the documented ranges and that
      the properties genuinely drive `CameraBoom`/`TopDownCamera`.
- [x] No regression in the 4 pre-existing `KrowdKontrol.Unit.FlatCamera3D*` tests.

## Validation

```
$ python harness/ci.py
GATE_OK mode=full (UNIT_PASSED tests=72, E2E_PASSED steps=1)
```

`KrowdKontrol.Unit.FlatCamera3D*` suite (5/5 pass): the new
`FlatCamera3DPipelineCameraFraming` test plus the 4 pre-existing tests, including the
`Pitch <= -45.0f` assertions in `PipelineSmoke`/`PipelineLevelHasConfiguredPawn`.

One transient `UE_AUTOMATION_FAILED KrowdKontrol.Unit.EnemyBase` on an earlier full-mode
run was investigated and confirmed to be a pre-existing flake unrelated to this diff
(EnemyBase untouched by this change; passed in isolation and on every subsequent full
suite/gate re-run) — not a regression introduced by this PR.

`app/` vs `app-source-tracked/` parity confirmed (zero diff) — no concurrent-task
leakage from the shared `app/` symlink. MISSION.md Hard Invariants reviewed: only
invariant #6 (flat-camera-3D engine/dimensionality lock) is adjacent, and this change
stays within that family (still a spring-arm-driven angled top-down camera) — no
regression.

---

Source lives under `app/` (gitignored, D-003) — this file is the tracked-repo record
of that change, not a substitute for reading the actual code.
