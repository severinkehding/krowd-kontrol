# Issue #25: 2-state calm/combat adaptive music system

Adds `UMusicSubsystem` (a `UTickableWorldSubsystem`) that plays a calm placeholder
track by default and crossfades to a combat placeholder track the instant any spawned
`AEnemyBase` reports itself "Hot" via the existing (previously unimplemented)
`IThreatState` interface, reverting to calm the instant no enemy is Hot.
`AEnemyBase` gets its first real `IThreatState` implementer (`GetThreatState()`,
mapping Alert/Attack/Controlled -> Hot, Idle/Banked -> Idle), closing a gap flagged
since issue #81. Two real, operator-supplied electronic tracks (Pixabay-licensed, 80s-electronic
driving-bass character per PRD 12) are imported as `USoundWave` assets:
`/Game/Audio/Music/CalmTrack` (96s, stylish-deep-electronic) and
`/Game/Audio/Music/CombatTrack` (126s, driving electronic). These replaced an
earlier attempt's procedurally generated `.wav` placeholders, which were written to
disk but never imported as Editor assets - the root cause of pass-2's critical E2E
finding (every Play immediately Stopped: a missing/unresolvable asset ends playback
instantly no matter how correct the code is). With real imported assets, live PIE
verification shows sustained streaming playback (decoder chunk advancement 8s+ into
the track).

## Files changed

| File | Action | Contains |
|------|--------|----------|
| `EnemyBase.h`/`.cpp` | UPDATE | `AEnemyBase` implements `IThreatState`; `GetThreatState()` mapping; new friend grant for the music-subsystem test. Also carries `FindPlayerEnergyComponent()` and a `FKrowdKontrolBomberEnemyTest` friend grant - **not part of issue #25's own scope** (see "Deviations from plan" below); both are already load-bearing for issue #15's Bomber-enemy work-in-progress, which shares this same file in the live `app/` project outside git. |
| `MusicSubsystem.h`/`.cpp` | CREATE | `EMusicState`, `FOnMusicStateChanged`, `UMusicSubsystem`: tick-driven aggregation over `TActorIterator<AEnemyBase>`, `FadeOut`/`FadeIn` crossfade via a shared `PlayTrackForState()` helper, a one-shot `Initialize()` override that actually starts the default-state track (see "Deviations from plan"), and a one-shot warning if `CalmTrack`/`CombatTrack` never resolve. |
| `Private/Tests/MusicStateTestListener.h`/`.cpp` | CREATE | Test-only dynamic-delegate listener for `OnMusicStateChanged`; also records `GetMusicState()` read from inside the callback (`ObservedStateDuringBroadcast`), to assert the flip-before-broadcast re-entrancy guarantee. |
| `Private/Tests/KrowdKontrolMusicSubsystemTest.cpp` | CREATE | `KrowdKontrol.Unit.MusicSubsystem`, cases (a)-(j): default state, single-enemy full Idle->Alert->Attack->Controlled->Banked cycle, idempotent no-op refresh, two-enemy aggregation, flip-before-broadcast re-entrancy assertion, and a real-track crossfade case asserting `CurrentMusicComponent` actually spawns/replaces a `UAudioComponent`. |
| `Private/Tests/KrowdKontrolEnemyBaseTest.cpp` | UPDATE | Added cases (m)/(n): `FindPlayerEnergyComponent()`'s found and not-found paths - see "Deviations from plan" below. |

Not mirrored here (per Hard Invariant #8's carve-out - `app-source-tracked/` never
holds binary/content assets): `app/Content/Audio/Music/CalmTrack.uasset` and
`CombatTrack.uasset`, imported via the `ImportAssets` commandlet from operator-supplied
tracks, and `app/Config/DefaultGame.ini`'s `[/Script/KrowdKontrol.MusicSubsystem]`
section pointing `CalmTrack`/`CombatTrack` at them.

## Acceptance criteria

- [x] A calm-state and a combat-state music track exist in the project as imported
      `USoundWave` assets: `/Game/Audio/Music/CalmTrack` / `CombatTrack`
      (operator-supplied, Pixabay-licensed - no licensing risk).
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
- [x] Editor-side WAV import (`USoundWave` assets) + `DefaultGame.ini` wiring — the
      WAV import turned out to already be done (see "Fix round" below for the
      evidence); `DefaultGame.ini` wiring landed in that same fix round. See "Fix
      round: pass-1 validation feedback" below for what changed and the honest
      caveat on how far this was re-verified this session.

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

## Known follow-up: RESOLVED (2026-08-17, operator-assisted)

An earlier version of this section documented that the placeholder `.wav` files were
never imported as `USoundWave` assets (the session's MCP connection was down), which
turned out to be the root cause of pass-2's critical E2E finding - a missing asset
ends playback the instant it starts, indistinguishable in LogAudio from a code bug.
Resolved by importing two real operator-supplied tracks via the `ImportAssets`
commandlet (`/Game/Audio/Music/CalmTrack`, 96s; `/Game/Audio/Music/CombatTrack`,
126s) and pointing `DefaultGame.ini`'s `[/Script/KrowdKontrol.MusicSubsystem]`
section at them. Live PIE verification with `LogAudio` at Verbose: sustained
streaming playback confirmed (decoder chunk advancement 8s+ into CalmTrack), where
the placeholder-era behavior was an instant Play/Stop pair.

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

## Fix round: pass-1 validation feedback (2026-08-17)

Addresses `dark-factory-validate-pr` pass-1's `request_changes` verdict (workflow
`68f0f516428af1026f66f69930ddda56`).

### Critical - CalmTrack/CombatTrack resolved to null (behavioral)

