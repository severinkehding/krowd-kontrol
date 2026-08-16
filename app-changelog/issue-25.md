# Issue #25: 2-state calm/combat adaptive music system

Adds `UMusicSubsystem` (a `UTickableWorldSubsystem`) that plays a calm placeholder
track by default and crossfades to a combat placeholder track the instant any spawned
`AEnemyBase` reports itself "Hot" via the existing (previously unimplemented)
`IThreatState` interface, reverting to calm the instant no enemy is Hot.
`AEnemyBase` gets its first real `IThreatState` implementer (`GetThreatState()`,
mapping Alert/Attack/Controlled -> Hot, Idle/Banked -> Idle), closing a gap flagged
since issue #81. Two short, procedurally generated placeholder `.wav` loops (80s
electronic, driving bass character - sparse 90 BPM calm bassline vs. faster 140 BPM
combat bassline) stand in for real music per MISSION.md's placeholder-first rule.

## Files changed

| File | Action | Contains |
|------|--------|----------|
| `EnemyBase.h`/`.cpp` | UPDATE | `AEnemyBase` implements `IThreatState`; `GetThreatState()` mapping; new friend grant for the music-subsystem test. Also carries `FindPlayerEnergyComponent()` and a `FKrowdKontrolBomberEnemyTest` friend grant - **not part of issue #25's own scope** (see "Deviations from plan" below); both are already load-bearing for issue #15's Bomber-enemy work-in-progress, which shares this same file in the live `app/` project outside git. |
| `MusicSubsystem.h`/`.cpp` | CREATE | `EMusicState`, `FOnMusicStateChanged`, `UMusicSubsystem`: tick-driven aggregation over `TActorIterator<AEnemyBase>`, `FadeOut`/`FadeIn` crossfade via a shared `PlayTrackForState()` helper, a one-shot `Initialize()` override that actually starts the default-state track (see "Deviations from plan"), and a one-shot warning if `CalmTrack`/`CombatTrack` never resolve. |
| `Private/Tests/MusicStateTestListener.h`/`.cpp` | CREATE | Test-only dynamic-delegate listener for `OnMusicStateChanged`; also records `GetMusicState()` read from inside the callback (`ObservedStateDuringBroadcast`), to assert the flip-before-broadcast re-entrancy guarantee. |
| `Private/Tests/KrowdKontrolMusicSubsystemTest.cpp` | CREATE | `KrowdKontrol.Unit.MusicSubsystem`, cases (a)-(j): default state, single-enemy full Idle->Alert->Attack->Controlled->Banked cycle, idempotent no-op refresh, two-enemy aggregation, flip-before-broadcast re-entrancy assertion, and a real-track crossfade case asserting `CurrentMusicComponent` actually spawns/replaces a `UAudioComponent`. |
| `Private/Tests/KrowdKontrolEnemyBaseTest.cpp` | UPDATE | Added cases (m)/(n): `FindPlayerEnergyComponent()`'s found and not-found paths - see "Deviations from plan" below. |

Not mirrored here (per Hard Invariant #8's carve-out - `app-source-tracked/` never
holds binary/content assets): `app/Content/_Placeholder/Music/PlaceholderCalmTrack.wav`
and `PlaceholderCombatTrack.wav`, two procedurally generated placeholder tracks that
exist on disk under `app/` but are not yet imported into the Editor as `USoundWave`
assets (see Known Follow-Up below).

## Acceptance criteria

- [x] A calm-state and a combat-state placeholder music track exist in the project:
      `app/Content/_Placeholder/Music/PlaceholderCalmTrack.wav` /
      `PlaceholderCombatTrack.wav`, procedurally generated (no licensing risk).
- [x] `UMusicSubsystem` plays the calm track by default (cleared rooms, hub/menu):
      `Initialize()` (`MusicSubsystem.cpp`) actually starts `CalmTrack` once, on world
      init - fixed from an earlier version that only set `CurrentState = Calm` and
      relied on `SetMusicState()`'s no-op-on-same-state guard, which silently
      swallowed the very first call and never actually started playback. Case (j) in
      `KrowdKontrolMusicSubsystemTest.cpp` now asserts `CurrentMusicComponent`
      actually becomes non-null on a real transition, closing the gap that let this
      ship uncaught the first time.
