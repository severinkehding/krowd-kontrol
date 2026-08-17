# Issue #77: Colourblind-safe shape/icon redundancy on enemy world indicators

Adds `UEnemyTypeIndicatorComponent`, a small reusable `UActorComponent` that shows a
floating world-space text marker (the enemy's own codename, e.g. `SN-1PR`) above an
enemy actor — a non-colour differentiator alongside the existing colour coding locked
by `MISSION.md` Hard Invariant #3. The marker text is derived directly from
`EEnemyType`'s own `UMETA(DisplayName)` values (no second label table to keep in
sync), and its colour (a neutral grey, matching `UAbilityCooldownTrayWidget`'s text
chrome via `FLinearColor(0.85, 0.85, 0.85, 1.0).ToFColor(bSRGB=true)`) is proven
distinct from all 5 reserved gameplay colours by the new test. `EnemyType` is set
once by the owning C++ subclass's constructor and exposed `VisibleAnywhere`/
`BlueprintReadOnly` (not editable per-instance), so a placed actor's marker can't
silently desync from its real type. `ABomberEnemy` (B0-0MR) and `ASniperEnemy`
(SN-1PR) — the only two core enemy types with concrete gameplay actor classes today —
each gain the component, configured to their own `EEnemyType`. RU-NNR and TR-UPR have
no concrete actor class yet (out of scope, tracked separately under the enemies-and-ai
PRD), so the new Automation test proves the type→marker mapping is distinct across all
4 enum values by spawning the component directly on the existing `AEnemyBaseTestActor`
test scaffold for those two, including a dedicated regression case for an owner with
no `RootComponent` (log + return, no crash, retryable rather than permanently
disabled).

Note: `app/`'s live `BomberEnemy.h`/`.cpp` also carry an unrelated `GetMovementSpeedUnitsPerSecond()`
addition from issue #122 (a different, separately-tracked in-flight change sharing
the same underlying files via the shared `app/` symlink). That hunk is deliberately
excluded from this PR's `app-source-tracked/` mirror — only the #77-scoped lines are
mirrored here, to keep this diff to exactly what this issue changed.

## Files changed

| File | Action | What it contains |
|------|--------|-------------------|
| `app-source-tracked/Source/KrowdKontrol/EnemyTypeIndicatorComponent.h` | CREATE | `UEnemyTypeIndicatorComponent` declaration: `EEnemyType EnemyType` property, `GetMarkerText()`, lazy `MarkerTextComponent`, idempotent `InitializeMarkerVisual()`. |
| `app-source-tracked/Source/KrowdKontrol/EnemyTypeIndicatorComponent.cpp` | CREATE | Implementation — derives marker text from `StaticEnum<EEnemyType>()->GetDisplayNameTextByValue()`, lazily creates/attaches a `UTextRenderComponent` in a neutral non-reserved colour on first `InitializeMarkerVisual()` call (from `BeginPlay()` or directly). |
| `app-source-tracked/Source/KrowdKontrol/BomberEnemy.h` | UPDATE | Adds `EnemyTypeIndicatorComponent` member (forward decl + `TObjectPtr` property). Only the #77-scoped hunk is mirrored — the unrelated issue #122 `GetMovementSpeedUnitsPerSecond()` override present in live `app/` is intentionally left out. |
| `app-source-tracked/Source/KrowdKontrol/BomberEnemy.cpp` | UPDATE | Constructs the component as `EEnemyType::B0_0MR` in the constructor. Same #122 exclusion as above. |
| `app-source-tracked/Source/KrowdKontrol/SniperEnemy.h` | UPDATE | Adds `EnemyTypeIndicatorComponent` member, mirrors BomberEnemy.h's shape. |
| `app-source-tracked/Source/KrowdKontrol/SniperEnemy.cpp` | UPDATE | Constructs the component as `EEnemyType::SN_1PR` in the constructor. |
| `app-source-tracked/Source/KrowdKontrol/Private/Tests/KrowdKontrolEnemyTypeIndicatorComponentTest.cpp` | CREATE | `KrowdKontrol.Unit.EnemyTypeIndicatorComponent` — constructor-time state (no World), World-backed attach/registration for all 4 `EEnemyType` values (2 real actors + 2 on `AEnemyBaseTestActor`), pairwise marker-text distinctness, reserved-colour non-collision, idempotency (no duplicate `MarkerTextComponent` on a second `InitializeMarkerVisual()` call), and a dedicated no-`RootComponent` regression (warns, doesn't crash, retryable). |
| `app-source-tracked/Source/KrowdKontrol/Private/Tests/EnemyBaseTestActor.h` / `.cpp` | UPDATE | Adds a `RootComponent` (a plain `USceneComponent`) so this scaffold actor — used to stand in for RU-NNR/TR-UPR above — has a real root independent of any other in-flight change to this shared file. |

## Acceptance criteria

- [x] **Each of the 4 core enemy types has a distinct, non-colour marker.**
      `GetMarkerText()` proven pairwise-distinct for all 4 `EEnemyType` values by the
      new test, even though only 2 (B0-0MR, SN-1PR) have concrete gameplay actors.
- [x] **`ABomberEnemy` and `ASniperEnemy` each carry a live, correctly-configured
      `EnemyTypeIndicatorComponent`** in addition to their existing colour coding.
- [x] **Automation test coverage.** `KrowdKontrol.Unit.EnemyTypeIndicatorComponent`
      covers distinctness, reserved-colour non-collision, attachment, and
      idempotency.
- [x] **Marker colour never collides with a Hard-Invariant-3-reserved colour.**
      The component's actual runtime colour (derived from `FLinearColor(0.85, 0.85,
      0.85, 1.0)`) is checked at runtime against `ReservedGameplayColours::GetAll()`,
      for all 4 enemy types including the two `AEnemyBaseTestActor` stand-ins.
- [ ] **RU-NNR/TR-UPR concrete gameplay actor classes** — explicitly out of scope
      (enemies-and-ai PRD's job), per the issue's own Notes.

## Validation evidence

`harness/ci.py` full mode: `GATE_OK`. 31 unit tests pass (`UNIT_PASSED tests=31`),
including the new `KrowdKontrol.Unit.EnemyTypeIndicatorComponent` (now with the
dedicated no-`RootComponent` regression case), with no regressions in `BomberEnemy`,
`SniperEnemy`, or `ReservedGameplayColours`. `UE_AUTOMATION_OK`, `E2E_PASSED steps=1`.
No protected paths touched. Hard Invariant #3 (5-colour lock) verified both by the
gate and by inspection — see `validation.md` for the full breakdown. Re-run after
self-fix (colour-space fix, `EnemyType` visibility fix, idempotency-flag-ordering
fix, and the new test case) to confirm the gate still passes against the corrected
code, not just the original submission.
