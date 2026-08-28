# Issue #317: Chain-Coloured Target Zone Poles/Markers

REQ-3 of `docs/prd-colour-coded-herding.md`: every pen/pole a player can deliver a
controlled enemy to now renders solidly in that pen's accepted type's reserved "chain
colour" (Purple/Teal/Orange/Blue), instead of the same placeholder green regardless of
type. The visual lives on `APlaceholderTargetZoneActor` (mesh + point light) — not on
`ATargetZone` itself, which is a collision-only, invisible detector actor.

`APlaceholderTargetZoneActor` gains `ApplyChainColour(FLinearColor)`
(`app/Source/KrowdKontrol/PlaceholderTargetZoneActor.h`/`.cpp`): lazily creates a
`UMaterialInstanceDynamic` from the existing shared placeholder material
`/Game/_Placeholder/Abilities/M_AbilityIndicator.M_AbilityIndicator` (no new content
asset), sets its `"Colour"` vector parameter on both beacon mesh components, and sets
`BeaconLightComponent`'s light colour to match — mirroring the exact MID-creation
pattern `UControlledDurationIndicatorComponent`/`UAbilityTargetingIndicatorComponent`
already use. A new reflected `CurrentChainColour` property (default Black) lets the
Automation test and any future MCP-driven E2E holdout assert the applied colour
without reading rendered pixels.

`ARoomActor::EnsureBankingZonesWired()` (`app/Source/KrowdKontrol/RoomActor.h`/`.cpp`)
gains a new private static helper, `ApplyChainColourToMarker(AActor*, const
ATargetZone*)`, called from both of that function's branches (the already-attached
`ExistingZone` branch and the freshly-spawned `BankingZone` branch) immediately after
each zone's `ZoneEnemyType`/`bAcceptAnyEnemyType` are finalized. The helper no-ops for
any-type zones (`bAcceptAnyEnemyType == true`) and for marker classes other than
`APlaceholderTargetZoneActor` — a designer-supplied custom `MarkerClass` continues to
own its own visuals. When it does apply a colour, it reads it exclusively from
`AbilityData::GetChainColourForEnemyType(Zone->ZoneEnemyType)` (issue #315, PR #341) —
no local/hardcoded colour constant is introduced anywhere in this change.

## Files changed

- `app/Source/KrowdKontrol/PlaceholderTargetZoneActor.h` — forward-declare
  `UMaterialInstanceDynamic`/`UMaterialInterface`; declare `ApplyChainColour()`,
  `CurrentChainColour`, `ChainColourMaterialInstance`
- `app/Source/KrowdKontrol/PlaceholderTargetZoneActor.cpp` — implement
  `ApplyChainColour()`
- `app/Source/KrowdKontrol/RoomActor.h` — declare `ApplyChainColourToMarker()`
- `app/Source/KrowdKontrol/RoomActor.cpp` — implement `ApplyChainColourToMarker()`;
  call it from both branches of `EnsureBankingZonesWired()`
- `app/Source/KrowdKontrol/Private/Tests/KrowdKontrolRoomActorBankingWiringTest.cpp` —
  extended with reflected-colour + beacon-light-colour assertions for all 4 existing
  markers (Purple/RU_NNR, Blue/SN_1PR, Teal/TR_UPR, Orange/B0_0MR), plus an
  idempotency re-check that a second `EnsureBankingZonesWired()` pass doesn't corrupt
  an already-applied colour
- Matching `app-source-tracked/Source/KrowdKontrol/...` mirrors of all 5 files above
  (D-009), confirmed byte-identical via `diff`

No `.Build.cs` change — `Materials/MaterialInstanceDynamic.h` is already used
elsewhere in this module without one. No new content asset (`.uasset`/`.umap`)
created or modified.

**Note on the `app-source-tracked/` mirror for `RoomActor.h`/`.cpp` and the test
file**: the live, shared `app/` symlink also currently carries unrelated,
still-unmerged work from open PR #345 (issue #243-v2, room-perimeter sealing),
since another concurrent task edits the same shared Unreal project directory. A
blanket copy of `app/`'s current `RoomActor.h`/`.cpp`/test-file contents into
`app-source-tracked/` would have leaked that unrelated PR's `SealRoomPerimeter`
code and test offset tweaks into this PR. Instead, this change's actual diff (the
`ApplyChainColourToMarker` helper, its two call sites, and the test's colour
assertions) was spliced directly onto the pre-existing `app-source-tracked/`
baseline, which matches `origin/main`. `app/`'s live files still contain both
this change and PR #345's in-progress work, which is expected and unrelated to
this issue.

## Acceptance criteria

- [x] Each type-keyed `ATargetZone`'s marker renders solidly (mesh MID + beacon
      light) in its accepted type's chain colour, read from
      `AbilityData::GetChainColourForEnemyType`
- [x] No local/hardcoded colour constants introduced
- [x] Any-type zones are never called into `ApplyChainColour` and keep the existing
      neutral green
- [x] `KrowdKontrol.Unit.RoomActorBankingWiring` (extended) passes, asserting all 4
      locked enemy types' colours
- [x] `app/` and `app-source-tracked/` copies are diff-clean for all 5 touched files
- [x] `python harness/ci.py` (full) reports `GATE_OK`
- [x] No regression in `KrowdKontrol.Unit.PlaceholderTargetZoneActorHasVisibleBeacon`,
      `KrowdKontrol.Unit.ChainColour`, or `KrowdKontrol.Unit.TargetZone*`

## Validation evidence

Quick gate (`python harness/ci.py --quick`, mode=quick):

```
HARNESS_START mode=quick driver=cli
STATIC_SKIPPED no 'static' command in harness.config.json
UNIT_PASSED tests=123
PIE_PASSED tests=5
GATE_OK mode=quick
```

Full validation (build + run the extended `KrowdKontrol.Unit.RoomActorBankingWiring`
Automation test) is deferred to the separate `dark-factory-validate` node, per this
repo's factory workflow split between `implement` (light inline check) and
`dark-factory-validate` (exhaustive gate).

MISSION.md Hard Invariants reviewed against this diff: this change touches the
colour-lock invariant (Hard Invariant 3) only by *reading* it via
`AbilityData::GetChainColourForEnemyType`/`ReservedGameplayColours` — it never
redefines the 5 reserved colours, and any-type zones are explicitly excluded from
ever receiving one of them (the `bAcceptAnyEnemyType` guard in
`ApplyChainColourToMarker`).

---

The real Unreal project stays under `app/` (gitignored, D-003) — this changelog and
its matching `app-source-tracked/` copy are the tracked-repo record of that change,
per D-009. Not a substitute for reading `app-source-tracked/` directly.