- [x] The moment any `AEnemyBase` reports `IThreatState::Hot`, playback switches to
      the combat track; reverts to calm when no enemy is Hot:
      `UMusicSubsystem::RefreshMusicState()` / `IsAnyEnemyInCombat()`
      (`MusicSubsystem.cpp`).
- [x] The switch is a `FadeOut`/`FadeIn` crossfade over `CrossfadeDurationSeconds`,
      never an abrupt `Stop()`/instant swap: `SetMusicState()` (`MusicSubsystem.cpp`),
      now actually exercised by case (j)'s real-track assertions (previously every
      test case ran with `CalmTrack`/`CombatTrack` unset, so this path was untested).
- [x] Exactly 2 discrete states (`EMusicState::Calm`/`Combat`) - no 3rd state, no
      stems/layering: `MusicSubsystem.h`.
- [x] `KrowdKontrol.Unit.MusicSubsystem` Automation Framework test passes and
      confirms the state switches correctly on hot/idle transitions:
      `KrowdKontrolMusicSubsystemTest.cpp`, 10 cases (a)-(j).
- [x] Level 1-2 validation commands pass: Editor/UnrealBuildTool rebuild succeeded
      (0 errors), `harness/ci.py` full mode `GATE_OK`.
- [x] No regressions in `KrowdKontrol.Unit.EnemyBase` or `KrowdKontrol.Unit.ThreatState`:
      both still present and passing in the same gate run.
- [x] No Hard Invariant from MISSION.md violated: no kill path touched (`Banked`
      still maps to `Idle`, never destroyed), no 6th gameplay colour introduced,
      ability/enemy rosters untouched, engine/2D lock untouched.
- [ ] Editor-side WAV import (`USoundWave` assets) + `DefaultGame.ini` wiring — see
      Known Follow-Up below. `SetMusicState()` degrades gracefully with unset tracks,
      so this does not block the state-machine/test half of this issue.

## Validation evidence

```
$ python harness/ci.py
HARNESS_START mode=full driver=cli
STATIC_SKIPPED no 'static' command in harness.config.json
UNIT_PASSED tests=29
APP_STARTED driver=cli
UE_AUTOMATION_RESULT passed=1 total=1
UE_AUTOMATION_OK
E2E_PASSED steps=1
HOLDOUT_ABSENT no .factory/holdout/run.py
MUTATIONS_ABSENT no harness/mutations/run.py
GATE_OK mode=full
```

