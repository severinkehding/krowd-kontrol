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

**Decided: it stays out of this git repo.** It's a separate directory on the Windows
host, reachable from this WSL session read/write via the `/mnt/c/...` mount, and this
repo only holds the factory harness — not the Unreal project itself. Considered and
rejected: committing it as a subfolder here, because Unreal's binary asset files
(`.uasset`, `.umap`) routinely exceed GitHub's 100MB hard limit and doing this properly
needs Git LFS set up *before* the first binary lands, not retrofitted after (that means
a history rewrite). Revisit if that tradeoff changes.

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
Windows-side terminal — not from inside WSL — then reopen).

**Confirmed working 2026-08-15.** The first two restart attempts didn't actually take —
`uptime -s` kept showing the *original* boot time, proving the WSL2 VM itself never
stopped (closing/reopening a terminal window reconnects to the same still-running VM;
only `wsl --shutdown` from a genuine Windows-side shell, followed by confirming
`Get-Process vmmem` errors out before reopening, actually restarts it). Once a real
restart happened: WSL's `eth0` came up as `192.168.0.26` — the Windows host's actual
LAN IP, not a `172.x` NAT address — and both `mcp__blender__get_blendfile_summary_path_info`
and `mcp__unreal-mcp__list_toolsets` returned real live data through the MCP connection.
If this ever regresses (e.g. after a Windows update resets `.wslconfig` handling),
check `ip addr show eth0` against the Windows host's real IP first — a mismatch means
mirrored mode isn't actually active, whatever the config file says.

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
├── README.md                 # Human-facing overview + workflow diagrams
├── docs/                     # PRD drop point for dark-factory-prd-to-issues
├── harness/                  # Validation gate - see harness/README.md
├── .factory/                 # Factory-internal state (decisions log; holdout + locks
│                              #   added once there's a real baseline to floor)
├── .archon/
│   ├── workflows/            # dark-factory-*.yaml - the five factory workflows
│   │                          #   (triage, fix-github-issue, validate-pr,
│   │                          #   comprehensive-test [parked], prd-to-issues)
│   ├── commands/              # dark-factory-*.md - prompt files those workflows load
│   └── mcp/                   # blender.json / unreal.json - MCP server configs for
│                              #   factory workflow nodes (mcp: field)
├── .claude/skills/            # archon, agent-browser, blender-mcp, unreal-agent-harness,
│                              #   68 gamedev skills, unreal-engine-cpp-pro
├── .mcp.json                  # MCP servers for this interactive session
└── scripts/
    ├── factory-stop.sh        # the stop button check
    └── orchestrator.sh        # cron dispatcher, live, every 10 min
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
