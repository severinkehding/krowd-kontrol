# Issue #134: Enemy type indicator text renders mirror-flipped under the top-down camera

## Summary

`UEnemyTypeIndicatorComponent`'s floating `MarkerTextComponent` was left at
`UTextRenderComponent`'s engine-default orientation, which is legible only from the
component's local -X side. Both of this project's fixed top-down camera rigs
(`FlatCamera3DPrototypePawn`'s -80° boom pitch, `Paper2DPrototypePawn`'s -90° boom
pitch) view the actor from the opposite (+X) side, so marker text rendered
mirror-flipped (e.g. `B0-0MR` as `ЯM0-08`). The fix adds a single, pitch-independent
180° Yaw `SetRelativeRotation` call in `InitializeMarkerVisual()` so the text faces
both camera rigs correctly, plus a regression test asserting that rotation on all 4
core enemy-type indicators.

## Acceptance Criteria

| Criterion | Status |
|---|---|
| Marker text reads correctly (not mirrored) under `FlatCamera3DPrototypePawn` | Satisfied — 180° Yaw flip is pitch-independent, verified against the rig's -80° boom pitch derivation |
| Marker text reads correctly (not mirrored) under `Paper2DPrototypePawn` | Satisfied — verified against the rig's -90° boom pitch derivation |
| No regression to existing marker behavior (attachment, colour, codename text) | Satisfied — only the rotation call was added; no other property touched |
| Regression coverage added | Satisfied — new assertion loop in `KrowdKontrolEnemyTypeIndicatorComponentTest.cpp` checks `MarkerTextComponent`'s relative rotation equals `FRotator(0, 180, 0)` for all 4 indicators |

## Validation Evidence

`harness/ci.py --full` → `GATE_OK` (mode=full): 40 unit tests passed (including the
new regression assertions), UE module build succeeded, 1/1 Automation Framework test
passed, 1/1 E2E step passed. No protected files touched; reviewed against all 8 Hard
Invariants in `MISSION.md` with no findings. Full details in
`artifacts/runs/c2738e69e4f0f2b72086f91f61bdb75e/validation.md`.
