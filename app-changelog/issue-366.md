# Issue #366 — Banking-radius ring honesty regression test

**Revised after code review (2026-08-30).** The original approach (a standalone
`KrowdKontrolBankingRadiusIndicatorHonestyTest.cpp` calling
`ShowBankingRadiusIndicator()` directly) was found to be tautological: it read
`ATargetZone::GetBankingRadiusUnits()` once to build the call and again to build the
expected value, with nothing in between that could diverge, so it never exercised the
actual production call site (`ARoomActor::EnsureBankingZonesWired()` /
`RoomActor.cpp:443`) where a hardcoded-constant regression would really occur. It also
substantially duplicated `KrowdKontrolTargetZoneTest.cpp` case (g) and
`KrowdKontrolPlaceholderTargetZoneActorTest.cpp`'s existing coverage. That file has
been deleted.

The fix instead extends the existing `KrowdKontrolRoomActorBankingWiringTest.cpp`,
which already drives two markers through the real `BeginPlay() ->
EnsureBankingZonesWired()` path but previously left both zones at the same default
150.0f radius (a coincidental match that a hardcoded literal could also produce).
Before the test's existing repeated `EnsureBankingZonesWired()` call (already present
to prove idempotency), the second zone's box extent is resized to 300x300 — its
`ExistingZone` branch (`RoomActor.cpp:488-492`) unconditionally re-derives the ring
radius from the zone's live extent on every call, so this is the one path that can
actually distinguish "ring genuinely derived from the zone" from "ring hardcoded to a
constant that happens to match." Two new assertions follow: the resized zone's ring
tracks its new 300.0f extent, and the two markers' ring radii differ.

## Acceptance criteria

- [x] Regression coverage proves the ground-ring's rendered radius is genuinely
      derived from `ATargetZone::GetBankingRadiusUnits()`, not a hardcoded constant —
      now via `KrowdKontrol.Unit.RoomActorBankingWiring`, driven through the real
      `ARoomActor::EnsureBankingZonesWired()` production call site
      (`RoomActor.cpp:443`), rather than a standalone test that bypassed it
- [x] Exercises two differently-configured zone instances (150.0f default, 300.0f
      resized) and asserts an explicit inequality between their ring radii
- [x] No duplication of `KrowdKontrolTargetZoneTest.cpp` case (g) or
      `KrowdKontrolPlaceholderTargetZoneActorTest.cpp`'s existing coverage — the
      standalone file that duplicated them has been removed
- [x] `harness/run_ue_automation.sh KrowdKontrol.Unit.RoomActorBankingWiring` passes
- [x] Full unit rung (`harness/run_ue_automation.sh KrowdKontrol.Unit.`) passes with no
      regressions
- [x] No production code (`TargetZone.h/.cpp`, `PlaceholderTargetZoneActor.h/.cpp`,
      `RoomActor.h/.cpp`) modified

## Validation evidence

See PR #393's harness run for `KrowdKontrol.Unit.RoomActorBankingWiring` and the full
`KrowdKontrol.Unit.` rung, re-run after this revision.

No protected files touched. Diff scope after this revision:
`app-source-tracked/Source/KrowdKontrol/Private/Tests/KrowdKontrolRoomActorBankingWiringTest.cpp`
(mirror of the identical file under `app/`) modified; the standalone
`KrowdKontrolBankingRadiusIndicatorHonestyTest.cpp` file (and its `app/` counterpart)
deleted.
