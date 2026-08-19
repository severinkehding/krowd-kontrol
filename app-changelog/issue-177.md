# Issue #177: Add punishment manager foundation with contact-damage trigger signal

Adds `UPunishmentManagerComponent`, a new `UActorComponent` exposing a no-payload
`OnPunishmentTriggered` dynamic multicast delegate that fires whenever the player pawn
takes real contact damage. It's wired as a sibling of `UPlayerEnergyComponent` in both
prototype pawns' constructors, listening to `UPlayerEnergyComponent::OnEnergyChanged`
and re-broadcasting a punishment-domain signal. This is PRD "Punishment System
(Punishments 1 & 2 + arbitration)" REQ-1 — pure trigger plumbing only. No punishment
effect (lockout, speed-reduction, arbitration) is implemented here; those are separate,
later issues that will bind their own listeners to `OnPunishmentTriggered`.

## Files changed

The real Unreal project lives under `app/` (gitignored symlink, D-003) and is what the
harness actually builds/tests against — `app/` itself is unchanged by this PR's
tracking. What this PR's diff actually contains is a **copy** of the new/changed
source, per D-009, at `app-source-tracked/<same path under app/Source/>`.

| File (under `app-source-tracked/Source/KrowdKontrol/`) | Action | What it contains |
|------|--------|-------------------|
| `PunishmentManagerComponent.h` | CREATE | `FOnPunishmentTriggered` delegate decl (no payload) + `UPunishmentManagerComponent : public UActorComponent` class decl, `OnPunishmentTriggered` (`BlueprintAssignable`), `HandleEnergyChanged(float)` |
| `PunishmentManagerComponent.cpp` | CREATE | Constructor (tick disabled) + `HandleEnergyChanged` re-broadcasting `OnPunishmentTriggered` |
| `FlatCamera3DPrototypePawn.h` / `.cpp` | UPDATE | Forward-declares and constructs `PunishmentManagerComponent`, binds it to `PlayerEnergyComponent->OnEnergyChanged` via `AddDynamic` |
| `Paper2DPrototypePawn.h` / `.cpp` | UPDATE | Same wiring as above, for the second prototype pawn |
| `PlayerEnergyComponent.h` | UPDATE | Adds `friend class FKrowdKontrolPunishmentManagerComponentTest;` to the existing test-seeding friend list (precedented, 3 prior grants already present) — does not add a new public mutator; `ApplyContactDamage` remains the sole path to change `CurrentEnergy` |
| `Private/Tests/PunishmentTriggeredTestListener.h` / `.cpp` | CREATE | Minimal `UObject` test listener (`CallCount`), mirrors `UBomberExplodedTestListener` |
| `Private/Tests/KrowdKontrolPunishmentManagerComponentTest.cpp` | CREATE | `KrowdKontrol.Unit.PunishmentManagerComponent` — exactly-once firing, repeat-firing independence, and the no-op (already-zero-energy) edge case |

## Acceptance criteria

- [x] **`UPunishmentManagerComponent` exists, is a `UActorComponent`, and exposes
      `OnPunishmentTriggered` (`UPROPERTY(BlueprintAssignable)`).** See
      `PunishmentManagerComponent.h`.
- [x] **Both `AFlatCamera3DPrototypePawn` and `APaper2DPrototypePawn` construct a
      `PunishmentManagerComponent` and bind it to their own
      `PlayerEnergyComponent->OnEnergyChanged`.** Verified in both pawns' `.cpp` diffs.
- [x] **No punishment effect is applied anywhere in this change.** `OnPunishmentTriggered`
      has exactly the test listener bound; nothing in `AbilityCastComponent.cpp` or pawn
      movement code references it.
- [x] **`KrowdKontrol.Unit.PunishmentManagerComponent` automation test exists and
      passes**, covering exactly-once firing per damage event, independent repeat
      firing, and the no-energy-change no-op case.
- [x] **Level 1-2 validation passes.** `harness/ci.py` full mode: `UNIT_PASSED tests=65`,
      `UE_BUILD_OK`, `UE_AUTOMATION_RESULT passed=1 total=1`, `GATE_OK`.
- [x] **Code mirrors existing patterns** — `FOnBomberExploded`-style no-payload
      delegate, `OvercrowdDetectionComponent`-style component shape, constructor-time
      `AddDynamic` wiring idiom identical to `AbilityCastVFXComponent`'s.
- [x] **No regressions** in the existing automation suite.

## Validation evidence

`python harness/ci.py` (full mode, real headless Unreal Editor rebuild + Automation
Framework run):

```
HARNESS_START mode=full driver=cli
STATIC_SKIPPED no 'static' command in harness.config.json
UNIT_PASSED tests=65
APP_STARTED driver=cli
UE_BUILD_START KrowdKontrolEditor Win64 Development
UE_BUILD_OK
UE_AUTOMATION_RESULT passed=1 total=1
UE_AUTOMATION_OK
E2E_PASSED steps=1
HOLDOUT_ABSENT no .factory/holdout/run.py
MUTATIONS_ABSENT no harness/mutations/run.py
GATE_OK mode=full
```

Green on first run — no fixes needed. Hard invariants (MISSION.md #1-#8) reviewed
directly against the diffs, all intact; see `validation.md` for the full breakdown.
