# Issue #361: Remove Stun-first activation gate blocking control abilities on ranged (SN-1PR) enemies

Full audit of every ability effect-application path (`AEnemyBase::ReceiveControl`,
`UAbilityCastComponent`'s five `TryCast*` entry points, `AbilityData.cpp`) found no
code path anywhere that requires `ControllingAbility == Stun` or any prior-Stun state
before another ability's effect applies to `ASniperEnemy` (or any enemy type).
`ReceiveControl`'s only precondition is `CurrentState == Alert || Attack` -
identical for all 5 abilities and all 4 concrete enemy types - and `ASniperEnemy`
adds no override that changes this. This resolves via the issue's own explicit
fallback branch ("if no such gate is found... still add the regression tests"), not
a functional code fix.

Existing coverage in `KrowdKontrolSniperEnemyTest.cpp` already proved this for 3 of
the 5 abilities (Sleep, Root, Snare - each with an explicit `EEnemyState::Controlled`
assertion after `ReceiveControl` on an Attack-state sniper, no prior Stun cast).
Stun and Fear were missing the equivalent explicit assertion - the only place they
were exercised was a loop that only checked eye-glow intensity, never the resulting
`EEnemyState`. Adds 2 new cases, (d2) Stun and (d3) Fear, mirroring the existing
Root/Snare pattern exactly, reusing the file's own `AdvanceToAttack` helper. All 5
abilities now have explicit `Controlled`-state regression coverage on an
Attack-state sniper with no prior Stun cast.

## Design decisions

- **No production code changed.** The reported gate does not exist in
  `AEnemyBase::ReceiveControl`, `UAbilityCastComponent`'s cast paths, or
  `AbilityData.cpp` - confirmed by tracing all 5 `TryCast*` entry points and their
  shared `ResolvePassedCastGates`/`ApplyControlToEnemiesInShape` helpers, none of
  which reference `EAbilitySlot::Stun` or any other ability's prior application.
- **Likely real-world explanation, flagged but out of scope**: the Sniper's long
  stand-off engagement distance (`GetAttackRangeUnits() == 1400.0f`) sits close to
  Sleep/Root's Long range tier (2000) but beyond Stun (`ThrownCircle`, 800 + 400
  landing radius = 1200 max), Snare (Cone/Medium = 1200), and Fear (fixed 400-unit
  `SelfCircle` around the player). A player casting from typical range would see
  Sleep/Root land and the shorter-range abilities appear to fail until they close
  distance - producing the "feels like Stun-first" impression without any actual
  activation gate. This is a targeting-range/tuning question
  (`docs/prd-ranged-engagement.md` REQ-5, a follow-up balance pass), not an
  activation-gate bug - not fixed here, per the issue's own scope boundary.

## Acceptance criteria

- [x] Audit `ReceiveControl`, `UAbilityCastComponent`'s cast paths, and
      `AbilityData.cpp` for a Stun-first gate - none found; see "Design decisions"
      and `investigation.md`'s evidence chain.
- [x] Remove the gate if found - N/A, no gate exists in code.
- [x] Otherwise add regression tests proving all 5 control abilities apply directly
      to an Attack-state `ASniperEnemy` with no prior Stun cast -
      `KrowdKontrolSniperEnemyTest.cpp` cases (d2)/(d3) (Stun, Fear), joining the
      existing (l2)/(m-snare)/Sleep-expiry cases (Root, Snare, Sleep) for 5/5
      coverage.
- [x] PR body documents the "no gate found" conclusion and the range-mismatch
      explanation for the operator's observation, per the issue's required
      fallback - see PR description.
- [x] Level 1-3 validation commands pass with exit 0 - `harness/ci.py` full mode,
      `GATE_OK`, see `validation.md`.
- [x] No regressions in existing tests - `UNIT_PASSED tests=125` includes the 2 new
      cases alongside all pre-existing ones, unmodified.

---

The real Unreal project stays under `app/` (gitignored, D-003) - this changelog and
its matching `app-source-tracked/` copy are the tracked-repo record of that change,
per D-009. Not a substitute for reading `app-source-tracked/` directly.
