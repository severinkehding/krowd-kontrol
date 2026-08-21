# Issue #181: Per-punishment debug CVars for ability-lockout and speed-reduction

Adds two console variables, `kk.Punishment.LockoutEnabled` and
`kk.Punishment.SpeedReductionEnabled`, both defaulting to `1` (enabled), that let a
developer disable either punishment independently at runtime for isolated playtesting.
Each CVar is a file-scope `static TAutoConsoleVariable<int32>` declared in the owning
component's own `.cpp` and checked as the first line of that component's
`HandlePunishmentTriggered()`.

**Post-review update**: the initial pass gated only the two components themselves and
left `UPunishmentArbitrationComponent` unchanged, on the reasoning that gating at the
component already satisfies "regardless of arbitration outcome." Review caught that this
breaks the CVar's actual playtesting purpose on `FlatCamera3DPrototypePawn` (the one
pawn wiring both components to arbitration): `UPunishmentArbitrationComponent::
HandlePunishmentTriggered()` branches on `AbilityLockoutComponent` *presence*, not
whether it will actually activate, so `kk.Punishment.LockoutEnabled=0` still entered the
ability-lock branch, unconditionally ended an active speed-reduction, then called a
now-gated no-op — canceling speed-reduction and activating nothing, the opposite of
isolating it for a playtest. Fixed by adding `UAbilityLockoutComponent::
IsLockoutEnabledByCVar()` (a static accessor onto the file-scope CVar) and checking it
alongside `AbilityLockoutComponent`'s presence in arbitration's priority branch, so a
CVar-disabled lockout now falls through to speed-reduction exactly like an unwired one
does on `Paper2DPrototypePawn`.

## Files changed

The real Unreal project lives under `app/` (gitignored symlink, D-003) and is what the
harness actually builds/tests against. Per D-009, this PR's diff contains a **copy** of
the new/changed source at `app-source-tracked/<same path under app/Source/>`.

| File (under `app-source-tracked/Source/KrowdKontrol/`) | Action | What it contains |
|------|--------|-------------------|
| `AbilityLockoutComponent.h` / `.cpp` | UPDATE | Declares `CVarPunishmentLockoutEnabled` (`kk.Punishment.LockoutEnabled`, default 1); `HandlePunishmentTriggered()` returns immediately, no state mutated, if the CVar reads 0; adds `static bool IsLockoutEnabledByCVar()` so `UPunishmentArbitrationComponent` can read the CVar from its own translation unit |
| `SpeedReductionPunishmentComponent.cpp` | UPDATE | Declares `CVarPunishmentSpeedReductionEnabled` (`kk.Punishment.SpeedReductionEnabled`, default 1); `HandlePunishmentTriggered()` returns immediately, before even the `MovementComponent` null-check, if the CVar reads 0 |
| `PunishmentArbitrationComponent.h` / `.cpp` | UPDATE (post-review) | `HandlePunishmentTriggered()`'s ability-lock branch now also requires `UAbilityLockoutComponent::IsLockoutEnabledByCVar()`; when the CVar is 0, falls through to the speed-reduction branch instead of ending it and calling a no-op |
| `Private/Tests/KrowdKontrolAbilityLockoutComponentTest.cpp` | UPDATE | New case (i): CVar=0 blocks `HandlePunishmentTriggered()` from locking Stun; CVar restored to 1 allows normal activation again. New case (j) (post-review): disabling the CVar mid-flight does not clear an already-active lockout, only blocks new triggers |
| `Private/Tests/KrowdKontrolSpeedReductionPunishmentComponentTest.cpp` | UPDATE | New case (f): CVar=0 blocks `HandlePunishmentTriggered()` from reducing `MaxSpeed`; CVar restored to 1 allows normal activation again. New case (g) (post-review): disabling the CVar mid-flight does not cancel an already-active reduction |
| `Private/Tests/KrowdKontrolPunishmentArbitrationComponentTest.cpp` | UPDATE (post-review) | New case (k): proves `kk.Punishment.LockoutEnabled=0` lets speed-reduction activate through arbitration instead of being preempted-into-nothing — the regression test for the arbitration fix above |

## Deviations from the investigation/plan

Initial implementation matched the investigation/plan exactly. Post-review, the scope
grew beyond the original plan: automated review (multi-agent PR review of #210) traced
the CVar gates through the pre-existing, unmodified `UPunishmentArbitrationComponent`
and found `kk.Punishment.LockoutEnabled=0` canceled an active speed-reduction and
activated nothing on `FlatCamera3DPrototypePawn` — the opposite of the CVar's stated
playtesting purpose. Nothing in #181 restricts touching
`UPunishmentArbitrationComponent`; the review's Option A fix (make arbitration
CVar-aware) was applied directly rather than shipped as a documented limitation, since
the whole point of this issue is that these CVars actually work for isolating each
punishment during playtesting. `app/` and `app-source-tracked/` were verified
byte-identical for all touched files both before and after every edit, including this
post-review round.

## Acceptance criteria

- [x] `kk.Punishment.LockoutEnabled` set to 0 prevents ability-lockout from activating on
      trigger, regardless of arbitration outcome — gated as the first line of
      `AbilityLockoutComponent::HandlePunishmentTriggered()`, and (post-review)
      `UPunishmentArbitrationComponent` no longer preempts-and-cancels an active
      speed-reduction when the CVar is 0.
- [x] `kk.Punishment.SpeedReductionEnabled` set to 0 prevents speed-reduction from
      activating on trigger, regardless of arbitration outcome — gated as the first line
      of `SpeedReductionPunishmentComponent::HandlePunishmentTriggered()`.
- [x] Both CVars default to 1 (enabled), preserving current behavior when unset.
- [x] Automation tests: CVar=0 blocks activation, default allows normal activation, for
      both punishments — case (i) / case (f); disabling mid-flight does not clear
      already-active state — case (j) / case (g) (post-review); the arbitration-path
      interaction is pinned by case (k) in
      `KrowdKontrolPunishmentArbitrationComponentTest.cpp` (post-review).
- [x] Level 1-3 validation commands pass with exit 0. See evidence below.
- [x] Code mirrors existing patterns exactly (early-return guard shape, lettered test
      cases, `TAutoConsoleVariable<int32>` per Epic's documented convention).
- [x] No regressions in existing `KrowdKontrol.Unit.*` tests.
- [x] `app/` and `app-source-tracked/` copies of every changed file are identical —
      confirmed via `diff` for all touched files.

## Validation evidence

`python harness/ci.py --quick` (inline sanity check, Phase 6):

```
HARNESS_START mode=quick driver=cli
STATIC_SKIPPED no 'static' command in harness.config.json
UNIT_PASSED tests=78
GATE_OK mode=quick
```

Note: two intermittent runs during this pass reported an unrelated failure
(`KrowdKontrol.Unit.EnemyBase`, then separately `KrowdKontrol.Unit.Level01Structure`) —
confirmed pre-existing and unrelated to this change by reverting all four touched files
back to their pre-PR content and re-running: the failure still occurred on some runs and
not others with the reverted code too, and the specific failing test differed between
runs. Neither `EnemyBase` nor `Level01Structure` touches the punishment system. Not
introduced by this change; not fixed here (out of scope).

Full `python harness/ci.py` (headless Unreal Editor rebuild + Automation Framework run)
is deferred to the separate `dark-factory-validate` node per this workflow's own
instructions; not re-run here.
