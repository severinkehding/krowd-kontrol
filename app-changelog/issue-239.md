# Issue #239: PIE scenario test — serialized placed-actor health across both shipped maps

Adds two new `KrowdKontrol.PIE.` group tests
(`app/Source/KrowdKontrol/Private/Tests/KrowdKontrolPIESerializedPlacedActorHealthTest.cpp`),
the second scenario test required by `docs/prd-functional-pie-tests.md` REQ-3 item 2.
Each opens one shipped map (`L_Level01`, `L_Level02`) in a real in-editor PIE session
via `AutomationOpenMap`, and for every placed `APlaceholderTargetZoneActor` beacon
marker found via `TActorIterator` asserts: the corrected component hierarchy issue
`#199`'s self-heal produces (`TargetZoneRootComponent` as root; mesh/column siblings
under it; light atop the column), a non-origin world location, and that the
self-healed, attached `ATargetZone` (`ARoomActor::EnsureBankingZonesWired()`, run for
real via `ARoomActor::BeginPlay()` during the PIE tick) spawned at that same marker
position. No lifecycle method (`PostInitializeComponents`, `EnsureBeaconHierarchy()`,
`EnsureBankingZonesWired()`) is ever called directly.

## Deviation from the investigation plan: two tests, not one looping over both maps

The plan specified a single `IMPLEMENT_SIMPLE_AUTOMATION_TEST` looping over both maps
in one `RunTest`, with a documented fallback if that shape didn't work in practice.
It didn't: `AutomationOpenMap`'s `EditorContext` path
(`UEditorEngine::AutomationLoadMap`) calls `FEditorFileUtils::LoadMap()`
**synchronously** on every call, not deferred behind a latent command. Calling it
twice inside one ordinary (non-latent) `for` loop meant the second call's synchronous
`LoadMap(L_Level02)` silently reloaded the editor world out from under the first map
before its own queued `FStartPIEForAutomationCommand` ever ran — confirmed empirically
in the Editor log (`Created PIE world by copying editor world from L_Level02` fired
twice, never `L_Level01`), and every marker assertion failed as a result, a sequencing
artifact rather than a real defect. Splitting into
`KrowdKontrol.PIE.SerializedPlacedActorHealth.L_Level01` and `.L_Level02` — one
`AutomationOpenMap` call per test, matching every other `KrowdKontrol.PIE.*` test's
shape — resolved it: after the split, only the world-origin position assertion still
failed (see below), and it failed identically whichever level ran first, ruling out
any remaining ordering artifact.

## A real regression, filed separately: issue #292

With the sequencing bug fixed, both tests still fail — but now on real level content,
not a test artifact. A subset of placed target-zone markers really do sit at
`(0, 0, 0)` at PIE runtime in both shipped maps (3 in `L_Level01`, 6 in `L_Level02`),
while every other assertion (hierarchy, self-healed `ATargetZone` presence, and the
marker/zone position match — trivially true since both are wrong together) passes for
every marker in both levels. This is exactly the "still-open zones-at-origin class of
bug" issue #239's own problem statement flagged as a known gap no existing gate
caught. Per `FACTORY_RULES.md` §2, the assertion is not weakened to force a pass and
the level content is not silently patched in this PR — the underlying defect is filed
as **issue #292** (coordinated-not-duplicated pattern, same as #234/#238), with the
suspected root cause (`EnsureBeaconHierarchy()`'s `SetRootComponent()` call not
carrying over the old root's world transform) noted there for whoever picks it up.

## Acceptance criteria

- [x] Tests live in the `KrowdKontrol.PIE.` group — see the deviation note above for
      why this is two tests (`KrowdKontrol.PIE.SerializedPlacedActorHealth.L_Level01`
      / `.L_Level02`) rather than the single name the plan specified; both are still
      the `KrowdKontrol.PIE.SerializedPlacedActorHealth*` mechanism the acceptance
      criteria describe, with full map coverage preserved.
- [x] Opens each of the two shipped maps in turn within a real PIE session via
      `AutomationOpenMap` — never `CreateNewMap()`/`LoadMap()`.
- [x] For each map, asserts every placed `APlaceholderTargetZoneActor`'s component
      hierarchy matches `#199`'s self-heal target — confirmed passing for every
      marker in both levels.
- [x] For each map, asserts every placed marker's world location is not the world
      origin, and its attached, self-healed `ATargetZone`'s spawned position matches
      the marker's position within tolerance — the hierarchy/attached-zone half
      passes for every marker; the not-at-origin half fails for real (issue #292),
      exactly the regression this test exists to catch.
- [x] No direct call to `PostInitializeComponents`, `EnsureBeaconHierarchy()`, or
      `EnsureBankingZonesWired()` anywhere in the new test.
- [x] `app/` and `app-source-tracked/` copies are byte-identical (`diff` clean,
      verified after every revision including the two-test split).
- [x] This changelog created, mapping every criterion to a concrete assertion, with
      real validation evidence below.
- [x] No production (non-test) code changed.

## Validation evidence

Editor build + targeted `KrowdKontrol.PIE.` run, executed live in this environment
(Windows-host `UnrealEditor-Cmd.exe` via `harness/run_ue_automation.sh`, not simulated):

```
$ harness/run_ue_automation.sh "KrowdKontrol.PIE."
UE_BUILD_START KrowdKontrolEditor Win64 Development
UE_BUILD_OK
UE_AUTOMATION_RESULT passed=2 total=4
UE_AUTOMATION_FAILED KrowdKontrol.PIE.SerializedPlacedActorHealth.L_Level01: state=Fail
UE_AUTOMATION_FAILED KrowdKontrol.PIE.SerializedPlacedActorHealth.L_Level02: state=Fail
```

The 2 passing tests are the pre-existing `RealSessionStartsAndEndsCleanly` (#236) and
`DefeatRestartRoundTrip` (#240) — both unaffected, confirming this change didn't
regress the existing `KrowdKontrol.PIE.` group. The 2 new tests both compile and run
correctly (see the deviation note above for how that was confirmed) and both fail for
the real reason documented above and tracked as issue #292, not a test defect.

**REQ-2 (issue #237, still open)** means neither new test runs under
`python harness/ci.py`'s automated `unit` rung yet — that filter is literally
`"KrowdKontrol.Unit."` and does not pick up `KrowdKontrol.PIE.*`. The targeted filter
above is the real validation command for this issue, matching #236/#240's own
precedent.

---

The real Unreal project stays under `app/` (gitignored, D-003) — this changelog and
its matching `app-source-tracked/` copy are the tracked-repo record of that change,
per D-009. Not a substitute for reading `app-source-tracked/` directly.
