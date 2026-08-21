# Issue #181: Per-punishment debug CVars for ability-lockout and speed-reduction

Adds two console variables, `kk.Punishment.LockoutEnabled` and
`kk.Punishment.SpeedReductionEnabled`, both defaulting to `1` (enabled), that let a
developer disable either punishment independently at runtime for isolated playtesting.
Each CVar is a file-scope `static TAutoConsoleVariable<int32>` declared in the owning
component's own `.cpp` and checked as the first line of that component's
`HandlePunishmentTriggered()` — no changes to `UPunishmentArbitrationComponent` or
`UPunishmentManagerComponent` were needed, since gating at the component itself already
satisfies "regardless of arbitration outcome" per the issue's acceptance criteria.

## Files changed

The real Unreal project lives under `app/` (gitignored symlink, D-003) and is what the
harness actually builds/tests against. Per D-009, this PR's diff contains a **copy** of
the new/changed source at `app-source-tracked/<same path under app/Source/>`.

| File (under `app-source-tracked/Source/KrowdKontrol/`) | Action | What it contains |
|------|--------|-------------------|
| `AbilityLockoutComponent.cpp` | UPDATE | Declares `CVarPunishmentLockoutEnabled` (`kk.Punishment.LockoutEnabled`, default 1); `HandlePunishmentTriggered()` returns immediately, no state mutated, if the CVar reads 0 |
| `SpeedReductionPunishmentComponent.cpp` | UPDATE | Declares `CVarPunishmentSpeedReductionEnabled` (`kk.Punishment.SpeedReductionEnabled`, default 1); `HandlePunishmentTriggered()` returns immediately, before even the `MovementComponent` null-check, if the CVar reads 0 |
| `Private/Tests/KrowdKontrolAbilityLockoutComponentTest.cpp` | UPDATE | New case (i): CVar=0 blocks `HandlePunishmentTriggered()` from locking Stun; CVar restored to 1 allows normal activation again |
| `Private/Tests/KrowdKontrolSpeedReductionPunishmentComponentTest.cpp` | UPDATE | New case (f): CVar=0 blocks `HandlePunishmentTriggered()` from reducing `MaxSpeed`; CVar restored to 1 allows normal activation again |

## Deviations from the investigation/plan

Implementation matched the investigation/plan exactly. `app/` and `app-source-tracked/`
were verified byte-identical for all four touched files both before and after this PR's
edits.

## Acceptance criteria

- [x] `kk.Punishment.LockoutEnabled` set to 0 prevents ability-lockout from activating on
      trigger, regardless of arbitration outcome — gated as the first line of
      `AbilityLockoutComponent::HandlePunishmentTriggered()`.
- [x] `kk.Punishment.SpeedReductionEnabled` set to 0 prevents speed-reduction from
      activating on trigger, regardless of arbitration outcome — gated as the first line
      of `SpeedReductionPunishmentComponent::HandlePunishmentTriggered()`.
- [x] Both CVars default to 1 (enabled), preserving current behavior when unset.
- [x] Automation tests: CVar=0 blocks activation, default allows normal activation, for
      both punishments — new case (i) / case (f).
- [x] Level 1-3 validation commands pass with exit 0. See evidence below.
- [x] Code mirrors existing patterns exactly (early-return guard shape, lettered test
      cases, `TAutoConsoleVariable<int32>` per Epic's documented convention).
- [x] No regressions in existing `KrowdKontrol.Unit.*` tests.
- [x] `app/` and `app-source-tracked/` copies of every changed file are identical —
      confirmed via `diff` for all four touched files.

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
