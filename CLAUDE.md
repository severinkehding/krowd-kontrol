# CLAUDE.md

Instructions for AI coding agents working in this repository. Read this before making
any code changes.

This file covers **how the code is written**. For *what* to build, see `MISSION.md`
(currently a placeholder). For *how the factory operates*, see `FACTORY_RULES.md`. When
this file and those conflict, `MISSION.md` wins on scope, `FACTORY_RULES.md` wins on
process, and `CLAUDE.md` wins on code style.

> 🚧 **PLACEHOLDER.** No app exists yet, so there's no real stack or layout to document.
> Fill this in - tech stack, repo layout, conventions, protected-path specifics - in the
> same commit that adds the first app skeleton.

---

## Project Overview

**TBD.** See `MISSION.md`.

## Tech Stack

**TBD.**

## Repo Layout

```
krowd-kontrol/
├── MISSION.md               # Product scope (placeholder) - factory reads this at triage
├── FACTORY_RULES.md         # Factory operational rules - every workflow reads this
├── CLAUDE.md                # This file - code conventions (placeholder)
├── FACTORY.md                # Factory status: autonomy level, components, stop button
├── README.md                 # Human-facing overview
├── harness/                  # Validation gate - see harness/README.md
├── .factory/                 # Factory-internal state (decisions log; holdout + locks
│                              #   added once there's a real baseline to floor)
├── .archon/
│   ├── workflows/            # dark-factory-*.yaml - the four factory workflows
│   └── commands/              # dark-factory-*.md - prompt files those workflows load
├── .claude/skills/            # archon + agent-browser skills
└── scripts/
    ├── factory-stop.sh        # the stop button check
    └── orchestrator.sh        # cron dispatcher (added once the loop goes live)
```

`app/` does not exist yet - added by the first app-skeleton work, at which point this
section and Tech Stack above must be filled in for real, in the same commit.

## Conventions

**TBD** once there's a stack to have conventions about.

## Protected Paths

Restated from `FACTORY_RULES.md` §5 for convenience - the factory cannot modify
`MISSION.md`, `FACTORY_RULES.md`, `CLAUDE.md`, anything under `.github/`, any
Dockerfile/compose/deploy config, or anything holding secrets. See `FACTORY_RULES.md`
for the authoritative, complete list.
