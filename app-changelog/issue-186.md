# Issue #186: Add reusable baseline lighting rig and apply it to L_Level01 and L_Level02

**Type**: feature

## Summary

Adds `ALevelLightingRigActor` — a reusable actor combining a `UDirectionalLightComponent`
and a `USkyLightComponent` tuned to a dim-but-readable, non-reserved grey-blue colour
(`(0.55, 0.6, 0.68)`), intended to be dropped into any level that needs baseline lighting.
Placement into `L_Level01.umap`/`L_Level02.umap` itself is **not done** — it requires a live
Unreal Editor/MCP session, which was unavailable in both the implementation and validation
sessions for this run. See "What's not done" below.

## Acceptance criteria

- [x] `ALevelLightingRigActor` exists (DirectionalLight + SkyLight), dim-but-readable,
      colour is not one of the five reserved gameplay colours and not pure white
      (Hard Invariant 3) — verified in `validation.md` Phase 3
- [x] Isolated unit test `KrowdKontrol.Unit.LevelLightingRigActorHasDimReadableLighting`
      confirms wiring and colour lock — passes in isolation (`passed=1 total=1`)
- [x] `Level01Test.cpp` / `Level02Test.cpp` updated with a rig-placement assertion
      (`TActorIterator<ALevelLightingRigActor>` count == 1)
- [ ] Rig actually placed in `L_Level01.umap` and `L_Level02.umap` — **BLOCKED**,
      environment issue (no live Editor/MCP tools in-session), not a code defect.
      `KrowdKontrol.Unit.Level01Structure` / `...Level02Structure` fail as designed
      (`LightingRigs.Num() == 0`, expected `1`) until this is done.
- [x] No regression in any other test — 66/68 passing, both failures accounted for above
- [x] No reserved-colour violation (Hard Invariant 3 holds)

## What's not done

Placing one `ALevelLightingRigActor` instance into each of `L_Level01.umap` and
`L_Level02.umap` requires a live Unreal Editor with a working MCP connection. Neither the
implement nor the validate session for this run had one (`mcp__unreal-mcp__*` tools never
registered in-session; direct probe of `127.0.0.1:8000/mcp` was unreachable during
validation). Per project precedent (issue #56/PR #99), no placeholder/fabricated map edit
was made. This PR is labeled `factory:needs-human` rather than `factory:needs-review` — a
human (or a future session with confirmed live `mcp__unreal-mcp__*` access) needs to open
the project, place the actor in both maps, save, and re-run `python harness/ci.py`. No
further source changes are expected to be needed at that point.

## Validation evidence

```
$ python3 harness/ci.py
HARNESS_START mode=full driver=cli
STATIC_SKIPPED no 'static' command in harness.config.json
UE_BUILD_START KrowdKontrolEditor Win64 Development
UE_BUILD_OK
UE_AUTOMATION_RESULT passed=66 total=68
UE_AUTOMATION_FAILED KrowdKontrol.Unit.Level01Structure: state=Fail
UE_AUTOMATION_FAILED KrowdKontrol.Unit.Level02Structure: state=Fail
GATE_FAILED: unit
```

Both failures are the intentional red-then-green assertions added by this PR's own
Task 4/5, pending Task 6 (map placement) — not a regression. Full detail in
`implementation.md` and `validation.md` for this run
(`artifacts/runs/7520520df66690fc77cfdeca2087c082/`).

## Files

| File | Action |
|------|--------|
| `app/Source/KrowdKontrol/LevelLightingRigActor.h` | CREATE |
| `app/Source/KrowdKontrol/LevelLightingRigActor.cpp` | CREATE |
| `app/Source/KrowdKontrol/Private/Tests/KrowdKontrolLevelLightingRigActorTest.cpp` | CREATE |
| `app/Source/KrowdKontrol/Private/Tests/KrowdKontrolLevel01Test.cpp` | UPDATE |
| `app/Source/KrowdKontrol/Private/Tests/KrowdKontrolLevel02Test.cpp` | UPDATE |
