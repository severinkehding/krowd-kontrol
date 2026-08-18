# Issue #29: Unmissable target-zone beacon on first successful Stun

Adds a new `UFirstStunBeaconComponent` (`app/Source/KrowdKontrol/FirstStunBeaconComponent.h/.cpp`),
attached to `AFlatCamera3DPrototypePawn` alongside `UGizmoFirstContactComponent` and
bound to `UAbilityCastComponent::OnAbilityCastApplied` the same way. On the first
successful Stun cast of a session, it finds the nearest `APlaceholderTargetZoneActor`
to the pawn and calls a new public `IntensifyBeacon()` method on it, which raises
`BeaconLightComponent`'s intensity from `BeaconBaselineIntensity` (3000.0f, unchanged)
to `BeaconIntensifiedIntensity` (9000.0f) — mirroring `ASniperEnemy`'s
`EyeGlowBaselineIntensity`/`EyeGlowIntensifiedIntensity` pattern. A local
`bool bHasTriggeredBeacon` guard ensures the beacon fires exactly once per session; no
GameInstance subsystem is needed since no respawn/relevel mechanic exists for the
player pawn today (PRD 09 REQ-2).

Also grants `FKrowdKontrolFirstStunBeaconComponentTest` friend access to
`AEnemyBase::TickCheckDetection` (`app/Source/KrowdKontrol/EnemyBase.h`) — a required
deviation from the plan (which did not list `EnemyBase.h` as a file to change) needed
to drive a test enemy through Idle->Alert deterministically, matching every other
`KrowdKontrol.Unit.*` test's own friend-grant precedent for this same method.

## Acceptance criteria

- [x] `APlaceholderTargetZoneActor::IntensifyBeacon()` raises the beacon's intensity
      from `BeaconBaselineIntensity` to `BeaconIntensifiedIntensity`, mirroring
      `ASniperEnemy`'s baseline/intensified pattern.
- [x] `UFirstStunBeaconComponent` fires exactly once, on the first successful Stun
      cast of a session, and never again — even across further successful Stun casts.
- [x] On trigger, the *nearest* `APlaceholderTargetZoneActor` to the player pawn is
      the one intensified (proven with 2+ zones at different distances in the test).
- [x] No reserved gameplay-information colour (Purple/Teal/Orange/Blue/White) is
      introduced or touched anywhere in this change — no colour-setting line was
      added or modified.
- [x] No pause, text box, widget, or blocking overlay is introduced — no UMG/Slate/
      HUD code is touched.
- [x] `KrowdKontrol.Unit.FirstStunBeaconComponent` exists, compiles, and passes,
      confirming the exactly-once-per-session firing behavior.
- [x] No regressions in `KrowdKontrol.Unit.PlaceholderTargetZoneActorHasVisibleBeacon`
      or any other existing `KrowdKontrol.Unit.*` test.
- [x] `app/` and `app-source-tracked/` copies of every changed/new file are identical
      (verified via `diff`).
- [x] `app-changelog/issue-29.md` created, following the `issue-83.md` convention.

## Validation evidence

Full gate (`python harness/ci.py`, mode=full):

```
HARNESS_START mode=full driver=cli
STATIC_SKIPPED no 'static' command in harness.config.json
UNIT_PASSED tests=57
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

Pre-change baseline was `UNIT_PASSED tests=56` (`app-changelog/issue-83.md` reported
50, with #151/#153/#156 landing since); the new `KrowdKontrol.Unit.FirstStunBeaconComponent`
test brings the count to 57, a net +1 with no regressions. Targeted filter runs during
implementation additionally confirmed `KrowdKontrol.Unit.FirstStunBeaconComponent`
(`passed=1 total=1`) and the extended `KrowdKontrol.Unit.PlaceholderTargetZoneActorHasVisibleBeacon`
(`passed=1 total=1`). One incidental `KrowdKontrol.Unit.EnemyBase` failure was observed
during one full-suite run ("Enemy actor should not be destroyed by reaching Banked");
it passed in isolation and on every other full-suite run, and this change never
modifies `EnemyBase.cpp` or anything on the Banked/GC path it exercises — a
pre-existing GC-timing flake, not a regression from this change.

MISSION.md Hard Invariants reviewed against this diff: no kill-rule, ability-roster,
enemy-roster, engine/dimensionality, networking, or `app`-tracking invariant is
touched. Hard Invariant 3 (five reserved gameplay-information colours) is satisfied by
construction — this change only ever sets light *intensity*, never colour, on
`APlaceholderTargetZoneActor`'s pre-existing green beacon (whose own placeholder-colour
caveat from issue #72's review remains untouched and unresolved by this issue).

---

The real Unreal project stays under `app/` (gitignored, D-003) — this changelog and
its matching `app-source-tracked/` copy are the tracked-repo record of that change,
per D-009. Not a substitute for reading `app-source-tracked/` directly.