`unit=29` (28 pre-change baseline + 1 `KrowdKontrol.Unit.MusicSubsystem`, now 10
internal cases (a)-(j) instead of the original 9), confirmed passing by name in
`app/Saved/Logs/KrowdKontrol.log`. Re-run after a self-fix review round (see
"Deviations from plan"): `UnrealBuildTool` rebuild via `Build.bat KrowdKontrolEditor
Win64 Development` succeeded with 0 errors after fixing a real compile error the
first review-fix pass introduced (`TestNotNull` can't deduce a template argument from
`TObjectPtr<UAudioComponent>` - fixed by binding to a local raw pointer first) and a
real runtime failure (`NewObject<USoundBase>()` asserts because `USoundBase` is
abstract - fixed by using the concrete `USoundWave` subclass instead, per the
Automation log's own handled-ensure stack trace). Both `harness/ci.py --quick` and
full mode are green on the current source; `KrowdKontrol.Unit.` (all 29 tests) also
verified directly via `harness/run_ue_automation.sh`.

## Known follow-up (not blocking)

The two placeholder `.wav` files exist on disk at `app/Content/_Placeholder/Music/`
but were not imported into the Unreal Editor as `USoundWave` assets this session -
the session's `unreal-mcp` client connection never came up even though the Editor
itself was confirmed live (see `implementation.md`'s Deviations section for the full
account). `DefaultGame.ini` was deliberately left unmodified rather than pointing
`CalmTrack`/`CombatTrack` at asset paths that don't exist yet. `SetMusicState()`
already degrades gracefully with unset tracks (proven by every Automation test case,
none of which configure a track), so the state machine itself is fully correct and
testable without this step. Fast follow-up: import both WAVs as `USoundWave` under
`Content/_Placeholder/Music/`, then add
`[/Script/KrowdKontrol.MusicSubsystem]` / `CalmTrack=...` / `CombatTrack=...` to
`app/Config/DefaultGame.ini`.

## Deviations from plan

None on Tasks 1-6 as originally planned/implemented. Task 7's Editor-import half is
incomplete per the Known Follow-Up above; the plan's own GOTCHA for that task
explicitly anticipated and permitted this split.

**Self-fix review round** (same issue, after an independent multi-agent review):

- **Fixed - playback-start bug (CRITICAL)**: `CurrentState` member-initializes to
  `Calm`, and `SetMusicState()`'s `NewState == CurrentState` no-op guard silently
  swallowed the very first `RefreshMusicState()` call - so the calm track never
  actually started playing on world/game load, only the state *flag* was correct.
  Fixed with a one-shot `Initialize(FSubsystemCollectionBase&)` override (mirrors
  `RoomEnemyBudgetController::InitializeRoom()`'s `bHasInitializedRoom` pattern) that
  bypasses the guard once via a new shared `PlayTrackForState()` helper (also used by
  `SetMusicState()`, so the two callers can't drift out of sync).
- **Fixed - untested crossfade path (CRITICAL)**: every original test case ran with
  `CalmTrack`/`CombatTrack` unset, so `SpawnSound2D`/`FadeIn`/`FadeOut` - this issue's
  own headline acceptance criterion - was never actually exercised. Added case (j),
  which configures real (`NewObject<USoundWave>()`, no `.uasset` needed) tracks and
  asserts `CurrentMusicComponent` spawns and is replaced (not reused) across a
  Calm->Combat->Calm round trip.
- **Fixed - silent misconfiguration (MEDIUM)**: `SetMusicState()`/`PlayTrackForState()`
  now logs one `Warning` (via `bHasWarnedMissingTrack`) the first time a track fails
  to resolve, rather than staying silent forever - matches
  `RoomEnemyBudgetController::EnemyClassToSpawn`'s precedent for this exact class of
  problem. Still silent-safe during the current, intentional pre-Task-7 window.
- **Fixed - untested re-entrancy guarantee (MEDIUM)**: `UMusicStateTestListener` now
  optionally records `GetMusicState()` read from inside its own callback
  (`ObservedStateDuringBroadcast`); case (c) asserts it already reflects the new
  state, locking down the "flip before broadcast" ordering the code comment already
  claimed but nothing previously verified.
- **Fixed - misleading/incomplete comments (HIGH/MEDIUM)**: `MusicSubsystem.h`'s
  `FKrowdKontrolMusicSubsystemTest` friend-grant comment claimed friendship is
  transitive across `AEnemyBase`/`UMusicSubsystem` (it isn't) - rewritten to describe
  what the grant is actually for (`CurrentMusicComponent` access, now used by case
  (j)). `EnemyBase.h`'s friend-grant comment didn't account for
  `FKrowdKontrolMusicSubsystemTest` being a cross-subsystem consumer test, not a
  concrete-subclass test like the others - extended to cover both cases.
- **Investigated, then kept as-is (not removed) - `FindPlayerEnergyComponent()` /
  `FKrowdKontrolBomberEnemyTest`**: the independent review correctly flagged these as
  absent from this issue's own plan/description/acceptance-criteria (they're issue
  #15/Bomber-enemy prep, referenced by their own doc comment) and recommended
  splitting them out. Attempting that split during the self-fix pass found that
  `BomberEnemy.cpp` - already present in the live `app/` project outside git, from
  issue #15's own in-flight work sharing this same file - calls
  `FindPlayerEnergyComponent()` directly and needs the `FKrowdKontrolBomberEnemyTest`
  friend grant to compile its own test. Removing either from `EnemyBase.h`/`.cpp`
  would have broken that unrelated, already-real feature the moment this branch's
  `app/` state was shared/merged - a materially worse outcome than the scope-mixing
  the review flagged. Kept in place; this row (and the "Files changed" table above)
  now discloses the bundling explicitly, which is what the review's own fallback
  option (keep + document) asked for once a clean split isn't safe.
- **Fixed - `FindPlayerEnergyComponent()` had no test coverage anywhere (HIGH)**:
  since the method is staying in this PR's tracked diff (previous bullet), added
  cases (m)/(n) to `KrowdKontrolEnemyBaseTest.cpp` (already a friend of `AEnemyBase`,
  no new friend grant needed) covering both the found-component and
  not-found/warning paths, using `FAutomationEditorCommonUtils::CreateNewMap()` +
  `NewObject<UPlayerEnergyComponent>(PlayerPawn)` + `RegisterComponent()`.
