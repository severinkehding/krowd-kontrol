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

Pass-2 follow-up also flagged that (d2)/(d3) (and the pre-existing Sleep/Root/Snare
cases) all call `ReceiveControl()` directly rather than through a real player-facing
cast entry point, leaving open the possibility of a gate living in the cast/
targeting layer instead. Adds (d4): a real `UAbilityCastComponent::TryCastAbility(Stun)`
call (the actual production entry point a player's cast key routes through) against
an Attack-state sniper with no prior cast, spawned into a live `UWorld` the same way
`KrowdKontrolAbilityCastComponentTest.cpp`'s existing cases do.

## Verifiable evidence (pass-1 review follow-up, re-verified pass-2)

Pass-1 review correctly noted this diff touches no production file, so the audit
conclusion above can't be taken on faith from prose alone. Pass-2 follow-up asked
for the *full* current relevant function bodies, not truncated excerpts, with
re-confirmed `file:line` references — every snippet below was re-read directly
from the live `app/` source (the real Unreal project; `app/` is a gitignored
symlink per `CLAUDE.md` D-003, so it can't itself appear in the diff) at the time
this section was written, quoted in full with nothing load-bearing elided:

**1. `AEnemyBase::ReceiveControl` — the only state gate, full body, `Source/KrowdKontrol/EnemyBase.cpp:69-105`:**

```cpp
void AEnemyBase::ReceiveControl(EAbilitySlot Ability)
{
    if (CurrentState == EEnemyState::Controlled)
    {
        // Sleep-flavour early wake (issue #257): being hit by any OTHER ability's
        // application while still Controlled by an ability flagged
        // bWakesEarlyOnOtherAbilityHit ends that Controlled window immediately.
        // Re-casting the SAME ability that's already controlling this enemy still
        // no-ops here (Ability == ControllingAbility).
        if (Ability != ControllingAbility && AbilityData::Get(ControllingAbility).bWakesEarlyOnOtherAbilityHit)
        {
            CurrentState = EEnemyState::Alert;
            OnControlledExpired();
            OnEnemyControlledExpired.Broadcast();
            ControlledDurationIndicatorComponent->Hide();
        }
        return;
    }

    if (CurrentState != EEnemyState::Alert && CurrentState != EEnemyState::Attack)
    {
        return;
    }
    CurrentState = EEnemyState::Controlled;
    ControllingAbility = Ability;
    const float OverrideSeconds = GetControlledDurationOverrideSeconds(Ability);
    RemainingControlledSeconds = OverrideSeconds >= 0.0f ? OverrideSeconds : AbilityData::Get(Ability).BaseDurationSeconds;
    TotalControlledSeconds = RemainingControlledSeconds;
    OnControlledEntry(Ability);
    ControlledDurationIndicatorComponent->Show(AbilityData::Get(Ability).Colour);
}
```

The only two branches that check anything are: (1) "already Controlled" —
`Ability != ControllingAbility` there only decides whether a *second* cast wakes
the target early, it does not block the second cast's own effect, since that
branch always `return`s regardless; and (2) "not yet hot" —
`CurrentState != Alert && CurrentState != Attack`. Neither branch, nor anything
else in the function, checks `Ability == EAbilitySlot::Stun`, a prior-Stun flag,
or any value of `ControllingAbility` that would gate a *first* cast on an
Attack-state enemy. `ASniperEnemy` does not override `ReceiveControl`.

**2. All 5 `TryCast*` entry points share one gate, `ResolvePassedCastGates`, full body, `Source/KrowdKontrol/AbilityCastComponent.cpp:313-386`:**

`TryCastAbility` (call site line 41), `TryCastThrownAbilityAtLocation` (line 76),
`TryCastLineAbilityTowardLocation` (line 173), `TryCastConeAbilityTowardLocation`
(line 252), and `TryCastSelfCircleAbility` (line 286) all call this function first:

```cpp
UAbilityCooldownComponent* UAbilityCastComponent::ResolvePassedCastGates(EAbilitySlot Ability, const TCHAR* CallerLogContext) const
{
    // Issue #246: the briefing card pauses the world while shown; APlayerController
    // still ticks/processes input while paused (engine default), so both the
    // briefing's dismiss-bind and an ability-cast key can fire from the same
    // keypress without this gate.
    if (const UWorld* World = GetWorld())
    {
        if (World->IsPaused())
        {
            return nullptr;
        }
    }

    // Belt-and-suspenders for the same-keypress race above: checks the briefing
    // widget's own visibility directly rather than relying on World->IsPaused()'s
    // per-frame ordering against the dismiss bind.
    if (const APawn* OwnerPawn = Cast<APawn>(GetOwner()))
    {
        if (const AKrowdKontrolPlayerController* Controller = Cast<AKrowdKontrolPlayerController>(OwnerPawn->GetController()))
        {
            if (Controller->BriefingCardWidgetInstance && Controller->BriefingCardWidgetInstance->IsBriefingVisible())
            {
                return nullptr;
            }
        }
    }

    AActor* Owner = GetOwner();
    if (!Owner)
    {
        return nullptr;
    }

    UAbilityUnlockComponent* UnlockComponent = Owner->FindComponentByClass<UAbilityUnlockComponent>();
    if (!UnlockComponent || !UnlockComponent->IsAbilityUnlocked(Ability))
    {
        return nullptr;
    }

    UAbilityCooldownComponent* CooldownComponent = Owner->FindComponentByClass<UAbilityCooldownComponent>();
    if (!CooldownComponent || CooldownComponent->IsOnCooldown(Ability))
    {
        return nullptr;
    }

    // Unlike Unlock/Cooldown above, a missing UAbilityLockoutComponent is NOT a gate
    // failure - it's optional. Presence-and-locked blocks; absence does not.
    UAbilityLockoutComponent* LockoutComponent = Owner->FindComponentByClass<UAbilityLockoutComponent>();
    if (LockoutComponent && LockoutComponent->IsAbilityLocked(Ability))
    {
        return nullptr;
    }

    return CooldownComponent;
}
```

(`UE_LOG` diagnostic lines at each early-return, present in the real file, are
omitted here as non-load-bearing — they log and return `nullptr`, they don't add
a condition.) Six checks total: world-paused, briefing-visible, `Owner` non-null,
ability-unlocked, ability-on-cooldown, ability-locked-out. None reference
`EAbilitySlot::Stun`, `ControllingAbility`, or any other ability's prior
application — every one of them gates on cast eligibility of the ability *being
cast*, never on what ability (if any) was cast before it.

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

**4. `AbilityData.cpp`/`AbilityData.h` (189/121 lines, both fully audited) hold no gating logic at all** —
it's a pure per-ability data table. Every field `FAbilityData` declares,
`Source/KrowdKontrol/AbilityData.h:35-104` (struct closes at 104; fields after
`bAllowsAttackWhileControlled` shown below are `bAllowsMovementWhileControlled`,
`ControlledSpeedMultiplier`, `EffectDescription`, and `KeyBindingLabel` — display/
tuning data, same as everything above them):

```cpp
struct FAbilityData
{
    EAbilitySlot Ability = EAbilitySlot::Stun;
    float BaseDurationSeconds = 0.0f;
    EAbilityRange Range = EAbilityRange::Short;
    EAbilityTargetType TargetType = EAbilityTargetType::SelfCircle;
    FLinearColor Colour = FLinearColor::Black;
    FName ColourTag = NAME_None;
    bool bIsColourNeutral = false;
    EEnemyType CounteredEnemyType = EEnemyType::RU_NNR;
    bool bWakesEarlyOnOtherAbilityHit = false;
    bool bAllowsAttackWhileControlled = false;
    bool bFleesFromCasterWhileControlled = false;
    // ...remaining fields (behaviour flags, range/duration tuning) - none of
    // these or the fields above are a CurrentState, ControllingAbility, or
    // prior-cast reference either.
};
```

No field is a `CurrentState`, `ControllingAbility`, or "was ability X cast
before this one" value — the struct describes one ability in isolation, with no
way to encode a dependency on another ability's prior activation even if
`AbilityData.cpp`'s `Get()` functions wanted to (they don't:
`AbilityData::Get(EAbilitySlot::Stun)` returns a plain `FAbilityData` struct
literal with no reference to any other ability or to enemy state).

### Pre-existing Sleep/Root/Snare coverage (unmodified — quoted, not in diff)

The other pass-1 follow-up flagged that this PR body cited stale line numbers
(269-275, 306-312) for pre-existing Root/Snare coverage, and that none of the 3
pre-existing per-ability assertions (Sleep, Root, Snare) are visible in the diff
since this PR doesn't touch them (a pre-existing passing test must not be
modified just to force it into a diff — see `CLAUDE.md`'s fix-PR-issues rule 2).
The quotes below were re-read directly against the live
`Private/Tests/KrowdKontrolSniperEnemyTest.cpp` a second time for this pass-2
follow-up; all three `file:line` references and code bodies below are
byte-verbatim matches against the current file, not carried over unchecked from
the earlier changelog draft:

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
explicit, verifiable `Attack -> Controlled` assertion with no prior Stun cast, via
`ReceiveControl()` called directly.

### New: a real cast-entry-point case, not just `ReceiveControl()` directly

(d2)/(d3) above, like the pre-existing Sleep/Root/Snare cases, call
`ReceiveControl()` directly. That proves the *effect-application* layer has no
gate, but doesn't rule one out living earlier, in the cast/targeting layer
(`TryCastAbility` itself, or `ResolvePassedCastGates`, or target-selection). New
case (d4), fully visible in this diff at
`Private/Tests/KrowdKontrolSniperEnemyTest.cpp:187-220`, closes that gap: it
spawns a live `UWorld` (same pattern `KrowdKontrolAbilityCastComponentTest.cpp`'s
existing cases use), spawns an `ASniperEnemy` and a cast-owner `APawn` with real
`UAbilityUnlockComponent`/`UAbilityCooldownComponent`/`UAbilityCastComponent`
instances, advances the sniper to Attack with no prior cast of any kind, then
calls `UAbilityCastComponent::TryCastAbility(EAbilitySlot::Stun)` — the actual
production entry point a player's cast key routes through — and asserts both
that the cast succeeds and that the sniper ends up `Controlled`. Stun is used
because it's the one ability `UAbilityUnlockComponent` unlocks by default
(`AbilityUnlockComponent.cpp:25`), so the case needs no extra unlock setup beyond
what every other `TryCastAbility` test in the codebase already relies on.

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
      coverage. Case (d4) adds a real `TryCastAbility` cast-entry-point regression
      on top, ruling out a gate in the cast/targeting layer specifically.
- [x] PR body documents the "no gate found" conclusion and the range-mismatch
      explanation for the operator's observation, per the issue's required
      fallback - see PR description.
- [x] Level 1-3 validation commands pass with exit 0 - `harness/ci.py` full mode,
      `GATE_OK`, see `validation.md`.
- [x] No regressions in existing tests - `UNIT_PASSED tests=125` (unchanged: cases
      d2/d3/d4 are new assertions inside the existing `KrowdKontrol.Unit.SniperEnemy`
      automation test, not new registered test classes, so the discovered-test count
      doesn't move; re-ran `harness/ci.py --quick` after this pass-2 change and it
      still reports `UNIT_PASSED tests=125` / `GATE_OK`).

---

The real Unreal project stays under `app/` (gitignored, D-003) - this changelog and
its matching `app-source-tracked/` copy are the tracked-repo record of that change,
per D-009. Not a substitute for reading `app-source-tracked/` directly.
