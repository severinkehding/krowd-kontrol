# Issue #234: OnLevelClear never fires in real PIE play despite all clear conditions met

In a real PIE session on `L_Level01`, the operator drove the full control→herd→bank
chain and verified all six enemies reached `EEnemyState::Banked`, but
`ULevelLifecycleSubsystem::OnLevelClear` never broadcast and
`Saved/SaveGames/KrowdKontrol_LevelClearTimes.sav` was never produced. Every
automation test for this subsystem drives `OnWorldBeginPlay()`/
`RefreshLevelClearState()` by direct call, so the real per-frame `Tick()` →
`RefreshLevelClearState()` path and the real engine-driven `OnWorldBeginPlay()` call
have never been exercised by any gate — this is a "live-fire gap," not a logic bug in
the clear-condition check itself.

The investigation (primary-source read of UE 5.8's `UWorld::BeginPlay()`) weakens but
does not fully rule out `OnWorldBeginPlay()` never firing (Epic's own UE-186247
ordering issue is a genuine, unresolved open gap for PIE's literal startup map).
It surfaces one concrete, project-specific, previously-untested lead for the other
named suspect (`Tick()` never running): nothing in this codebase sets
`bTickEvenWhenPaused`/overrides `IsTickableWhenPaused()`, and this codebase's only
pause source (`UBriefingCardWidget::ShowBriefing()`) has never been exercised in its
real, non-no-op form by any test — three existing test files already document
`SetGamePaused()` as a documented no-op in `CreateNewMap()`-based Automation test
worlds.

Root cause is **not conclusively established** (matching the issue's own
elimination-table conclusion). This change ships the requested loud logging at every
step of the chain plus one low-risk, well-justified hardening fix
(`IsTickableWhenPaused() = true` on `ULevelLifecycleSubsystem`) rather than claiming
false certainty. If the pause-stall theory is the actual cause, this PR closes the
issue outright; if not, the logging makes the real cause visible in the very next
live PIE session instead of requiring a fifth investigation cycle.

## Files changed

All `.h`/`.cpp` files below were written identically to `app/Source/KrowdKontrol/...`
(the real project, gitignored per D-003) and mirrored into `app-source-tracked/` per
D-009, so this is a plain-text copy for review — `app/` itself is unchanged in kind.

| File | Action | What it contains |
|------|--------|-------------------|
| `app/Source/KrowdKontrol/LevelLifecycleSubsystem.h` | UPDATE | `IsTickableWhenPaused()` override returning `true`; new `bHasLoggedFirstTick` one-shot guard field |
| `app/Source/KrowdKontrol/LevelLifecycleSubsystem.cpp` | UPDATE | Entry/broadcast logging in `OnWorldBeginPlay`, one-shot first-`Tick()` logging, pre-broadcast logging in `RefreshLevelClearState` |
| `app/Source/KrowdKontrol/LevelClearTimeSubsystem.cpp` | UPDATE | Entry logging in `HandleLevelBegin`/`HandleLevelClear` (previously completely silent) |
| `app/Source/KrowdKontrol/BriefingCardWidget.cpp` | UPDATE | Logging around both `SetGamePaused()` calls in `ShowBriefing()`/`DismissBriefing()` |
| `app/Source/KrowdKontrol/Private/Tests/KrowdKontrolLevelLifecycleSubsystemTest.cpp` | UPDATE | Tripwire assertion that `IsTickableWhenPaused()` returns `true` (direct-mechanism check — `SetGamePaused()` itself is a no-op in `CreateNewMap()` worlds, so an end-to-end pause simulation isn't possible here) |

## Acceptance criteria

- [x] Loud, unmistakable logging at `OnWorldBeginPlay` fire and `OnLevelClear`
      broadcast, plus the closely-related handler/pause logging needed to make the
      next live session's log capture fully diagnostic in one pass.
- [x] One low-risk, narrowly-justified fix (`IsTickableWhenPaused() = true`)
      targeting the issue's own named suspect #1, with test coverage for the
      mechanism.
- [ ] **Confirm the real PIE clear→save chain now works end-to-end.** Requires a
      live PIE session; this worktree has no network path to the Unreal MCP server
      (structural gap, not something to route around here — see investigation.md's
      Manual Verification section for the exact log-line sequence a human/holdout
      run should check for).

## Validation

`python harness/ci.py --quick` → `GATE_OK` (see `implementation.md`). Full validation
deferred to the `dark-factory-validate` node.

## Notes

No deviations from the investigation's plan — all six implementation steps applied
exactly as specified, plus the mirror copy this file documents. This is a
long-standing, three-times-skipped acceptance gap (per #212's merge decision
comment), not a regression — the live-fire path has never been proven to work in any
prior PR, because no test or holdout tool has ever been able to exercise it. Per the
investigation's explicit scope boundaries, `IsTickableWhenPaused()` was **not**
touched on `UMusicSubsystem`, `UOvercrowdVisualEffectSubsystem`, or
`UOvercrowdAudioSubsystem` (same theoretical exposure, not named by this issue, each
with its own gameplay-relevance tradeoff — flagged as a possible follow-up if this
fix turns out to be the actual root cause), and `UBriefingCardWidget`'s pause/dismiss
behavior itself was not changed, only logged.

The full implementation record for this issue lives in
`/home/severin/.archon/workspaces/severinkehding/krowd-kontrol/artifacts/runs/9e3d7a10a44fe321a3487a58255c0367/implementation.md`.
`app/` itself (the gitignored symlink to the real Unreal project, CLAUDE.md's
Environment section / `.factory/decisions.md` D-003) is unchanged in kind by this
tracked copy — the files above under `app-source-tracked/` are a plain-text mirror
made at PR-creation time (D-009) so GitHub has a non-empty diff to open a PR against
and reviewers have real source to check, not a description of it.
