# Issue #211: Complete the bank-delivery chain

Wires the three previously-independent, additive gaps that made PRD 01 loop step 5
("Bank") unreachable in any real playthrough:

1. `AEnemyBase` now implements `IHerdable` (`IsControlled()` reads
   `CurrentState == Controlled`; `GetHerdColourTag()` reads
   `AbilityData::Get(ControllingAbility).ColourTag`), so `ATargetZone::HandleZoneOverlap`
   no longer early-outs on every regular enemy.
2. `ARoomActor::EnsureBankingZonesWired()` (called from the new `BeginPlay()` override,
   idempotent, public for test/Blueprint callers) self-heals a colour-tagged
   `ATargetZone` attached to each already-placed `TargetZones` marker in
   `L_Level01`/`L_Level02`, resolving each marker's `EnemyType` to its countering
   ability's `ColourTag` via `AbilityData::GetAll()`.
3. `ARoomActor::HandleZoneActorBanked()` subscribes to each spawned zone's
   `OnActorBanked` and calls `AEnemyBase::TransitionToBanked()` on the banked actor -
   deliberately not `URoomEnemyBudgetController::NotifyEnemyBanked()`, which stays
   separate, out-of-scope future work per that class's own comments.

`ReservedGameplayColours`/`AbilityData` gained the `FName` tag vocabulary
(`GetPurpleTag()` etc., `FAbilityData::ColourTag`) both (1) and (2) need, centralizing
text previously only "proven" by hardcoded literals duplicated across test files.

## Deviation from the investigation/plan artifact

The plan assumed `IHerdable` + a placed `ATargetZone` + the delegate wiring above was
sufficient. It was not: empirically (via a real `harness/run_ue_automation.sh` run,
not guesswork), a controlled `ATrooperEnemy` swept into a real `ATargetZone` never
banked. Diagnostic logging added and removed during implementation showed why: every
concrete enemy's mesh root defaults to the engine's `BlockAllDynamic` profile (Block
response to the `WorldDynamic` channel), and `ATargetZone::ZoneCollisionComponent` is
also `WorldDynamic` (`OverlapAllDynamic`). Unreal always resolves a Block-vs-Overlap
pair between two components as a blocking collision, never an overlap event,
regardless of which side blocks - so no regular enemy could ever have physically
triggered a zone's `OnActorBanked`, in this fix or any future one, without also
addressing this.

Fixed with one additional line in `AEnemyBase::BeginPlay()` (a function already in
this issue's file scope): `RootPrimitive->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap)`.
Scoped to just the `WorldDynamic` channel - the root's Block response to `WorldStatic`
(the room floor) is untouched - and applied once, generically, at the `AEnemyBase`
level rather than duplicated in every concrete subclass's own constructor (`TrooperEnemy.cpp`,
`SniperEnemy.cpp`, `BomberEnemy.cpp`, `RunnerEnemy.cpp` were **not** touched). Confirmed
safe via `grep -rn "OnComponentHit\|NotifyHit\|OnActorHit"` across the whole module:
zero results - no other system in this codebase reads a Hit/Block event off an enemy's
root component, so nothing depends on an enemy's root actually blocking (vs
overlapping) a `WorldDynamic` object today.

This also explains why the pre-existing `KrowdKontrolDualZoneBossTest.cpp` never
caught this: it broadcasts `OnActorBanked` directly (`ZoneA->OnActorBanked.Broadcast(DummyActor)`),
never via a real physical overlap - the only prior test that did drive a real overlap
(`KrowdKontrolTargetZoneTest.cpp`) used a test-only fixture (`ATargetZoneTestActor`)
built with an explicit `OverlapAllDynamic` collision component, not a production
`AEnemyBase` subclass.

## Acceptance criteria

- [x] `AEnemyBase` implements `IHerdable` (`Cast<IHerdable>` succeeds on every
      subclass; `IsControlled()`/`GetHerdColourTag()` report correctly against the
      real state machine).
- [x] Every already-placed marker in `ARoomActor::TargetZones` gets a colour-tagged
      `ATargetZone` attached, self-healed via `EnsureBankingZonesWired()`
      (idempotent - a second call never double-spawns).
