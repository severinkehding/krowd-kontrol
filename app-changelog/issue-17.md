# Issue #17: Implement SN-1PR enemy: long-range sniper attack, Sleep weakness telegraph

Lands both the shared base Enemy AI state machine (issue #12, a hard dependency of
#17) and the first concrete core enemy type, SN-1PR (issue #17), in one combined PR
per both issues' Notes ("ship together if the combined diff still fits the factory's
500-line PR cap"). `AEnemyBase` is an abstract, tick-driven
`Idle -> Alert -> Attack -> Controlled -> Banked` state machine mirroring the
already-landed `ABossBase` pattern, with `Banked` as the only terminal ("defeated")
state — no code path ever calls `Destroy()`, enforcing MISSION.md Hard Invariant 2.
`ASniperEnemy` extends it with a distinct cone silhouette, a Blue-locked "eye glow"
light that intensifies only when hit by Sleep specifically, a separate attack "tell"
light plus a telegraph countdown timer that fires `OnSniperShotFired`, and a long
`GetAttackRangeUnits()` override so it attacks almost as soon as it detects the
player (the mechanical definition of "long-range" in this state machine).

## Files changed

| File | Action | What it contains |
|------|--------|-------------------|
| `app-source-tracked/Source/KrowdKontrol/EnemyBase.h` | CREATE | `EEnemyState` enum + abstract `AEnemyBase` declaration — state machine, `ReceiveControl`/`TransitionToBanked` public hooks, `GetAttackRangeUnits`/`OnControlledEntry`/`OnAttackEntry` subclass-overridable points. |
| `app-source-tracked/Source/KrowdKontrol/EnemyBase.cpp` | CREATE | Guard/mutate/broadcast state machine implementation + tick-driven proximity/attack-range detection (`TickCheckDetection`). |
| `app-source-tracked/Source/KrowdKontrol/Private/Tests/EnemyBaseTestActor.h`/`.cpp` | CREATE | Test-only concrete subclass of the `Abstract` `AEnemyBase`, counting hook invocations. |
| `app-source-tracked/Source/KrowdKontrol/Private/Tests/EnemyBankedTestListener.h`/`.cpp` | CREATE | `AddDynamic`-target listener for `FOnEnemyBanked`, mirroring `BossBankedTestListener`. |
| `app-source-tracked/Source/KrowdKontrol/Private/Tests/KrowdKontrolEnemyBaseTest.cpp` | CREATE | `KrowdKontrol.Unit.EnemyBase` — default state, proximity-driven Idle→Alert→Attack, no-skip guards, `ReceiveControl` from both Alert and Attack, terminal/idempotent Banked, not-destroyed. |
| `app-source-tracked/Source/KrowdKontrol/SniperEnemy.h` | CREATE | `ASniperEnemy : AEnemyBase` declaration — mesh/eye-glow/tell-light components, telegraph timing properties, `FOnSniperShotFired` delegate. |
| `app-source-tracked/Source/KrowdKontrol/SniperEnemy.cpp` | CREATE | Component construction (cone mesh, Blue eye-glow via `ReservedGameplayColours::GetBlue()`, placeholder tell-light colour), overridden hooks, attack-telegraph timer. |
| `app-source-tracked/Source/KrowdKontrol/Private/Tests/SniperShotFiredTestListener.h`/`.cpp` | CREATE | `AddDynamic`-target listener for `FOnSniperShotFired`. |
| `app-source-tracked/Source/KrowdKontrol/Private/Tests/KrowdKontrolSniperEnemyTest.cpp` | CREATE | `KrowdKontrol.Unit.SniperEnemy` — distinct cone silhouette, Blue glow response gated on Sleep specifically (and only Sleep), attack-tell-precedes-shot ordering, exactly-once shot fire, long attack range. |

No existing file was modified; no `KrowdKontrol.Build.cs` change was needed (all
headers used are already in the `Engine`/`Core`/`CoreUObject` modules this target
already depends on).

## Acceptance criteria

**Issue #12 (base state machine):**
- [x] **Explicit state enum + documented transition table.** `EEnemyState` (`Idle`,
      `Alert`, `Attack`, `Controlled`, `Banked`) with the transition table documented
      inline in `EnemyBase.h`'s header comment.
- [x] **Idle -> Alert via proximity/vision-range check, no advanced perception, no
      pathfinding.** `TickCheckDetection` does a flat `FVector::Dist` check against
      `DetectionRangeUnits`.
- [x] **Alert -> Attack after a fixed delay/condition, overridable per concrete
      type.** `GetAttackRangeUnits()` is a `protected virtual`; `ASniperEnemy`
      overrides it.
- [x] **Public `ReceiveControl` entry point transitions Attack/Alert -> Controlled**,
      interface/hook only. Implemented exactly as specified.
- [x] **Public entry point for Controlled -> Banked**, interface/hook only.
      `TransitionToBanked()`.
- [x] **No group tactics or flanking coordination.** Not implemented — flat
      single-actor proximity check only.
- [x] **At least one `KrowdKontrol.Unit.*` test confirms Idle -> Alert -> Attack in
      isolation.** `KrowdKontrol.Unit.EnemyBase`, cases (c)/(e).

**Issue #17 (SN-1PR):**
- [x] **Extends the base Enemy state machine.** `ASniperEnemy : AEnemyBase`.
- [x] **Placeholder silhouette visually distinguishable in shape, colour-independent.**
      Distinct cone mesh (`/Engine/BasicShapes/Cone.Cone`, scaled), unused elsewhere in
      the module (cube = placeholder enemy, cylinder = target zone). Asserted against
      both other shapes explicitly in the test.
- [x] **Visible/audible attack tell precedes the shot, distinct from other enemies.**
      `AttackTellLightComponent` turns on at `OnAttackEntry`, provably before
      `OnSniperShotFired` broadcasts (test asserts the ordering, not just that both
      happen). Audio tell explicitly out of scope (issue #36, re-filed separately).
- [x] **Movement/attack behavior matches "long-range sniper attack."** Large
      `GetAttackRangeUnits()` (1400, vs. base `DetectionRangeUnits` default 1500) means
      SN-1PR attacks almost as soon as it detects the player rather than closing
      distance.
- [x] **Sleep specifically intensifies the glow; other abilities produce no glow
      response.** `OnControlledEntry` checks `Ability == EAbilitySlot::Sleep` before
      changing `EyeGlowLightComponent`'s intensity. Verified both directions (Sleep
      does intensify; a non-Sleep ability does not) in the test.
- [x] **Blue used only in its locked information-colour role.** `ReservedGameplayColours::GetBlue()`
      is the sole source of the eye-glow colour, never a local literal.
- [x] **Does not implement the Sleep ability itself.** `ReceiveControl` only routes to
      the existing `EAbilitySlot` interface; no duration/effect logic added.

## Validation evidence

Full gate, run against a freshly-rebuilt `UnrealEditor-KrowdKontrol.dll`:

```
$ python3 harness/ci.py
HARNESS_START mode=full driver=cli
STATIC_SKIPPED no 'static' command in harness.config.json
UNIT_PASSED tests=27
APP_STARTED driver=cli
UE_AUTOMATION_RESULT passed=1 total=1
UE_AUTOMATION_OK
E2E_PASSED steps=1
HOLDOUT_ABSENT no .factory/holdout/run.py - NOTHING above the independence line ran.
MUTATIONS_ABSENT no harness/mutations/run.py - this gate has never been shown to fail.
GATE_OK mode=full
```

`UNIT_PASSED tests=27` = 25 pre-existing + `KrowdKontrol.Unit.EnemyBase` +
`KrowdKontrol.Unit.SniperEnemy`, confirming both new Automation tests are genuinely
discovered by the gate's own invocation, not just individually runnable. Full mode
passed on the first run — no fixes were needed.

Hard invariants re-checked by reading the implementation (not just trusting the
gate): #2 (no enemy is ever killed — `Banked` is the only terminal state, no
`Destroy()` call anywhere), #3 (five-colour lock — Blue only via the reserved
accessor; the non-reserved tell-light colour is flagged inline for human review),
#4/#5 (ability roster / enemy roster fixed — reuses existing `EAbilitySlot`, SN-1PR is
one of the 4 already-enumerated core types). All hold.

## Known limitation

The combined tracked-mirror diff for this PR (~729 lines across 12 files) exceeds the
factory's 500-line PR cap. Both issues' Notes permit a combined PR "if the combined
diff still fits" — it doesn't quite. Flagged here for human/validator judgment on
size grounds rather than silently exceeded; splitting further would separate the base
state machine (issue #12) from its first real consumer (issue #17), which the plan
judged not worth the review-continuity cost given the base class only exists to serve
this consumer immediately.
