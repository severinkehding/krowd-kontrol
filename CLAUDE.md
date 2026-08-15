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

**The Unreal project** (`KrowdKontrol.uproject` — `ModelingToolsEditorMode`,
`ModelContextProtocol`, `AllToolsets`, `MCPClientToolset` all enabled; MCP server not
yet started — see the `unreal-agent-harness` skill) lives on the Windows filesystem:

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

**Blender 5.2** is also installed on the Windows host
(`C:\Program Files\Blender Foundation\Blender 5.2`), wired up via the `blender-mcp`
skill (`.claude/skills/blender-mcp/`) for asset generation feeding into the Unreal
project. Cross-boundary calls from WSL to Windows `.exe`s work directly — `wslpath -w`
converts a WSL path to something the Windows binary can read, and it round-trips (a
Windows process writing "here" lands back at the expected WSL path too). The MCP bridge
is **not** left running by default — see that skill for why and how to start it.

### WSL2 ↔ Windows networking — required for any live MCP connection

**Both `blender-mcp` and `unreal-mcp` need WSL2 to reach `127.0.0.1` on the Windows
host** (Unreal's HTTP server directly; Blender's TCP bridge via the `blender-mcp`
relay process, which itself runs in WSL). By default WSL2 uses NAT networking with
"localhost forwarding," which is *supposed* to make this work automatically — **it did
not, on this machine.** Diagnosed 2026-08-15: `UnrealEditor` already had an explicit
Windows Firewall "Allow" rule for both Private and Public profiles, and WSL still
couldn't reach port 8000 or Blender's port 9876 — that combination points at WSL2's
NAT/loopback-forwarding mechanism itself failing, not a firewall block (no admin
rights were available in this session to rule firewall out completely, but an
already-allowed process still failing is strong evidence against it being the cause).

**Fix applied: WSL2 mirrored networking mode** — `%UserProfile%\.wslconfig`
(`C:\Users\Admin\.wslconfig`) now has:
```ini
[wsl2]
networkingMode=mirrored
```
This makes WSL2 share the Windows host's network namespace outright — `127.0.0.1` means
the same thing on both sides, no per-port forwarding to rely on, so it fixes this
category of problem for any future port too, not just these two. Requires Windows 11
with a build supporting mirrored mode (confirmed here: build 26200, well above the
~22621.2428 minimum) and **a full WSL restart to take effect** (`wsl --shutdown` from a
Windows-side terminal — not from inside WSL — then reopen). **Applied but not yet
verified** as of this commit — restarting WSL ends the session that made this change;
verify connectivity fresh after the restart (`curl http://127.0.0.1:8000/mcp` from WSL
should get a response instead of hanging/refusing, and likewise a raw TCP connect to
`127.0.0.1:9876` once Blender's bridge is started).

## Project Overview

**TBD.** See `MISSION.md`.

## Tech Stack

**Unreal Engine** (confirmed — see Environment above), asset pipeline through
**Blender 5.2** via `blender-mcp`. Version/plugins/gameplay architecture: **TBD** beyond
what's already in `KrowdKontrol.uproject`.

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
