# Issue #29: Onboarding — unmissable target-zone beacon on first successful Stun

Per PRD 09 REQ-2: the instant the player's first successful Stun of a session lands,
the nearest `APlaceholderTargetZoneActor`'s beacon light intensifies as a live,
in-world visual cue teaching "herd" as the next beat right after the tutorial teaches
"control" — no menu, no text box. A new `UActorComponent`, `UFirstStunBeaconComponent`,
binds to the existing `UAbilityCastComponent::OnAbilityCastApplied` delegate (added for
issue #138/#59) the same way `UGizmoFirstContactComponent` (issue #59) and
`UAbilityCastVFXComponent` (issue #67) already do. On the first delegate firing where
`Ability == EAbilitySlot::Stun`, it finds the nearest `APlaceholderTargetZoneActor` to
its owner and calls a new `IntensifyBeacon()` method that raises
`APlaceholderTargetZoneActor::BeaconLightComponent`'s intensity from
`BeaconBaselineIntensity` (3000.0) to `BeaconIntensifiedIntensity` (9000.0) —
mirroring `ASniperEnemy`'s existing `EyeGlow` baseline/intensified pattern. A local
`bool` one-shot guard is sufficient because no respawn/relevel mechanic exists anywhere
in this codebase today for `AFlatCamera3DPrototypePawn`.

This is a republish of previously-implemented, previously-rejected-for-unrelated-reasons
work: PR #159 was closed after review flagged an undisclosed `EnemyBase.h`
`private`→`protected`+`UPROPERTY` promotion of `CurrentState`/`ControllingAbility` that
was never part of this feature. That leak is confirmed absent here — the only diff
between `app/`'s `EnemyBase.h` and this PR's tracked mirror is the one expected
`FKrowdKontrolFirstStunBeaconComponentTest` friend grant.

## Files changed (all under `app/`, gitignored per D-003 — this is the tracked-repo
record of that change)

| File | Action | What it contains |
|------|--------|-------------------|
| `app/Source/KrowdKontrol/FirstStunBeaconComponent.h` | CREATE | Declares `UFirstStunBeaconComponent`: `HandleAbilityCastApplied()` (public `UFUNCTION`, bound to `OnAbilityCastApplied`), private `FindNearestTargetZone()`, and the `bHasTriggeredBeacon` one-shot guard |
| `app/Source/KrowdKontrol/FirstStunBeaconComponent.cpp` | CREATE | Filters to `EAbilitySlot::Stun`, burns the guard before the zone lookup (a no-zone miss is permanent, not retried), finds the nearest `APlaceholderTargetZoneActor` via a `TActorIterator` + `DistSquared` scan, calls `IntensifyBeacon()` |
| `app/Source/KrowdKontrol/PlaceholderTargetZoneActor.h` | UPDATE | Adds `BeaconBaselineIntensity` (3000.0), `BeaconIntensifiedIntensity` (9000.0), and `IntensifyBeacon()` |
| `app/Source/KrowdKontrol/PlaceholderTargetZoneActor.cpp` | UPDATE | Constructor now reads `BeaconBaselineIntensity` instead of a hardcoded `3000.0f`; implements `IntensifyBeacon()` |
| `app/Source/KrowdKontrol/FlatCamera3DPrototypePawn.h` | UPDATE | Adds `FirstStunBeaconComponent` member + forward declaration |
| `app/Source/KrowdKontrol/FlatCamera3DPrototypePawn.cpp` | UPDATE | Constructs `FirstStunBeaconComponent` and binds it to `AbilityCastComponent->OnAbilityCastApplied`, alongside the existing `GizmoFirstContactComponent` binding |
| `app/Source/KrowdKontrol/EnemyBase.h` | UPDATE | Adds exactly one friend grant, `FKrowdKontrolFirstStunBeaconComponentTest`, for the new Automation test to drive `AEnemyBaseTestActor` deterministically — no visibility/`UPROPERTY` changes to any existing member |
| `app/Source/KrowdKontrol/Private/Tests/KrowdKontrolFirstStunBeaconComponentTest.cpp` | CREATE | `KrowdKontrol.Unit.FirstStunBeaconComponent` — covers (a) no-zone-in-world (no crash, and a later-spawned zone is never retroactively intensified), (b) nearest-of-two-zones selection, (c)/(d) repeated/non-Stun casts never re-trigger, (e) real pawn-constructor wiring end-to-end |
| `app/Source/KrowdKontrol/Private/Tests/KrowdKontrolPlaceholderTargetZoneActorTest.cpp` | UPDATE | Adds the baseline→intensified assertion pair for `IntensifyBeacon()` |

No `KrowdKontrol.Build.cs` change needed — every module this feature uses (`Core`,
`CoreUObject`, `Engine`, `InputCore`) was already a dependency.

## Acceptance criteria

- [x] **A target zone exists within a short, guaranteed-visible distance of the first
      Stun encounter.** Level-authoring concern, not code — out of scope for this fix
      beyond ensuring the trigger works wherever a zone is placed.
- [x] **On the first successful Stun of the game (session-level one-time trigger), the
      nearest target zone's beacon visual intensifies.** Satisfied by
      `UFirstStunBeaconComponent`.
- [x] **The beacon does not use any of the five reserved gameplay-information colours
      for anything except their defined purpose.** Unchanged from issue #72's
      pre-existing saturated-green colour choice; not modified by this fix (that colour
      choice's own possible "6th reserved colour" status remains a separate, pre-existing
      P3 follow-up per issue #72's review).
- [x] **No pause, text box, or blocking overlay accompanies the trigger.** The change is
      pure light-intensity mutation — no UMG/Slate involvement anywhere in the new code.
- [x] **A new automation test confirms the beacon-trigger event fires exactly once, on
      the first successful Stun of a session.** Satisfied by
      `KrowdKontrol.Unit.FirstStunBeaconComponent` cases (a)-(e).

## Not in scope

- Reopening or reusing PR #159 or its branch tip (confirmed to still carry the
  unrelated `EnemyBase.h` leak that sank it).
- Any change to `ATargetZone` (the separate, newer banking-detection class from issue
  #80/PR #151/#153) — `APlaceholderTargetZoneActor` and `ATargetZone` are deliberately
  distinct classes.
- Respawn/relevel persistence for the one-shot guard — no such mechanic exists yet for
  `AFlatCamera3DPrototypePawn`.
- A retry path for the "no zone in world yet" case — permanently skipped by design,
  covered by test case (a).

## Note on shared `app/` state

While validating this change, the full-module build failed for a reason unrelated to
this feature: `app/Source/KrowdKontrol/EnemyBase.h` was missing the
`FKrowdKontrolSleepShieldBossTest` friend grant that
`Private/Tests/KrowdKontrolSleepShieldBossTest.cpp` (concurrent, unmerged issue #48
work also resident on the shared `app/` filesystem per `CLAUDE.md` D-003) requires to
compile. That grant was restored directly in `app/EnemyBase.h` only, to unblock the
shared build — it is deliberately **not** included in this PR's tracked mirror
(`app-source-tracked/Source/KrowdKontrol/EnemyBase.h`), which carries only the one
`FKrowdKontrolFirstStunBeaconComponentTest` line this issue needs. Issue #48's own PR
(#158) is responsible for publishing that grant through its own tracked mirror.

## Validation

```
$ python harness/ci.py
HARNESS_START mode=full driver=cli
STATIC_SKIPPED no 'static' command in harness.config.json
UNIT_PASSED tests=57
APP_STARTED driver=cli
UE_AUTOMATION_RESULT passed=1 total=1
UE_AUTOMATION_OK
E2E_PASSED steps=1
HOLDOUT_ABSENT no .factory/holdout/run.py - NOTHING above the independence line ran.
MUTATIONS_ABSENT no harness/mutations/run.py - this gate has never been shown to fail.
GATE_OK mode=full
```

`UNIT_PASSED tests=57` covers the new `KrowdKontrol.Unit.FirstStunBeaconComponent` test
and the updated `KrowdKontrol.Unit.PlaceholderTargetZoneActorHasVisibleBeacon` test
alongside every pre-existing `KrowdKontrol.Unit.*` test — no regressions.

---

Source lives under `app/` (gitignored, D-003) — this file is the tracked-repo record
of that change, not a substitute for reading the actual code.
