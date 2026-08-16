# Issue #44: Boss base actor (Armed/Vulnerable/Banked state machine + shield/split/enrage hooks)

Adds `ABossBase`, a new `UCLASS(Abstract)` `AActor` subclass giving every future boss
encounter (3 mid-bosses + Drain, PRD 04) a shared, structurally-safe foundation: a
linear `Idle -> Armed -> Vulnerable -> Banked` state machine where `Banked` is the only
terminal/defeat state reachable (no HP-to-zero kill path exists — there is no other enum
value to reach and no `Destroy()` call anywhere in the class), plus three independent
boolean flags (shield, split, enrage) with protected `virtual` hooks
(`OnShieldChanged`/`OnSplitChanged`/`OnEnrageChanged`) a mid-boss subclass overrides to
implement its own twist without touching the base class's transition logic. A new
`KrowdKontrol.Unit.BossBase` Automation Framework test proves both guarantees. This is a
standalone base class — the codebase has no existing PRD 03 enemy AI state machine to
build on yet, so per the issue's own documented fallback, this ships only the minimal
`Banked` terminal state this base actor needs, not the full PRD 03 system.

## Files changed (all under `app/`, gitignored per D-003 — this is the tracked-repo
record of that change, per CLAUDE.md's `app-source-tracked/` carve-out)

| File | Action | What it contains |
|------|--------|-------------------|
| `app/Source/KrowdKontrol/BossBase.h` | CREATE | `EBossState` enum (`Idle`/`Armed`/`Vulnerable`/`Banked`), `FOnBossBanked` dynamic multicast delegate, `ABossBase` class declaration: guarded transition methods, shield/split/enrage getters/setters, protected no-op virtual hooks, private bare-bool state fields |
| `app/Source/KrowdKontrol/BossBase.cpp` | CREATE | Guard-then-transition logic for `AdvanceToArmed()`/`AdvanceToVulnerable()`/`TransitionToBanked()` (flip-before-broadcast ordering, mirroring `APlaceholderTerminalActor::Interact()`); redundant-set-guarded shield/split/enrage setters that call their hook only on an actual value change |
| `app/Source/KrowdKontrol/Private/Tests/BossBaseTestActor.h`/`.cpp` | CREATE | Test-only concrete `ABossBase` subclass overriding the 3 hooks to count invocations, so the test can confirm hooks actually fire (and are skipped on redundant same-value sets) |
| `app/Source/KrowdKontrol/Private/Tests/BossBankedTestListener.h`/`.cpp` | CREATE | Test-only `UObject` listener for `OnBossBanked` (dynamic multicast delegates only bind `UFUNCTION`s via `AddDynamic`, never a lambda), mirroring `RoomClearedTestListener` |
| `app/Source/KrowdKontrol/Private/Tests/KrowdKontrolBossBaseTest.cpp` | CREATE | `KrowdKontrol.Unit.BossBase` — `NewObject<>()`-only, no `UWorld` needed. Proves: default state is `Idle`; skipped-state transitions no-op; full valid progression Idle→Armed→Vulnerable→Banked; repeated calls don't double-advance; every transition method is a permanent no-op once `Banked` is reached, and `OnBossBanked` fires exactly once; the actor is never destroyed (`IsActorBeingDestroyed()` false); shield/split/enrage are independently toggleable, observable via getter and hook call-count, and redundant same-value sets don't re-fire the hook |

No `app/Source/KrowdKontrol/KrowdKontrol.Build.cs` change was needed —
`Core`/`CoreUObject`/`Engine` (already linked) cover everything this class uses.

## Acceptance criteria

- [x] **`ABossBase` exists in `Source/KrowdKontrol/`, exposing `Armed`, `Vulnerable`, and
      terminal `Banked` states, with no path to any other "defeated"-equivalent state**
      — structurally true: no health/damage member exists anywhere in the class, and the
      `EBossState` enum has no value besides `Idle`/`Armed`/`Vulnerable`/`Banked`.
- [x] **Shield, split, and enrage flags exist with generic, subclass-overridable hooks**
      — `SetHasShield`/`SetIsSplit`/`SetIsEnraged` call `OnShieldChanged`/
      `OnSplitChanged`/`OnEnrageChanged` only on an actual value change.
- [x] **`KrowdKontrol.Unit.BossBase` confirms both the no-kill guarantee and the
      hook toggle/observe behavior** — see test description above.
- [x] **No new enemy type, ability, or colour is introduced** — structurally true, no
      enum value, ability reference, or colour reference is added anywhere in this diff.
- [x] **Level 1-3 validation commands pass with exit 0** — see Validation below.
- [x] **Code mirrors existing patterns exactly** — guard-then-broadcast
      (`RoomEnemyBudgetController`), flip-before-broadcast
      (`PlaceholderTerminalActor::Interact()`), bare internal guard fields, plain
      non-Blueprint getters, one dedicated test listener per delegate.
- [x] **No regressions in existing `KrowdKontrol.Unit.*` tests.**

## Validation

`harness/ci.py` (full mode) — `GATE_OK`, `UNIT_PASSED tests=20` (up from the pre-change
baseline of 19, confirming `KrowdKontrol.Unit.BossBase` is included and passing).
`harness/run_ue_automation.sh KrowdKontrol.Unit.BossBase` — `UE_AUTOMATION_RESULT
passed=1 total=1`, `UE_AUTOMATION_OK`. `harness/run_ue_automation.sh KrowdKontrol.Unit.`
(full regression) — `UE_AUTOMATION_RESULT passed=20 total=20`, `UE_AUTOMATION_OK`, no
existing test broken. Module compiled via the engine-bundled `dotnet.exe`
(`Engine/Binaries/ThirdParty/DotNet/10.0/win-x64/dotnet.exe`) against
`UnrealBuildTool.dll`, targeting `KrowdKontrolEditor Win64 Development` — `Result:
Succeeded`. Hard Invariants #2 (no-kill), #3-5 (colour/ability/roster), #6 (Unreal/2D),
#7 (no networking), and #8 (`app/` untracked) all re-checked directly against
`BossBase.h`/`.cpp` and hold; see `validation.md` for the full breakdown. No protected
files touched.
