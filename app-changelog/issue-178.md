# Issue #178: Implement Punishment 1: ability-lockout on contact damage

Adds `UAbilityLockoutComponent`, a new `UActorComponent` that locks the player's most
recently successfully cast ability (falling back to `Stun` if nothing has been cast yet
this run) for a fixed 8s duration whenever `UPunishmentManagerComponent::OnPunishmentTriggered`
fires (real contact damage, issue #177, merged as PR #182). `UAbilityCastComponent::TryCastAbility`
gains a new, optional, read-only gate on this lockout state, and the already-merged tray
visual (`UAbilityCooldownTrayWidget::SetSlotLocked`, PR #129, previously a "placeholder
driver only") is wired to this real state via a new `BindAbilityLockoutComponent()`
method, called from `AKrowdKontrolPlayerController::WireWidgetsToPawn` alongside the
existing `BindAbilityUnlockComponent()` call. This is PRD "Punishment System (Punishments
1 & 2 + arbitration)" REQ-2 (P0). Punishment 2 (speed reduction) and cross-punishment
arbitration are separate, later issues.

## Files changed

The real Unreal project lives under `app/` (gitignored symlink, D-003) and is what the
harness actually builds/tests against — `app/` itself is unchanged by this PR's
tracking. What this PR's diff actually contains is a **copy** of the new/changed
source, per D-009, at `app-source-tracked/<same path under app/Source/>`.

| File (under `app-source-tracked/Source/KrowdKontrol/`) | Action | What it contains |
|------|--------|-------------------|
| `AbilityLockoutComponent.h` | CREATE | `FOnAbilityLockoutChanged(EAbilitySlot, bool)` delegate decl + `UAbilityLockoutComponent : public UActorComponent` class decl: `IsAbilityLocked`/`GetRemainingLockoutSeconds` queries, `HandleAbilityCastApplied`/`HandlePunishmentTriggered` handlers, `LockoutDurationSeconds` (default 8s), friend-class test hooks |
| `AbilityLockoutComponent.cpp` | CREATE | Constructor, per-slot `TArray<float>` remaining-time state, tick-driven `AdvanceLockouts` (broadcasts on `>0→<=0` transition), `StartLockout` (broadcasts on `<=0→>0` transition, silent refresh on re-trigger) |
| `AbilityCastComponent.h` / `.cpp` | UPDATE | New optional, read-only lockout gate in `TryCastAbility`, inserted after the existing cooldown gate; asymmetric null handling (missing component does NOT block casting, unlike Unlock/Cooldown) |
| `AbilityCooldownTrayWidget.h` / `.cpp` | UPDATE | New `BindAbilityLockoutComponent()`, binds `OnAbilityLockoutChanged` directly to `SetSlotLocked` (signatures match exactly, no adapter needed); updates the stale "placeholder driver only" doc comment |
| `FlatCamera3DPrototypePawn.h` / `.cpp` | UPDATE | Constructs `AbilityLockoutComponent`, binds it to `AbilityCastComponent->OnAbilityCastApplied` and `PunishmentManagerComponent->OnPunishmentTriggered` |
| `KrowdKontrolPlayerController.cpp` | UPDATE | Calls `AbilityTrayWidget->BindAbilityLockoutComponent(...)` in `WireWidgetsToPawn`, alongside the existing unlock binding |
| `EnemyBase.h` | UPDATE | Adds `friend class FKrowdKontrolHUDWiringTest;` (precedented, 11 prior grants already present) so the new production-wiring test can drive a real enemy to `Alert` via `TickCheckDetection` |
| `Private/Tests/AbilityLockoutChangedTestListener.h` / `.cpp` | CREATE | Minimal `UObject` test listener for `FOnAbilityLockoutChanged`, mirrors `AbilityCastAppliedTestListener` |
| `Private/Tests/KrowdKontrolAbilityLockoutComponentTest.cpp` | CREATE | `KrowdKontrol.Unit.AbilityLockoutComponent` — Stun fallback, most-recently-cast targeting, expiry timing, per-slot independence, delegate-fires-exactly-on-transitions, clamp-to-zero |
| `Private/Tests/KrowdKontrolAbilityCastComponentTest.cpp` | UPDATE | New cases: a locked ability blocks `TryCastAbility`; a missing lockout component does not block casting |
| `Private/Tests/KrowdKontrolAbilityCooldownTrayWidgetTest.cpp` | UPDATE | New case: `BindAbilityLockoutComponent` drives the tray through real activation and expiry |
| `Private/Tests/KrowdKontrolHUDWiringTest.cpp` | UPDATE | New case: the production wiring path (real pawn cast → real punishment broadcast → real tray) actually locks the tray, not just the isolated `Bind*` method |

## Acceptance criteria

- [x] **On the punishment manager's trigger firing, the player's most recently cast
      ability becomes uncastable for a fixed lockout duration (8s default), Stun
      fallback if nothing cast yet.** `AbilityLockoutComponent.{h,cpp}` +
      `FlatCamera3DPrototypePawn` wiring; covered by
      `KrowdKontrolAbilityLockoutComponentTest.cpp` cases (a)/(b).
- [x] **`UAbilityCastComponent::TryCastAbility` gates on the lockout state using the
      same gate-chain pattern as unlock/cooldown, not a parallel mechanism.** New
      optional gate inserted after the cooldown gate in `TryCastAbility`.
- [x] **The merged tray locked-slot visual is driven by this real lockout state,
      replacing its placeholder driver; shows locked for exactly the lockout
      duration and reverts on expiry.** `BindAbilityLockoutComponent` +
      `KrowdKontrolPlayerController::WireWidgetsToPawn` wiring.
- [x] **Automation tests**: (a) lockout blocks `TryCastAbility` while active —
      `KrowdKontrolAbilityCastComponentTest.cpp` new case; (b) lockout expires and
      casting is restored — `KrowdKontrolAbilityLockoutComponentTest.cpp` case (c);
      (c) tray's locked-slot state mirrors the lockout state through activation and
      expiry — `KrowdKontrolAbilityCooldownTrayWidgetTest.cpp` and
      `KrowdKontrolHUDWiringTest.cpp` new cases.
- [x] **Level 1-3 validation passes (`GATE_OK` both modes).** See Validation evidence.
- [x] **Code mirrors existing patterns exactly** — `AbilityCooldownComponent` is the
      direct structural template; delegate-bind idiom matches
      `AbilityCastVFXComponent`'s; `friend class` test-hook convention followed.
- [x] **No regressions in the existing automation suite** — 71 tests passing (65
      baseline + new lockout coverage), all previously-existing cases unchanged.
- [x] **Every edit lands under both `app/` and `app-source-tracked/`.** Diffed
      byte-for-byte identical for all 16 files before commit (see `validation.md`).

## Validation evidence

`python harness/ci.py` (full mode, real headless Unreal Editor rebuild + Automation
Framework run):

```
HARNESS_START mode=full driver=cli
STATIC_SKIPPED no 'static' command in harness.config.json
UNIT_PASSED tests=71
APP_STARTED driver=cli
UE_BUILD_OK
UE_AUTOMATION_RESULT passed=1 total=1
UE_AUTOMATION_OK
E2E_PASSED steps=1
HOLDOUT_ABSENT no .factory/holdout/run.py
MUTATIONS_ABSENT no harness/mutations/run.py
GATE_OK mode=full
```

Green on first run — no fixes needed. Hard invariants (`MISSION.md` #1-#8) reviewed
directly against the diff: not at risk (player-side punishment only, no enemy HP/death
logic, no colour/ability-roster changes, no engine/networking config touched); the one
non-planned file (`EnemyBase.h`) is an additive test-only `friend class` grant matching
an existing 11-instance pattern. See `validation.md` for the full breakdown.