- [x] A controlled, correctly-CC'd regular enemy physically overlapping its room's
      banking zone reaches `EEnemyState::Banked` end-to-end (proven via a real
      physics overlap, not a bypassed test hook) - `KrowdKontrol.Unit.RoomActorBankingWiring`.
- [x] `ADualZoneBoss`'s existing boss encounter is unaffected -
      `ABossBase` is not a subclass of `AEnemyBase`, so `HandleZoneActorBanked`'s
      `Cast<AEnemyBase>` can never match a boss actor; re-ran
      `KrowdKontrol.Unit.DualZoneBoss` (unmodified) alongside the new tests with no
      regression.
- [x] `URoomEnemyBudgetController::NotifyEnemyBanked()` wiring left untouched -
      explicitly out of scope per that class's own comments.
- [x] `app/` and `app-source-tracked/` copies of every changed/new file are
      identical (verified via `diff`, re-confirmed at PR-creation time).
- [x] `python harness/ci.py --quick` / full `harness/ci.py` report `GATE_OK` with the
      unit test count incremented by 2 versus the pre-change baseline
      (`UNIT_PASSED tests=80`, was 78).

## Files changed

| File | Action | Description |
|------|--------|-------------|
| `ReservedGameplayColours.h`/`.cpp` | UPDATE | Added 5 `FName` tag accessors (`GetPurpleTag()` etc.). |
| `AbilityData.h`/`.cpp` | UPDATE | Added `FAbilityData::ColourTag`; populated in all 5 static entries. |
| `EnemyBase.h`/`.cpp` | UPDATE | `AEnemyBase` implements `IHerdable`; added `IsControlled()`/`GetHerdColourTag()`; `BeginPlay()` now also fixes the enemy root's `WorldDynamic` collision response (see Deviation above); 2 new friend-class grants for the new tests. |
| `RoomActor.h`/`.cpp` | UPDATE | Added `EnsureBankingZonesWired()`, `BeginPlay()` override, `HandleZoneActorBanked()`. |
| `Private/Tests/KrowdKontrolEnemyBaseHerdableTest.cpp` | CREATE | `KrowdKontrol.Unit.EnemyBaseHerdable` - confirms `IsControlled()`/`GetHerdColourTag()` against a real `AEnemyBaseTestActor` state machine. |
| `Private/Tests/KrowdKontrolRoomActorBankingWiringTest.cpp` | CREATE | `KrowdKontrol.Unit.RoomActorBankingWiring` - end-to-end marker->zone self-heal, idempotency, and a real physical-overlap bank. |

## Validation evidence

Full gate (`python harness/ci.py`, mode=full):

```
HARNESS_START mode=full driver=cli
STATIC_SKIPPED no 'static' command in harness.config.json
UNIT_PASSED tests=80
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

Targeted runs during implementation additionally confirmed
`KrowdKontrol.Unit.EnemyBaseHerdable` and `KrowdKontrol.Unit.RoomActorBankingWiring`
each pass in isolation, and regression re-runs of `KrowdKontrol.Unit.Herdable`,
`KrowdKontrol.Unit.TargetZone`, `KrowdKontrol.Unit.EnemyBase`, and
`KrowdKontrol.Unit.DualZoneBoss` show no change in behavior.

MISSION.md Hard Invariants reviewed against this diff: no kill-rule violation (Banked
remains the only terminal state - `TransitionToBanked()` itself is unchanged), no
colour-lock violation (all 5 `ColourTag` values route through the existing
`ReservedGameplayColours` accessors, never a new literal), no ability-roster or
enemy-roster change, no engine/dimensionality/networking/`app`-tracking invariant
touched.

## Manual verification (not performed this session)

Per the plan's Validation section: open `L_Level01`/`L_Level02` in the Unreal Editor,
enter PIE, cast each enemy type's correct CC ability, herd it into its room's
target-zone marker, and confirm a save file with a recorded clear time appears. Not
performed as part of this automated implementation pass - the acceptance evidence
above is unit-test-level, per this issue's explicit "code-only, no live level-editing"
scope boundary.

---

The real Unreal project stays under `app/` (gitignored, D-003) - this changelog and
its matching `app-source-tracked/` copy are the tracked-repo record of that change,
per D-009. Not a substitute for reading `app-source-tracked/` directly.
