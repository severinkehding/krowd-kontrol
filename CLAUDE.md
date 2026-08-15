# CLAUDE.md

Instructions for AI coding agents working in this repository. Read this before making
any code changes.

This file covers **how the code is written**. For *what* to build, see `MISSION.md`
(currently a placeholder). For *how the factory operates*, see `FACTORY_RULES.md`. When
this file and those conflict, `MISSION.md` wins on scope, `FACTORY_RULES.md` wins on
process, and `CLAUDE.md` wins on code style.

> 🚧 **PLACEHOLDER.** No app is tracked in this repo yet, so most of this file is still
> TBD. One real fact is known and recorded below (Environment) — fill in the rest (repo
> layout, conventions, protected-path specifics) in the same commit that wires the
> Unreal project into this repo's factory loop.

---

## Environment

Claude Code (this repo's interactive + factory-dispatched sessions) runs in **WSL2
(Linux)**. **Unreal Engine itself runs on the Windows host**, not in WSL — the editor,
GPU, and MCP plugin all live on the Windows side.

**The Unreal project** (`KrowdKontrol.uproject`, fresh — `ModelingToolsEditorMode`
enabled, MCP plugin not yet enabled) lives on the Windows filesystem:

```
Windows path: C:\Users\Admin\OneDrive\Dokumente\Unreal Projects\KrowdKontrol
WSL path:     /mnt/c/Users/Admin/OneDrive/Dokumente/Unreal Projects/KrowdKontrol
```

It is **not currently tracked in this git repo** — it's a separate directory on the
Windows host, reachable from this WSL session read/write via the `/mnt/c/...` mount.
Whether/how it becomes part of `krowd-kontrol`'s git history (submodule, separate repo
entirely, symlink, or left as-is with this repo only holding the factory harness) is an
open question, not yet decided — don't assume one without asking.

See the `unreal-agent-harness` skill (`.claude/skills/unreal-agent-harness/`) for MCP
connection setup and troubleshooting. Its `scripts/ue_launch.sh` and `ue_crashlog.sh`
are **macOS-specific templates** from the original author's machine (hardcoded
`/Users/...` paths, `sips`/`open`/macOS crash-log locations) — they need a Windows
rewrite (PowerShell or a WSL-side script invoking `.exe` paths under `/mnt/c/...`)
before they're usable here. Not done yet.

## Project Overview

**TBD.** See `MISSION.md`.

## Tech Stack

**Unreal Engine** (confirmed — see Environment above). Version/plugins/gameplay
architecture: **TBD** beyond what's already in `KrowdKontrol.uproject`.

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
