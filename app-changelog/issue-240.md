# Issue #240: PIE scenario test — defeat-restart round trip

Adds `KrowdKontrol.PIE.DefeatRestartRoundTrip`
(`app/Source/KrowdKontrol/Private/Tests/KrowdKontrolPIEDefeatRestartTest.cpp`), the
third `KrowdKontrol.PIE.` group test, built on the `KrowdKontrol.PIE.` mechanism issue
#236/PR #276 established. It opens `L_Level01` in a real in-editor PIE session, drives
the possessed pawn's `UPlayerEnergyComponent` to 0 via the existing code-side
`AKrowdKontrolPlayerController::Cheat_ZeroPlayerEnergy()` cheat (never simulated
input), waits for the resulting hard level-reload
(`HandleLevelFailed → RequestLevelRestart → UGameplayStatics::OpenLevel()`) to
complete, then asserts the reloaded world is `L_Level01` — explicitly not the engine's
`OpenWorld` template — with player energy restored to its pre-trigger starting value.
This pins issue #223's fix (PIE map-name-mangling stripping on restart) in the only
environment that bug can reproduce in, since `KrowdKontrol.Unit.LevelRestart` /
`KrowdKontrol.Unit.BossCheckpointRestart` run in `CreateNewMap()` worlds specifically
to avoid the real `OpenLevel()` call. No production code changes — issue #223's fix
already shipped; this issue only adds the regression test.

## Acceptance criteria

- [x] Test lives in the `KrowdKontrol.PIE.` group —
      `IMPLEMENT_SIMPLE_AUTOMATION_TEST` name is
      `"KrowdKontrol.PIE.DefeatRestartRoundTrip"`.
- [x] Starts a real PIE session on `L_Level01` — via
      `AutomationOpenMap(TEXT("/Game/Maps/L_Level01"))`, the same mechanism
      `KrowdKontrolPIESessionTest.cpp` uses.
- [x] Drives player energy to 0 via a code-side deterministic call, not simulated
      input — `Controller->Cheat_ZeroPlayerEnergy()`, called directly inside a latent
      command (`FKrowdKontrolTriggerDefeatRestartCommand`).
- [x] After the restart completes, asserts the reloaded world is `L_Level01` and is
      explicitly not the engine's OpenWorld template —
      `TestEqual(RemovePIEPrefix(GetMapName()), "L_Level01")` +
      `TestFalse(GetMapName().Contains("OpenWorld"))` in
      `FKrowdKontrolAssertDefeatRestartCompletedCommand`.
- [x] Asserts restored player energy matches the expected initial/full value after
      restart — `TestEqual(Energy->GetCurrentEnergy(), *ExpectedFullEnergy)`, where
      `ExpectedFullEnergy` was captured pre-trigger (not hardcoded), so the test stays
      correct if `MaxEnergy` is ever redesigned.
- [x] `app/` and `app-source-tracked/` copies of the new test file are byte-identical
      (verified via `diff`, no output).
- [x] No production code changes — only the new test file plus its tracked mirror and
      this changelog entry.

## Validation evidence

Not building/running the Unreal Editor in this environment — see
`implementation.md` for the harness invocation used and its result.

---

The real Unreal project stays under `app/` (gitignored, D-003) — this changelog and
its matching `app-source-tracked/` copy are the tracked-repo record of that change,
per D-009. Not a substitute for reading `app-source-tracked/` directly.
