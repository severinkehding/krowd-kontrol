# Issue #257: Sleep ability — thrown-bomb AoE circle at cursor, long range, breaks on other-ability hit

Sleep previously reused the generic auto-nearest-target single-enemy cast shared by every
other ability. This change gives it its own locked shape per the PRD: a cursor-aimed
thrown-bomb landing circle (radius = 4x body diameter) clamped to the Long range tier,
affecting every enemy inside on landing, with a Sleep-only rule that a different ability's
hit on a sleeping enemy wakes it immediately instead of letting its Controlled duration
run out.

## Acceptance criteria

- [x] Casting Sleep throws to the cursor position, clamped to the Long range tier —
      `UAbilityCastComponent::TryCastThrownAbilityAtLocation` + `ComputeClampedThrowLocation`
      (`AbilityCastComponent.cpp`), built as a generic/reusable helper for a future Stun
      implementation, not Sleep-specific.
- [x] AoE circle (radius = 4x placeholder body diameter) centered on landing point affects
      every enemy inside (multi-target) — existing `CircleAtCursor` shape application,
      now driven by the thrown landing point instead of cursor-follow.
- [x] Affected enemies enter Controlled at full immobilization per `AbilityData` duration,
      with early-wake-on-other-ability-hit flavour — new `AbilityData::bWakesEarlyOnOtherAbilityHit`
      (true only for Sleep) + `AEnemyBase::ReceiveControl`'s new early-wake branch, which
      reuses the existing Controlled→Alert + `OnEnemyControlledExpired` edge.
- [x] `IsControlled()`, banking eligibility, Crowd Mastery sampling, and herd/bank chain
      unaffected; early-woken enemy reverts to its prior AI state via the same expiry path
      normal timeout uses (no new/broken half-Controlled state introduced).
- [x] Automation tests: in-shape/out-of-shape landing circle, throw-distance clamp at Long
      tier, cooldown-always-consumed, wake-on-second-ability-hit, same-ability re-cast is a
      no-op, non-wake-flagged ability unaffected by the new branch — added across
      `KrowdKontrolAbilityCastComponentTest.cpp`, `KrowdKontrolAbilityDataTest.cpp`,
      `KrowdKontrolEnemyBaseTest.cpp`, `KrowdKontrolAbilityPressHoldComponentTest.cpp`.

## Scope note

Stun (issue #256) is left fully unwired to the new throw mechanism — its key press still
uses the pre-existing auto-nearest-target path. `TryCastThrownAbilityAtLocation` is written
generically so Stun can adopt it later without duplicating the throw/clamp logic, per the
issue's own suggestion.

## Validation evidence (from validation.md)

- `harness/ci.py` full gate: `UNIT_PASSED tests=98`, `UE_BUILD_OK`,
  `UE_AUTOMATION_RESULT passed=1 total=1`, `E2E_PASSED steps=1` → `GATE_OK mode=full`.
- Passed clean on first validation run, no fixes required.
- Reviewed against all 8 MISSION.md Hard Invariants; no violations (no new ability added,
  no color/visual additions, no-kill rule unaffected — Sleep still disables, not destroys).
- Concurrent-task leakage check: `FlatCamera3DPrototypePawn.cpp/.h` in the shared `app/`
  checkout carry unrelated in-progress issue #263 (per-tick cursor-facing) code; confirmed
  absent from this PR's `app-source-tracked/` mirror and unrelated to this change.
