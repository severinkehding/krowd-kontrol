# Issue #330: Save-file persistence for the accumulated Crowd Mastery total

Adds cross-launch persistence for `UCrowdMasteryTotalSubsystem::AccumulatedTotal`
(REQ-4 of `docs/prd-crowd-mastery-persistence.md`), completing this PRD's four
requirements. REQ-1 (accumulation, issue #327), REQ-2 (menu display, issue #328), and
REQ-3 (reset control, issue #329) were already shipped and needed no changes here.

The issue asked whether the existing save-file machinery makes this persistence
"cheap" enough to build, or whether it should be closed as deferred. Investigation
confirmed it is cheap: `ULevelClearTimeSaveGame` (the one shared save-game class,
persisted through `ULevelClearTimeSubsystem::SaveSlotName`) already carries two
unrelated stats (`BestClearTimesByLevel`, `BestCrowdMasteryByLevel`) side by side -
adding one more scalar field (`AccumulatedCrowdMasteryTotal`) is a direct extension of
an established pattern (the same one issue #174 used to add `BestCrowdMasteryByLevel`
to this same class), not new save infrastructure.

`UCrowdMasteryTotalSubsystem` gains a private `LoadOrCreateSaveGame()` /
`PersistAccumulatedTotal()` pair (a same-shaped, independent copy of
`ULevelClearTimeSubsystem`'s own load/save methods, not a cross-class call - keeps
this subsystem's "never calls `GetWorld()`/`GetGameInstance()`" testability rule
intact), a public `LoadPersistedTotal()` called from a new `Initialize()` override at
real `GameInstance` startup, and write-through-to-disk on every mutation
(`DepositRunMastery()`, `ResetAccumulatedTotal()`) so the on-disk value can never
diverge from the in-memory authority.

## Acceptance criteria

- [x] `ULevelClearTimeSaveGame` carries a new `AccumulatedCrowdMasteryTotal` field on
      the same shared save slot as `BestClearTimesByLevel`/`BestCrowdMasteryByLevel`
- [x] `UCrowdMasteryTotalSubsystem::Initialize()` loads the persisted total at real
      `GameInstance` startup via a public, independently-testable `LoadPersistedTotal()`
- [x] `DepositRunMastery()` and `ResetAccumulatedTotal()` write through to the same
      save slot on every call - the total can never diverge from what's on disk
- [x] `UCrowdMasteryTotalSubsystem` remains the sole runtime authority - nothing reads
      `AccumulatedCrowdMasteryTotal` off the save object directly except this
      subsystem's own `LoadPersistedTotal()`/`PersistAccumulatedTotal()`
- [x] `KrowdKontrol.Unit.CrowdMasteryTotalSubsystem` gains a save/reload round-trip
      assertion and passes
- [x] `KrowdKontrol.PIE.LifecycleLiveFire` gains one assertion proving the real
      deposit+persist chain and passes
- [x] `python harness/ci.py --mode full` reaches `GATE_OK`
- [x] `app/` and `app-source-tracked/` copies byte-identical for every touched file
- [x] `app-changelog/issue-330.md` written (this file)

## Validation evidence

`python harness/ci.py --mode full`: `GATE_OK` -
`UNIT_PASSED tests=125`, `PIE_PASSED tests=6`, `UE_AUTOMATION_OK passed=1 total=1`,
`E2E_PASSED steps=1`.

## Deviation from the investigation plan

The plan's Task 5 assumed `KrowdKontrol.PIE.LifecycleLiveFire`'s existing drive loop
already reached a state where a fresh Crowd Mastery deposit assertion would pass
trivially. It did not: that loop calls `AEnemyBase::ReceiveControl()` directly rather
than through a real `UAbilityCastComponent::ApplyAbility()`, so the
`OnAbilityCastApplied` broadcast that would normally trigger
`UCrowdMasterySubsystem::SampleControlledCount()` never fired, leaving
`RunningMaxControlledCount` (and therefore the deposited/persisted total) stuck at 0
for the whole scenario - confirmed by a real gate run failing with `Expected 'The
accumulated Crowd Mastery total should be greater than zero after a real level clear'
to be true`.

Fix: `FKrowdKontrolDriveAllEnemiesToBankedCommand::Update()` now calls
`UCrowdMasterySubsystem::SampleControlledCount()` explicitly, immediately after each
`ReceiveControl()` call - the same public, test-drivable entry point that method's own
doc comment already documents as existing for exactly this purpose ("real production
callers and the Automation Framework test can drive it directly"). No production code
changed for this; only the test's simulation of "an ability was cast" was made
faithful enough for this issue's new assertion to prove something real, rather than
asserting `0 > 0` would trivially never be true regardless of REQ-4's own correctness.

## Scope limits (not built here)

- No new save-game class or save slot - reuses `ULevelClearTimeSaveGame`/
  `ULevelClearTimeSubsystem::SaveSlotName` entirely, per the issue's own "cheap"
  determination
- No caching change to `GetBestClearTimeSeconds`/`GetBestCrowdMasteryCount` on
  `ULevelClearTimeSubsystem` - unrelated to this issue
- No Steam/platform sync layer - explicitly future scope per the PRD
- No migration/versioning logic for the save file schema - a new `int32` field with a
  `= 0` default is forward/backward compatible by Unreal's own serialization behaviour,
  matching how `BestCrowdMasteryByLevel` was added to this same class in issue #174
  without any migration step
- No new standalone `KrowdKontrol.PIE.*` test file - extended the existing
  `LifecycleLiveFire` scenario instead, per `docs/prd-functional-pie-tests.md`'s
  "coordinate, don't duplicate" guidance for a save/load feature that already has a
  driving scenario reaching the exact real event (`OnLevelClear`) it depends on

---

The real Unreal project stays under `app/` (gitignored, D-003) - this changelog and its
matching `app-source-tracked/` copy are the tracked-repo record of that change, per
D-009. Not a substitute for reading `app-source-tracked/` directly.