`app/Content/_Placeholder/Music/PlaceholderCalmTrack.wav` and
`PlaceholderCombatTrack.wav` turn out to have already been imported into the Editor
as `USoundWave` assets in a prior session - `app/Saved/Logs/KrowdKontrol.log` shows
`Performing atomic reimport of .../PlaceholderCalmTrack.wav` and
`Saving Package: /Game/_Placeholder/Music/PlaceholderCalmTrack` (both timestamped
2026.08.16-20.10.03), and the resulting `.uasset` files on disk are confirmed real
`SoundWave` packages by binary inspection (`class SoundWave`, correct package path).
What was still actually missing was the `DefaultGame.ini` wiring itself. Added to
`app/Config/DefaultGame.ini` (gitignored - not visible in this repo's own diff, per
CLAUDE.md's Environment section; the value format is the standard UE ini
soft-object-path string for a `TSoftObjectPtr`):

```ini
[/Script/KrowdKontrol.MusicSubsystem]
CalmTrack=/Game/_Placeholder/Music/PlaceholderCalmTrack.PlaceholderCalmTrack
CombatTrack=/Game/_Placeholder/Music/PlaceholderCombatTrack.PlaceholderCombatTrack
```

`harness/ci.py` full mode is `GATE_OK` after this change (29/29 unit tests,
`KrowdKontrol.Smoke.` e2e still passing). **Caveat, stated honestly**: neither the
Automation unit-test suite (which injects its own `NewObject<USoundWave>()` test
tracks in case (j), not the Config-driven ones) nor `harness/e2e.py` (Smoke-suite
only, per `harness/README.md`) actually exercises `PlayTrackForState()`'s
`CalmTrack.LoadSynchronous()` / `CombatTrack.LoadSynchronous()` path. This session had
no live `unreal-mcp` connection (Editor not running, MCP server not started - see
CLAUDE.md's WSL2/mirrored-networking section), so the live-PIE inspection pass-1 used
to originally catch `CalmTrack=None` could not be re-run to positively confirm the
fix end-to-end. The config values are correct against the real, verified asset paths,
but this is an inference from static evidence, not a re-run behavioral confirmation -
pass-2 should re-check with a live PIE session if one is available.

### High - E2E "no APawn with a UPlayerEnergyComponent" (e2e)

Investigated by code inspection (no live Editor session available this round, so this
is a static-analysis finding, not a re-run PIE confirmation). `AEnemyBase::GetThreatState()`
- the method issue #25's music switch actually reads via `IThreatState` - derives
`Hot`/`Idle` purely from `CurrentState` (`Alert`/`Attack`/`Controlled` map to `Hot`;
`Idle`/`Banked` map to `Idle`), and `CurrentState` only ever advances via
`TickCheckDetection(PlayerLocation)`, called every `Tick()` with
`UGameplayStatics::GetPlayerPawn(GetWorld(), 0)`'s location - a plain distance check
against `DetectionRangeUnits`/`GetAttackRangeUnits()`. None of that path touches
`FindPlayerEnergyComponent()` or `UPlayerEnergyComponent` at all.
`AFlatCamera3DPrototypePawn` (the pawn in `L_FlatCamera3DPrototype`, the map pass-1's
E2E session used) self-possesses via `AutoPossessPlayer = EAutoReceiveInput::Player0`
in its own constructor, so `GetPlayerPawn()` should resolve to it without needing a
`GameMode`/`DefaultPawnClass` - the Hot-state trigger path looks reachable on
inspection, independent of the missing-audio-asset issue above.

The `FindPlayerEnergyComponent` warning pass-1 actually saw comes from a different
code path: `ABomberEnemy`'s contact-damage handling (`BomberEnemy.cpp`, issue #15's
own work, not part of this PR's tracked diff - see the scope note below) calls
`FindPlayerEnergyComponent()` when applying damage on overlap, and
`AFlatCamera3DPrototypePawn` has no `UPlayerEnergyComponent` attached, so that call
always warns-and-returns-null today regardless of issue #25. Forcing "player-enemy
overlap" during E2E testing exercises that Bomber-specific overlap/damage path, not
the proximity-based `TickCheckDetection` path issue #25's music switch depends on -
the two are unrelated once traced through the code, even though both hang off
`AEnemyBase`/its subclass. No source edit made for this item: it's an investigated
finding pointing at a pre-existing, already-disclosed scope-mixing issue (next
section), not a bug in code this PR owns.

### Medium - split FindPlayerEnergyComponent()/friend grant out (scope)

Re-investigated; same conclusion the original implementation already reached (see
"Deviations from plan" above), now with the structural reason confirmed directly:
`BomberEnemy.cpp` is issue #15's own work, open as PR #119
(`archon/task-fix-issue-15`, not merged to `main`) - but `app/` is a single physical,
gitignored Unreal project shared across every in-flight branch (CLAUDE.md's
Environment section), not a per-branch checkout. `BomberEnemy.cpp` calls
`FindPlayerEnergyComponent()` directly in that shared `app/` tree today (confirmed via
grep against the live file). Removing the method/friend grant from
`EnemyBase.h`/`.cpp` in this PR would edit the one shared physical file both PRs'
source lives in and break PR #119's build the moment anyone builds against current
`app/` state - a cross-PR regression, not a scope fix. Left in place, unchanged from
the original implementation; this PR's tracked diff already discloses the bundling in
the "Files changed" table above, which is the documented fallback this exact
situation calls for.

### Low - stale PR description

PR #120's top-level description synced via `gh pr edit` to match this file's already-
accurate case count (10 cases (a)-(j), not 9) and fix-round history (this file's
"Deviations from plan" / "Self-fix review round" sections above), plus this fix
round's findings.
