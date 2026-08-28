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

## Verifiable evidence (pass-1 review follow-up)

Pass-1 review correctly noted this diff touches no production file, so the audit
conclusion above can't be taken on faith from prose alone. Quoting the exact
precondition logic at its current `file:line` in `app/` (the real Unreal project
source; `app/` is a gitignored symlink per `CLAUDE.md` D-003, so it can't itself
appear in the diff) so it's checkable directly against this changelog entry:

**1. `AEnemyBase::ReceiveControl` — the only state gate, `Source/KrowdKontrol/EnemyBase.cpp:69,94-98`:**

```cpp
void AEnemyBase::ReceiveControl(EAbilitySlot Ability)
{
    ...
    if (CurrentState != EEnemyState::Alert && CurrentState != EEnemyState::Attack)
    {
        return;
    }
    CurrentState = EEnemyState::Controlled;
```

No `Ability`-identity check, no `ControllingAbility`-history check, no
`EAbilitySlot::Stun` reference anywhere in the function. `ASniperEnemy` does not
override `ReceiveControl`.

**2. All 5 `TryCast*` entry points share one gate, `Source/KrowdKontrol/AbilityCastComponent.cpp:313-386`:**

`TryCastAbility` (line 41), `TryCastThrownAbilityAtLocation` (line 76),
`TryCastLineAbilityTowardLocation` (line 173), `TryCastConeAbilityTowardLocation`
(line 252), and `TryCastSelfCircleAbility` (line 286) all call
`ResolvePassedCastGates(Ability, ...)` first. Its full gate list, in order: world
not paused, briefing card not visible, `Owner` non-null, ability unlocked
(`UAbilityUnlockComponent`), ability not on cooldown (`UAbilityCooldownComponent`),
ability not locked out (`UAbilityLockoutComponent`, optional). None of these six
checks reference `EAbilitySlot::Stun`, `ControllingAbility`, or any other
ability's prior application — they gate on cast eligibility of the ability being
cast, never on what ability (if any) was cast before it.

**3. The only enemy-state gate before `ReceiveControl` is called — Alert/Attack, same as inside `ReceiveControl` itself:**

`TryCastAbility`'s single-target path, `FindNearestValidTarget`,
`Source/KrowdKontrol/AbilityCastComponent.cpp:414-418`:
```cpp
    const EEnemyState State = It->GetEnemyState();
    if (State != EEnemyState::Alert && State != EEnemyState::Attack)
    {
        continue;
    }
```

The other 4 entry points' shared shape-application path,
`ApplyControlToEnemiesInShape`, `Source/KrowdKontrol/AbilityCastComponent.cpp:440-441`:
```cpp
    const bool bWasFreshlyTargetable = (Enemy->GetEnemyState() == EEnemyState::Alert || Enemy->GetEnemyState() == EEnemyState::Attack);
    Enemy->ReceiveControl(Ability);
```

**4. `AbilityData.cpp` (189 lines, full file audited) holds no gating logic at all** —
it's a pure per-ability data table (duration, range, target shape, colour,
countered enemy type, behaviour flags). No field or function anywhere in the file
branches on `CurrentState`, `ControllingAbility`, or a prior cast of any other
ability; `AbilityData::Get(EAbilitySlot::Stun)` returns a plain `FAbilityData`
struct literal with no reference to any other ability.

### Pre-existing Sleep/Root/Snare coverage (unmodified — quoted, not in diff)

The other pass-1 follow-up flagged that this PR body cited stale line numbers
(269-275, 306-312) for pre-existing Root/Snare coverage, and that none of the 3
pre-existing per-ability assertions (Sleep, Root, Snare) are visible in the diff
since this PR doesn't touch them. Corrected current line numbers, quoted verbatim
so all 5 abilities' coverage is checkable from this changelog alone:

**Sleep — `Private/Tests/KrowdKontrolSniperEnemyTest.cpp:437-446`:**
```cpp
ASniperEnemy* ExpirySniper = NewObject<ASniperEnemy>();
AdvanceToAttack(ExpirySniper, ZeroDistanceLocation);
ExpirySniper->ReceiveControl(EAbilitySlot::Sleep); // Attack -> Controlled, 7.0f override
TestEqual(TEXT("GetTotalControlledSeconds should reflect the 7s Sleep override, not the base duration"),
    ExpirySniper->GetTotalControlledSeconds(), 7.0f);
...
ExpirySniper->TickControlledDuration(6.9f);
TestEqual(TEXT("Sniper should still be Controlled just under the 7s override"),
    static_cast<uint8>(ExpirySniper->GetEnemyState()), static_cast<uint8>(EEnemyState::Controlled));
```

**Root — `Private/Tests/KrowdKontrolSniperEnemyTest.cpp:287-293`:**
```cpp
ASniperEnemy* RootedSniper = NewObject<ASniperEnemy>();
AdvanceToAttack(RootedSniper, ZeroDistanceLocation);
TestTrue(TEXT("(l2) Attack tell should be visibly on before Root interrupts"),
    RootedSniper->AttackTellLightComponent->Intensity > 0.0f);
RootedSniper->ReceiveControl(EAbilitySlot::Root);
TestEqual(TEXT("(l2) Sniper should be Controlled after Root"),
    static_cast<uint8>(RootedSniper->GetEnemyState()), static_cast<uint8>(EEnemyState::Controlled));
```

**Snare — `Private/Tests/KrowdKontrolSniperEnemyTest.cpp:326-330`:**
```cpp
ASniperEnemy* SnaredSniper = NewObject<ASniperEnemy>();
AdvanceToAttack(SnaredSniper, ZeroDistanceLocation);
SnaredSniper->ReceiveControl(EAbilitySlot::Snare);
TestEqual(TEXT("(m-snare) Sniper should be Controlled after Snare"),
    static_cast<uint8>(SnaredSniper->GetEnemyState()), static_cast<uint8>(EEnemyState::Controlled));
```

Combined with this PR's new (d2) Stun (`:170-174`) and (d3) Fear (`:177-181`)
cases (both fully visible in the diff), all 5 control abilities now have an
explicit, verifiable `Attack -> Controlled` assertion with no prior Stun cast.

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
