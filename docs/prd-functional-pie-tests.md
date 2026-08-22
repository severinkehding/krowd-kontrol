# PRD: Functional PIE Tests (exercising the engine paths the unit suite bypasses)

**Author**: operator (Severin), drafted with the interactive session, 2026-08-22.
**Feeds**: `dark-factory-prd-to-issues`. Grounded in `FACTORY_RULES.md`'s
one-definition-of-green principle, `harness/README.md` / D-004's ladder design,
and three shipped incidents that share one root cause.

## Problem — the "green suite, dead game" gap

The entire `KrowdKontrol.Unit.` suite runs in worlds that never execute the
engine's real lifecycle: `CreateNewMap()` worlds skip begin-play, tests invoke
lifecycle methods **by direct call** for synchronous determinism, and nothing
ever runs a map the way the game does. This has now shipped three
live-only failures past a fully green gate:

1. **#199** — placed beacon markers kept a stale serialized hierarchy: the
   `PostInitializeComponents` self-heal never runs in editor worlds, the exact
   world the E2E inspects and saves persist from.
2. **#223** — defeat-restart traveled to Epic's OpenWorld template: PIE's
   `UEDPIE_0_` map-name mangling only exists in a real PIE session, which no
   test ever entered.
3. **#234** (open) — `OnLevelClear` never fires in real play despite every
   condition met: every lifecycle test drives `OnWorldBeginPlay()` /
   `RefreshLevelClearState()` by direct call, so the real begin/tick path has
   never been executed by any gate. Found by a live operator playthrough that
   proved the whole control→herd→bank chain and still got no save file.

The operator can catch these interactively; the pipeline cannot — not because
it lacks tools, but because **no test tier runs the real engine paths**. That
tier is buildable headlessly: Unreal's Automation framework can open a real map
and run a PIE session inside an automation test (latent `AutomationOpenMap` /
start-PIE commands), letting begin-play, subsystem `OnWorldBeginPlay`, ticks,
and real actor lifecycles execute — all under the existing
`run_ue_automation.sh` runner.

## Requirements

### REQ-1: A `KrowdKontrol.PIE.` test group that runs real sessions (P0)
- A new test group (name distinct from `Smoke`/`Unit`) whose tests open a real
  shipped map, start an in-editor PIE session, pump frames via latent commands,
  assert against live state, and end the session — real begin-play, real
  subsystem ticks, real serialized placed actors, no direct lifecycle calls.
- Deterministic drivers stay code-side (call `ReceiveControl`, teleport actors
  onto zones via C++ — no simulated input; input simulation stays out of scope).
- Must run under the existing headless invocation (`run_ue_automation.sh`
  filter arg); if `-nullrhi` proves incompatible with PIE automation, the
  script's documented filter-branch hook (its own comment reserves exactly
  this) gains a group-specific flag set — loudly, not silently.

### REQ-2: Harness ladder integration — one definition of green (P0)
- `harness/harness.config.json` / `ci.py`'s ladder gains the PIE group as a
  rung after `unit`, so every validate-pr run and every fix-run self-check
  executes it. No workflow YAML changes needed if it rides the existing
  harness invocation — that is the preferred shape.
- Runtime budget: the group must stay lean (a handful of scenario tests, not a
  port of the unit suite) — target well under 2 minutes so the gate stays
  usable.

### REQ-3: The first three scenario tests (P0)
1. **Lifecycle live-fire** — open L_Level01, start PIE, assert level-begin
   observably fired, drive every enemy to Banked through real state
   transitions (code-driven control + teleport-onto-zone), pump ticks, assert
   `OnLevelClear` fires and `Saved/SaveGames/KrowdKontrol_LevelClearTimes.sav`
   exists with a recorded time. This is open issue #234's acceptance test —
   coordinate, don't duplicate: #234 fixes the subsystem, this PRD's test
   proves it and pins it forever.
2. **Serialized placed-actor health** — open both shipped maps in the PIE
   session and assert the placed markers/zones have the correct component
   hierarchy and positions at runtime (pins #199's class of bug, and would
   have caught the still-open zones-at-origin misplacement).
3. **Defeat-restart round trip** — start PIE on L_Level01, drive energy to 0,
   assert the reloaded world is L_Level01 (not the engine default map) with
   restored energy (pins #223's fix in the only environment it can regress in).

### REQ-4: Guidance for future PIE tests (P1)
- A short authoring note in `harness/README.md`: when a feature's failure mode
  involves begin-play order, ticking, serialized instances, PIE naming, or
  save/load, it needs a `KrowdKontrol.PIE.` scenario, not only unit coverage.
  The three incidents above are the motivating examples to cite.

## Out of scope
- Simulated player input (viewport focus/keystrokes) — stays with the E2E
  holdout and operator playtests.
- Visual/perceptual judgment (screenshot comparison) — separate concern.
- Porting existing unit tests into PIE form — units stay the fast tier;
  PIE tests are scenario-shaped only.
- Fixing #234 itself (that issue owns the subsystem fix; this PRD owns the
  tier that proves it).

## Existing surfaces to build on (do not reinvent)
`harness/run_ue_automation.sh` (incl. its reserved filter-branch comment) /
`harness/ci.py` + `harness.config.json` ladder (D-004);
Unreal Automation latent commands (`AutomationOpenMap`, start/stop PIE,
`FWaitLatentCommand`) — engine-provided;
`ULevelLifecycleSubsystem` / `ULevelClearTimeSubsystem` (scenario 1);
`APlaceholderTargetZoneActor` heal + `ATargetZone`/`ARoomActor` spawn wiring
(scenario 2); `AKrowdKontrolPlayerController` restart path +
`UPlayerEnergyComponent` (scenario 3); the `KrowdKontrol.<Group>.<Name>`
naming convention (CLAUDE.md).
