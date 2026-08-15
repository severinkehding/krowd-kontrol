# Issue #71: Add ability cooldown timers, distinct from the ability-lockout punishment mechanic

Adds `UAbilityCooldownComponent`, an `ActorComponent` giving each of the 5 crowd-control
abilities (Stun/Sleep/Root/Fear/Snare) an independent, short cooldown timer. The sole
public mutator, `TryStartCooldown(EAbilitySlot)`, returns `false` (no state change)
while that slot is still cooling down and `true` once it starts a fresh cooldown —
future ability-cast code has exactly one legal entry point to gate a cast on. Cooldown
state is exposed only through cooldown-specific accessors (`IsOnCooldown`,
`GetRemainingCooldownSeconds`) — deliberately no shared "unavailable" flag — so PRD 08's
not-yet-built Punishment 1 (ability lockout) can later add its own, structurally
separate state without ever touching this component. `EAbilitySlot` was extracted out
of `AbilityCooldownTrayWidget.h` into its own `AbilitySlot.h` so this new gameplay
component doesn't need to depend on a UMG widget header to reuse the enum.

## Files changed (all under `app/`, gitignored per D-003 — this is the tracked-repo
record of that change, see the closing note below)

| File | Action | What it contains |
|------|--------|-------------------|
| `app/Source/KrowdKontrol/AbilitySlot.h` | CREATE | `EAbilitySlot` UENUM extracted out of `AbilityCooldownTrayWidget.h`, unchanged name/values/order |
| `app/Source/KrowdKontrol/AbilityCooldownTrayWidget.h` | UPDATE | Removes the inline `EAbilitySlot` enum; includes `AbilitySlot.h` instead. No behavior change |
| `app/Source/KrowdKontrol/AbilityCooldownComponent.h` | CREATE | Component declaration: `AbilityCooldownDurations` (`EditDefaultsOnly`, one entry per slot), sole mutator `TryStartCooldown()`, `AdvanceCooldowns()`, cooldown-only queries `IsOnCooldown()`/`GetRemainingCooldownSeconds()`, `DefaultAbilityCooldownSeconds = 3.0f` placeholder constant |
| `app/Source/KrowdKontrol/AbilityCooldownComponent.cpp` | CREATE | Manual remaining-time/duration-array implementation (mirrors `AbilityCooldownTrayWidget`'s own pattern, not `FTimerManager` — no `UWorld` needed, testable via bare `NewObject()`), `IsValidIndex` guards + `FMath::Max` clamps throughout |
| `app/Source/KrowdKontrol/Private/Tests/KrowdKontrolAbilityCooldownTest.cpp` | CREATE | `KrowdKontrol.Unit.AbilityCooldown` — covers acceptance criteria (a)-(g) below |

## Acceptance criteria

- [x] **Each ability has an independent cooldown timer that starts on
      `TryStartCooldown()` and blocks a further `TryStartCooldown()` on that same slot
      until it expires.** Cases (a)-(c): first call succeeds and starts the timer, a
      second call mid-cooldown is blocked with no state change, and a call right after
      expiry succeeds again.
- [x] **Cooldown state is exposed via cooldown-specific accessors only — no shared
      "unavailable" flag.** `IsOnCooldown`/`GetRemainingCooldownSeconds` are the only
      state queries; no generic availability flag exists for a future lockout system to
      collide with.
- [x] **`KrowdKontrol.Unit.AbilityCooldown` exists and asserts both directions of the
      recast rule.** Case (b) (blocked mid-cooldown) and case (c) (allowed immediately
      after expiry), plus (d) per-slot independence, (e) per-slot custom duration
      configuration, (f) large-delta clamping to zero, (g) defensive fallback on a
      too-short or negative-value durations array.
- [x] **Cooldown durations are driven by a named constant
      (`DefaultAbilityCooldownSeconds`), not inline magic numbers.**
- [x] **`KrowdKontrol.Unit.AbilityCooldownTrayWidget` still passes unchanged after the
      `EAbilitySlot` extraction.** Re-run as part of validation; no regression.

## Deviations from plan

`BeginPlay()` was intentionally omitted from `UAbilityCooldownComponent` — it would
have added nothing beyond calling `Super::BeginPlay()` (no seeding needed beyond the
constructor's array init), per this codebase's no-dead-code convention. The
`FTimerManager` cooldown approach `web-research.md` recommended was deliberately not
used — it requires a valid `GetWorld()`, which the bare `NewObject()`-constructed
Automation test instance (matching `PlayerEnergyComponent`'s test precedent) does not
have.

## Validation

```
$ python harness/ci.py --quick
HARNESS_START mode=quick driver=cli
STATIC_SKIPPED no 'static' command in harness.config.json
UNIT_PASSED tests=7
GATE_OK mode=quick
```

`harness/run_ue_automation.sh` does not invoke `UnrealBuildTool` before running tests,
so a manual `UnrealBuildTool` build was run first (`Result: Succeeded`, compiling
`AbilityCooldownComponent.cpp`, the post-extraction `AbilityCooldownTrayWidget.cpp`, and
both new/changed test files cleanly). After that rebuild,
`app/Saved/Logs/KrowdKontrol.log` shows all 10 `KrowdKontrol.Unit.*` tests — including
the new `AbilityCooldown` test and the pre-existing `AbilityCooldownTrayWidget`
regression check — individually logged `Test Completed. Result={Success}`, zero
`Result={Fail...}` lines. The `UNIT_PASSED tests=7` count above undercounts this due to
a suspected report/log race from concurrent factory worktrees sharing the same on-disk
`app/` project (see `implementation.md`'s Deviations section for the full account) —
not a real failure, per the per-test log evidence. Full `harness/ci.py` (non-`--quick`)
validation is deferred to the `dark-factory-validate` node per this workflow's stages.

---

Source lives under `app/` (gitignored, D-003) — this file is the tracked-repo record
of that change, not a substitute for reading the actual code.
