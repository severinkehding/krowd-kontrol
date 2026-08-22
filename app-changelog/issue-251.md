# Issue #251: Retune flat-camera default zoom-out and widen its arm-length clamp

PRD `Mission Briefing & Live Quest Tracker` REQ-4: `AFlatCamera3DPrototypePawn`'s camera
framing defaults, added in issue #188, turned out too close for crowd management once real
levels were played. Live 2026-08-22 playtests manually pushed the arm length to ~1600 and
found that slightly too far; the existing `[300, 600]` clamp didn't even allow that value to
be set as a new default without widening it first.

**Number changes (all in `FlatCamera3DPrototypePawn.h`):**
- `CameraArmLength: 450 → 1500` (clamp `[300, 600] → [600, 2000]`) — REQ-4's own example
  numbers (arm length "1500 (≈2.5× the old max)", clamp "e.g. 600–2000"), keeping 1500
  comfortably inside the new range (900 above min, 500 below max).
- `CameraFieldOfView: 75 → 90` (clamp unchanged, `[60, 90]`) — wider FOV pairs with the
  longer arm length; 90 sits exactly on the existing `ClampMax`, which REQ-4 already
  accounts for, so the clamp itself doesn't move.
- `CameraBoomPitch`: unchanged (`-60.0f`, clamp `[-75, -45]`) — out of scope; REQ-4 only
  calls out arm length and FOV.

No `.cpp` changes: `ApplyCameraFraming()` already reads these properties generically, so
retuning the `UPROPERTY` defaults/clamp meta is sufficient.

## Files changed (all under `app/`, gitignored per D-003 — mirrored here per D-009)

| File | Action | What it contains |
|------|--------|-------------------|
| `app/Source/KrowdKontrol/FlatCamera3DPrototypePawn.h` | UPDATE | `CameraArmLength` default/clamp widened, `CameraFieldOfView` default raised |
| `app/Source/KrowdKontrol/Private/Tests/KrowdKontrolFlatCamera3DPipelineSmokeTest.cpp` | UPDATE | `FKrowdKontrolFlatCamera3DCameraFramingTest`'s asserted bounds/fixture values updated to match; dropped the now-inverted "closer than 800cm" `CameraArmLength` assertion |
| `app-source-tracked/Source/KrowdKontrol/FlatCamera3DPrototypePawn.h` | UPDATE | Plain-text mirror |
| `app-source-tracked/Source/KrowdKontrol/Private/Tests/KrowdKontrolFlatCamera3DPipelineSmokeTest.cpp` | UPDATE | Mirror |

## Acceptance criteria

- [x] `AFlatCamera3DPrototypePawn::CameraArmLength` default is `1500.0f`
- [x] `AFlatCamera3DPrototypePawn::CameraFieldOfView` default is `90.0f`
- [x] `CameraArmLength`'s `ClampMin`/`ClampMax` meta widened to `[600.0, 2000.0]` so
      `1500.0f` is legal
- [x] `KrowdKontrol.Unit.FlatCamera3DPipelineCameraFraming`'s documented/asserted bounds
      match the new clamp range and defaults
- [x] No new camera mechanics introduced — `ApplyCameraFraming()`, `CameraBoomPitch`, and
      every other property/component on the pawn are unchanged
- [x] `app/` and `app-source-tracked/` are byte-identical for both edited files (zero diff
      confirmed)
- [ ] `python harness/ci.py` (full mode) reports `GATE_OK` with the `FlatCamera3D*` suite
      passing — see Validation below

## Validation

```
$ python harness/ci.py --quick
```

(inline sanity check only — full-mode run against Unreal's Automation Framework is left to
the `dark-factory-validate` node; see that node's report for the `FlatCamera3D*` suite
result.)

## Risks (not mitigated by this change, flagged for follow-up)

- `L_FlatCamera3DPrototype.umap`'s placed pawn instance may carry a per-instance
  Details-panel override on `CameraArmLength` from before this change (legal under the old
  `[300,600]` clamp); the class-default change alone doesn't retune an already-overridden
  placed instance. No existing automation test asserts arm length on the placed instance
  (only pitch), so this isn't caught by the harness. Editing the `.umap` requires a live
  Unreal Editor/MCP session, out of reach from this factory worktree — out of scope for
  this C++-only issue.

---

Source lives under `app/` (gitignored, D-003) — this file is the tracked-repo record
of that change, not a substitute for reading the actual code.
