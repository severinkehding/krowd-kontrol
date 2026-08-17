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
| Marker text reads correctly (not mirrored) under `FlatCamera3DPrototypePawn` | **Unverified — derivation only.** Code review independently re-derived this rig's camera position from `CameraBoom`'s -80° pitch via `USpringArmComponent`'s socket-placement formula and got a result inconsistent with this PR's own "+X side" premise (camera socket lands at negative X, not positive) — see PR #137 review discussion. Not resolved because no rendering/screenshot capability exists in this environment (no live Unreal/MCP connection this session). Needs an actual in-editor/PIE screenshot under this rig before this row can be marked Satisfied. |
| Marker text reads correctly (not mirrored) under `Paper2DPrototypePawn` | **Unverified — derivation only.** Same caveat as above; additionally this rig's boom is exactly Pitch=-90° (gimbal-lock boundary), where "which side" is less well-defined than for the angled -80° rig. Needs a PIE screenshot to confirm. |
| No regression to existing marker behavior (attachment, colour, codename text) | Satisfied — only the rotation call was added; no other property touched |
| Regression coverage added | Partially satisfied — new assertion loop in `KrowdKontrolEnemyTypeIndicatorComponentTest.cpp` checks `MarkerTextComponent`'s relative rotation equals `FRotator(0, 180, 0)` for all 4 indicators, plus a (c3) composition check that the fixed rotation composes correctly with a rotated owner. Neither renders anything, so neither can independently confirm the rotation *value* itself is correct (only that it doesn't silently regress) — see the two rows above. |

## Known Open Question

PR #137 review flagged that the fix's own "-80°/-90° boom → camera views from the
component's +X side" premise may be backwards for `FlatCamera3DPrototypePawn`: an
independent re-derivation of the spring-arm socket placement puts that rig's camera at
negative X, which — if `UTextRenderComponent`'s default legible side really is local
-X, per the premise this fix itself relies on — is already the legible side, meaning
the added 180° Yaw flip could do nothing or actively re-mirror text under that rig.
Neither the original derivation nor the review's counter-derivation could be settled
with a live screenshot (no Unreal/MCP rendering connection available in this
environment), so this remains open. **Before treating this fix as verified, get an
actual in-editor/PIE screenshot of a marker under both `FlatCamera3DPrototypePawn` and
`Paper2DPrototypePawn` and visually confirm the text reads correctly (not mirrored)
under each.** See PR #137 review discussion for both derivations in full.

## Validation Evidence

`harness/ci.py --full` → `GATE_OK` (mode=full): 40 unit tests passed (including the
new regression assertions), UE module build succeeded, 1/1 Automation Framework test
passed, 1/1 E2E step passed. No protected files touched; reviewed against all 8 Hard
Invariants in `MISSION.md` with no findings. Full details in
`artifacts/runs/c2738e69e4f0f2b72086f91f61bdb75e/validation.md`.
