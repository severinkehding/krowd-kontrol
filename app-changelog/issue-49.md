# Issue #49: Add URoomMetadataComponent

Adds `URoomMetadataComponent`, a data-only `UActorComponent` attachable to an existing
`ARoomActor`, that lets a designer hand-author four pieces of per-room metadata in the
level/Blueprint editor: an enemy-type budget (count per each of the 4 locked enemy
types), a target-zone count/type summary, a difficulty tier, and an optional "requires
ability" gate (`None` or one of the 5 locked abilities). This is pure metadata storage
for a future room-pool shuffler (PRD 05 REQ-4) — no shuffling/sequencing logic is
built here. `EnemyTypeBudget`/`TargetZoneCounts` use `TArray<FRoomEnemyTypeCount>`,
not `TMap<EEnemyType, int32>`, to avoid two confirmed, still-open Unreal Editor bugs
(UE-39260, UE-219729) affecting struct-valued/enum-keyed `TMap` UPROPERTYs in the
Details panel — mirrors `ARoomActor::FRoomTargetZone`'s existing enum-tag-plus-payload
pattern instead.

## Files changed

All paths are under `app/` (gitignored per D-003) — this table and the matching
`app-source-tracked/` copy are the tracked-repo record of that change; see the
closing note below.

| File | Action | What it contains |
|------|--------|-------------------|
| `app/Source/KrowdKontrol/RoomAbilityGate.h` | CREATE | New `ERoomAbilityGate` enum (`None`, `Stun`, `Sleep`, `Root`, `Fear`, `Snare`) — own file, since `EAbilitySlot` has no `None` value and its `Count` sentinel sizes 3 other components |
| `app/Source/KrowdKontrol/RoomDifficultyTier.h` | CREATE | New `ERoomDifficultyTier` enum (`Easy`, `Medium`, `Hard`) — no prior difficulty-tier concept exists in the codebase |
| `app/Source/KrowdKontrol/RoomMetadataComponent.h` | CREATE | `FRoomEnemyTypeCount` struct + `URoomMetadataComponent` class declaration (`EnemyTypeBudget`, `TargetZoneCounts`, `DifficultyTier`, `RequiredAbility`, all `EditInstanceOnly, BlueprintReadWrite`) |
| `app/Source/KrowdKontrol/RoomMetadataComponent.cpp` | CREATE | Non-ticking constructor; pre-populates `EnemyTypeBudget`/`TargetZoneCounts` with one zeroed entry per locked enemy type |
| `app/Source/KrowdKontrol/Private/Tests/KrowdKontrolRoomMetadataComponentTest.cpp` | CREATE | `KrowdKontrol.Unit.RoomMetadataComponent` — attaches the component to a real `ARoomActor` and round-trips all 4 fields |

## Acceptance criteria

- [x] **`URoomMetadataComponent` attaches to `ARoomActor` and exposes editable
      metadata.** All 4 fields (`EnemyTypeBudget`, `TargetZoneCounts`,
      `DifficultyTier`, `RequiredAbility`) are `EditInstanceOnly, BlueprintReadWrite`,
      editable per-placed-instance in the Details panel.
- [x] **`KrowdKontrol.Unit.RoomMetadataComponent` confirms attachment and a full field
      round-trip.** Test spawns a real `ARoomActor`, attaches the component, checks
      default-state completeness, sets every field, and reads each back — including
      `RequiredAbility == None` as an explicitly tested value, not just the default.
- [x] **No shuffling/sequencing logic added.** The component only stores data; no
      code here reads it to sequence or select rooms.

## Validation

```
$ python harness/ci.py --quick
HARNESS_START mode=quick driver=cli
STATIC_SKIPPED no 'static' command in harness.config.json
UNIT_PASSED tests=43
GATE_OK mode=quick
```

---

The real Unreal project stays under `app/` (gitignored, D-003) — this changelog and
its matching `app-source-tracked/` copy are the tracked-repo record of that change,
per D-009. Not a substitute for reading `app-source-tracked/` directly.
