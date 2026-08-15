# The factory

<!--
  Maintainer: whoever changes what runs unattended. Update the level and the date in the
  SAME commit that changes either - a stale level here is a lie about what is running
  with nobody watching.
-->

**Current autonomy level: 0 (bootstrap)** — the harness, governance files, and
workflows are wired up and verified, but MISSION.md is a placeholder, so triage
rejects every issue with a placeholder-state reason (see `FACTORY_RULES.md` §0 — not
`factory:needs-human`, since triage never emits that verdict, by design) instead of
actually building anything. This is intentional: the loop runs for real, but has
nothing real to decide yet. Level jumps to **4** — untriaged issue classified, planned,
built, reviewed, independently validated, and **merged** with no human in the chain —
the commit MISSION.md is filled in for real.
**Level 5 is deliberately not the goal.** The factory will not write its own issues.
**Stop button:** `.factory-stop` in the orchestrator's working copy (works with the
network down) **and** the `factory:stop` label on any open issue (reachable from a
phone). Both fail closed. Checked by `scripts/factory-stop.sh` before anything else is
read.
**Built from PRD:** none yet — `MISSION.md` is a placeholder. **Change one, change
both**, in the same commit, once a PRD exists.

## The five components, as built here

| # | Component | This repo's version |
|---|---|---|
| 1 | Workflow-driven repo | **Archon**, five YAML workflows in `.archon/workflows/` (triage, fix-github-issue, validate-pr, comprehensive-test [parked], prd-to-issues). State in GitHub labels |
| 2 | The trigger | Pure-bash orchestrator, **in this repo** at `scripts/orchestrator.sh`, cron every **10 min**, `MAX_PARALLEL=4` with per-target locks |
| 3 | Deployment | **Decided:** local, not blue/green — see below. Not built yet, nothing to package until there's real game content |
| 4 | Guidance layer | `MISSION.md` (placeholder) · `FACTORY_RULES.md` · `CLAUDE.md` (placeholder), all three protected |
| 5 | Validation harness | `harness/ci.py` + `appproc.py` (verbatim, generic) + repo-specific `harness.config.json`/`serve.py`/`e2e.py` (currently empty placeholders — see `harness/README.md`) |

Unlike dark-factory-experiment, the orchestrator lives **inside this repo** rather than
on a separate VPS — this machine is both the operator's machine and the dispatch host,
so there's no reason to hide it outside version control. `scripts/factory-stop.sh` still
calls it out as protected (section 5 of `FACTORY_RULES.md`) the same way.

## Component 3, decided but not built

krowd-kontrol is a local Unreal Engine project, not a web service — there's no live
traffic to route between a blue and a green instance, so dark-factory-experiment's
blue/green pattern doesn't map here. "Deployment" for this project means **packaging a
local Windows build** (`UnrealEditor-Cmd.exe -run=BuildCookRun`, or equivalent) once
there's real game content worth packaging. Not built yet — there's nothing to package.
When it is, it belongs in `scripts/` alongside the orchestrator, on the protected-files
list the same way (`FACTORY_RULES.md` §5, "any file under `deploy/`, `infra/`,
`scripts/deploy/`, or equivalent").

The Unreal project itself is **not tracked in this repo** — see `CLAUDE.md`'s
Environment section for why (Git LFS would be required before the first binary asset
lands, and that tradeoff hasn't been taken).

## Getting real work into the loop

Two ways an issue enters the queue:

- **A human files one directly** — the original path, always available.
- **`dark-factory-prd-to-issues`** — feed it a PRD (pasted text, or `file:docs/prd.md`)
  and it decomposes it into discrete, independently-buildable issues and files them.
  It does not decide what to build (a human wrote the PRD) and it does not triage what
  it files — every issue it creates goes through the exact same `dark-factory-triage`
  gate as a human-filed one. This does not conflict with "the factory will not write
  its own issues" above: decomposing a human-authored PRD into issue-sized units is
  mechanical, not ideation. Manual trigger only — not part of the cron dispatch
  rotation, since a PRD lands whenever a human decides it does, not on a schedule.

## The gates that are actually code

Everything else is a prompt instruction, which is a suggestion with good manners. These
cannot be argued past:

1. **`apply-verdict`** in `dark-factory-validate-pr.yaml` — bash reads a verdict file
   and branches on it. The merge is never a model deciding to merge.
2. **The `APP_STARTED` backstop**, same node. Deterministic bash reads the harness run's
   output and flips any `approve` to `reject`+escalate when the marker is absent.

## Component 5, stated honestly

No app exists yet, so:

```
python harness/ci.py --quick
  HARNESS_START mode=quick driver=http
  STATIC_SKIPPED no 'static' command in harness.config.json
  UNIT_SKIPPED no 'unit' command in harness.config.json
  GATE_OK mode=quick
```

Full mode (`python harness/ci.py`) fails loudly at the app-boot step — see
`harness/README.md` for why that's correct right now, not a bug. Comprehensive-test
(weekly regression) is ported but parked unused until there's an app worth
regression-testing.

## What "verified" means for this bootstrap state

- `archon doctor`, `archon validate workflows`, `archon validate commands` all clean.
- `harness/ci.py --quick` → `GATE_OK`.
- `scripts/factory-stop.sh` fails closed when it can't reach GitHub (confirmed before
  the repo existed on GitHub — see the repo's initial setup).
- A real issue filed on this repo gets triaged live and lands on `factory:rejected`
  with a placeholder-state reason (confirmed: issue #1, 2026-08-15) — proving the full
  chain (Archon → Claude Code → `gh` labels and comments) fires end to end, even with
  nothing real to decide yet.
- The cron orchestrator fires on schedule and its dispatch logic is exercised at least
  once against real repo state.

None of that requires MISSION.md to be real. Filling in MISSION.md is what turns this
from "the loop runs" into "the loop builds something."
