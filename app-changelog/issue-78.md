# Issue #78: Add UPlayerEnergyComponent

Adds `UPlayerEnergyComponent`, an `ActorComponent` for the player pawn that tracks
`CurrentEnergy`/`MaxEnergy` and enforces, by construction, that energy can only ever
be reduced through a single public entry point, `ApplyContactDamage()` — no other
public mutator exists or may be added (PRD 01 REQ-4: energy must only decrease from
enemy contact, never from casting an ability). Each hit's raw damage is clamped to a
configurable `MaxDamagePerHit` before being applied (REQ-6: enemy count, not per-hit
damage, is the difficulty lever). Broadcasts `OnEnergyChanged` for future HUD
consumption; nothing subscribes to it yet. Scope deliberately excludes enemy AI and
ability-casting — those are separate PRDs (see the issue's own Notes).

## Files changed (all under `app/`, gitignored per D-003 — this is the tracked-repo
record of that change, see the closing note below)

| File | Action | What it contains |
|------|--------|-------------------|
| `app/Source/KrowdKontrol/PlayerEnergyComponent.h` | CREATE | Component declaration: `MaxEnergy`/`MaxDamagePerHit` (`EditDefaultsOnly`), `CurrentEnergy` (`BlueprintReadOnly`, not `ReadWrite` — no Blueprint pin can bypass the mutator), `FOnEnergyChanged` dynamic multicast delegate, sole public mutator `ApplyContactDamage(float RawAmount, AActor* DamageSource)` |
| `app/Source/KrowdKontrol/PlayerEnergyComponent.cpp` | CREATE | `BeginPlay()` seeds `CurrentEnergy` to `MaxEnergy`; `ApplyContactDamage()` clamps `RawAmount` to `[0, SafeMaxDamagePerHit]` (both bounds — see review fix below), subtracts, clamps `CurrentEnergy` to `[0, MaxEnergy]`, broadcasts `OnEnergyChanged` only when the value actually changed, returns the actual amount subtracted |
| `app/Source/KrowdKontrol/Private/Tests/KrowdKontrolPlayerEnergyComponentTest.cpp` | CREATE | `KrowdKontrol.Unit.PlayerEnergyComponent` — covers acceptance criteria (a)/(b)/(c) below |

## Acceptance criteria

- [x] **Compiles as part of the `KrowdKontrol` module, no new module dependencies**
      beyond Core/CoreUObject/Engine (already enabled). Confirmed via a real
      `UnrealBuildTool` invocation (`Result: Succeeded`).
- [x] **(a) `ApplyContactDamage` reduces `CurrentEnergy` by the expected clamped
      amount.** Directly tested.
- [x] **(b) A raw damage value above `MaxDamagePerHit` is clamped down, not applied
      in full.** `FMath::Clamp(RawAmount, 0.0f, SafeMaxDamagePerHit)`; tested.
- [x] **(c) `CurrentEnergy` never drops below 0.** `FMath::Clamp(CurrentEnergy -
      ClampedDamage, 0.0f, MaxEnergy)`; tested.
- [x] **No public API other than `ApplyContactDamage` can reduce energy.**
      `CurrentEnergy` is `BlueprintReadOnly` (not `ReadWrite`); no setter exists.
      Enforced by the class's own doc-comment as an explicit invariant for future
      edits, not just current absence.

## Review finding (fixed before this PR)

Code review caught that clamping only the *upper* bound of `RawAmount`
(`FMath::Clamp(RawAmount, 0.0f, MaxDamagePerHit)` without also floor-guarding
`MaxDamagePerHit` itself) would let a negative `MaxDamagePerHit` — e.g. a bad
editor/DataTable value — flip `ApplyContactDamage` into healing the player instead of
damaging them. Fixed with `SafeMaxDamagePerHit = FMath::Max(0.0f, MaxDamagePerHit)`
applied before the main clamp, plus a regression test covering a negative
`MaxDamagePerHit` input.

## Validation

```
$ python harness/ci.py
HARNESS_START mode=full driver=cli
STATIC_SKIPPED no 'static' command in harness.config.json
UNIT_PASSED tests=3
APP_STARTED driver=cli
UE_AUTOMATION_RESULT passed=1 total=1
UE_AUTOMATION_OK
E2E_PASSED steps=1
HOLDOUT_ABSENT no .factory/holdout/run.py - NOTHING above the independence line ran.
MUTATIONS_ABSENT no harness/mutations/run.py - this gate has never been shown to fail.
GATE_OK mode=full
```

`UNIT_PASSED tests=3` covers the new `KrowdKontrol.Unit.PlayerEnergyComponent` test
alongside the two pre-existing `KrowdKontrol.Unit.*` tests — no regression. MISSION.md
Hard Invariants reviewed by inspection: no kill/destroy semantics, no colour usage, no
new ability or enemy type, no networking code — this is a player energy/damage
component, unrelated to enemy banking.

---

The real Unreal project stays under `app/` (gitignored, D-003) — this changelog and
its matching `app-source-tracked/` copy are the tracked-repo record of that change,
per D-009. Not a substitute for reading `app-source-tracked/` directly.
