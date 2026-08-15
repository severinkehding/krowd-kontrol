# The validation harness

```bash
python harness/ci.py --quick   # static + unit. Runs anywhere, no app required.
python harness/ci.py           # the whole gate. Needs a real app - see below.
```

Current state (bootstrap - no app yet):

```
HARNESS_START mode=quick driver=http
STATIC_SKIPPED no 'static' command in harness.config.json
UNIT_SKIPPED no 'unit' command in harness.config.json
GATE_OK mode=quick
```

## What is in here, and where it came from

| File | Origin |
|---|---|
| `ci.py` · `appproc.py` | **Verbatim from the `build-dark-factory` skill**, via dark-factory-experiment. The step ladder and the app-process driver (http/cli/library) are the same in every factory; do not edit them here - change `harness.config.json` instead. |
| `harness.config.json` | This repo. Every command krowd-kontrol's gate runs. Currently empty on purpose - see below. |
| `serve.py` | This repo. Placeholder for the app's real boot entrypoint (wired in via `http.start` once there is one). |
| `e2e.py` | This repo. Placeholder - `NotImplementedError` until there's an app and MISSION.md defines what "working" means. |
| `static.py` / `unit.py` | **Not present yet.** Add these if the stack ends up split across multiple sub-projects (e.g. a backend + frontend that each need their own check command aggregated into one `static`/`unit` rung) - dark-factory-experiment's version of these files exists for exactly that reason. A single-stack app can usually just put one command directly in `harness.config.json`. |

## The bootstrap gap, stated honestly

There is no app, so:

- `harness.config.json`'s `static`/`unit`/`http` are empty.
- `ci.py --quick` skips static and unit loudly (`STATIC_SKIPPED`/`UNIT_SKIPPED`) and
  still reports `GATE_OK` - that's the harness's own "loud, never silent" design working
  as intended, not a workaround.
- `ci.py` (full mode) will fail the moment it tries to start the app:
  `appproc.py`'s `HttpApp` raises `AppDidNotStart("driver=http but http.start is empty
  in harness.config.json")`. This is deliberate - a gate that quietly reported success
  with nothing running would be a worse starting state than one that fails loudly.
- Because of that, `.archon/workflows/dark-factory-validate-pr.yaml`'s deterministic
  infra-backstop will route real PRs to `factory:needs-human` rather than auto-merging,
  until an app exists. See `FACTORY.md` for why that's expected right now.

## What to fill in first

In the same commit that adds `app/`:

1. `harness.config.json` - set `http.start` (and `http.health_path` /
   `http.health_contains` if the default `/health` doesn't fit), `static`, `unit`, and
   `unit_count_pattern`.
2. `harness/serve.py` - the real boot script `http.start` invokes.
3. `harness/e2e.py` - real assertions once MISSION.md defines what the app is supposed
   to do. Follow dark-factory-experiment's `e2e.py` for the shape: assert hard
   invariants from MISSION.md against the live process, not implementation details.
4. Update `.archon/workflows/dark-factory-validate-pr.yaml`'s `dark-factory-behavioral-e2e`
   command if there's a real user journey worth walking with `agent-browser` (see
   `.claude/skills/agent-browser/SKILL.md`) - that layer has authority over a merge the
   same way it does in dark-factory-experiment; `harness/e2e.py` is a floor under it, not
   a replacement for it.

Nothing here needs a `.factory/holdout/` or `.factory/locks/floor.json` yet - both
encode a *measured baseline* (dark-factory-experiment's are DynaChat's real numbers).
Add those once there's a real gate to floor and a builder to hold assertions out from
under.
