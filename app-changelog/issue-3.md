# Issue #3: Track per-level clear time and persist a personal best

Adds the tracking/persistence layer for PRD 06 REQ-2 (Replayability & Meta-Progression):
a timer that starts when a level begins and stops when it's cleared, a per-level (not
global) personal-best record that only updates on a faster clear, and local,
session-surviving persistence via Unreal's `USaveGame` API. Introduces
`ULevelClearTimeSubsystem` (`UGameInstanceSubsystem`) plus `ULevelClearTimeSaveGame`
(`USaveGame`) — no save/load system existed anywhere in this codebase before this
issue. Display of the value (post-run summary screen) is explicitly out of scope —
that's a separate, already-identified follow-up issue that depends on this one, as is
wiring `StartLevelTimer`/`StopLevelTimerAndRecordClear` into a real level-begin/clear
event (no such event exists yet; PRD 05's level-progression system doesn't expose one).

## Files changed

The real Unreal project lives under `app/` (gitignored symlink, D-003) and is what
the harness actually builds/tests against — `app/` itself is unchanged by this PR's
tracking. What this PR's diff actually contains is a **copy** of the new source, per
D-009, at `app-source-tracked/<same path under app/Source/>`.

| File (under `app-source-tracked/Source/KrowdKontrol/`) | Action | What it contains |
|------|--------|-------------------|
| `LevelClearTimeSaveGame.h` | CREATE | `ULevelClearTimeSaveGame : public USaveGame` — header-only data container, `TMap<FName, float> BestClearTimesByLevel` keyed per level, not global |
| `LevelClearTimeSubsystem.h` | CREATE | `ULevelClearTimeSubsystem : public UGameInstanceSubsystem` declaration — `StartLevelTimer(FName)`, `StopLevelTimerAndRecordClear(FName) -> float`, `RecordClearTime(FName, float) -> bool`, `GetBestClearTimeSeconds(FName, float&) const -> bool`, static `SaveSlotName` |
| `LevelClearTimeSubsystem.cpp` | CREATE | Implementation: wall-clock timer bookkeeping (`FPlatformTime::Seconds()`), best-time comparison, persistence via `UGameplayStatics::SaveGameToSlot`/`LoadGameFromSlot`/`DoesSaveGameExist`/`CreateSaveGameObject`. Unknown-timer stop logs a warning and no-ops rather than crashing or recording a bogus zero-second clear. |
| `Private/Tests/KrowdKontrolLevelClearTimeSubsystemTest.cpp` | CREATE | `KrowdKontrol.Unit.LevelClearTimeSubsystem` — covers all acceptance criteria below |
| `KrowdKontrol.Build.cs` | NONE | `Engine` (owner of `Kismet/GameplayStatics.h`, `GameFramework/SaveGame.h`, `Subsystems/GameInstanceSubsystem.h`) already a `PublicDependencyModuleNames` entry — verified, not edited |

## Acceptance criteria

- [x] **A timer starts when a level begins and stops when the level is cleared, as a
      callable API.** `StartLevelTimer(FName LevelID)` / `StopLevelTimerAndRecordClear(FName LevelID)`
      implement the start/stop/elapsed-time logic itself. **Wiring these calls to a real
      level-begin/level-clear event is explicitly out of scope for this issue** — no such
      event exists anywhere in this codebase yet (PRD 05's level-progression system isn't
      built; the same "foundation, no live wiring yet" gap already documented on
      `UGizmoNarrativeSubsystem` (issue #57), `AbilityUnlockComponent`, and
      `OvercrowdDetectionComponent`). A follow-up issue, filed once PRD 05 lands a real
      level-begin/clear signal, will call these from that event.
- [x] **On clear, elapsed time is compared against the level's stored best; if faster
      (or no best exists), it's saved as the new personal best.** `RecordClearTime` —
      unit tested for "first clear becomes best" (a), "slower clear doesn't overwrite"
      (b), and "faster clear overwrites" (c) exactly as specified in the issue.
- [x] **Personal-best data persists across play sessions, keyed per level, not
      global.** `ULevelClearTimeSaveGame` via `UGameplayStatics::SaveGameToSlot`/
      `LoadGameFromSlot` — unit tested via a second, independently-constructed
      subsystem instance reading back the same on-disk slot.
- [x] **A `KrowdKontrol.Unit.*` Automation Framework test confirms (a)/(b)/(c).**
      `KrowdKontrolLevelClearTimeSubsystemTest.cpp`, also covers per-level keying,
      cross-session persistence, and the unstarted-timer no-op-with-warning case.
- [x] **No leaderboard, networked storage, or online infrastructure.** Persistence is
      a single local `USaveGame` slot; no networking API touched anywhere in this
      change (MISSION.md Hard Invariant 7).

## Validation

From `validation.md` (`dark-factory-validate`, status `ALL_PASS`):

```
$ python harness/ci.py
HARNESS_START mode=full driver=cli
STATIC_SKIPPED no 'static' command in harness.config.json
UNIT_PASSED tests=52
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

`UNIT_PASSED tests=52` is the full `KrowdKontrol.Unit.*` sweep, which includes the new
`KrowdKontrol.Unit.LevelClearTimeSubsystem` test — confirmed separately by running it
in isolation:

```
$ KROWD_KONTROL_SKIP_UBT=1 harness/run_ue_automation.sh "KrowdKontrol.Unit.LevelClearTimeSubsystem"
UE_AUTOMATION_RESULT passed=1 total=1
UE_AUTOMATION_OK
```

Hard invariants: reviewed, none of MISSION.md's 8 apply to this diff (no enemy logic,
no gameplay-information colour usage, no ability/enemy roster changes, no
engine/dimensionality change, no networking code).
