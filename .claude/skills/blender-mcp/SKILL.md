---
name: blender-mcp
description: >
  Generate, inspect, and edit 3D assets in Blender via Blender's official MCP server —
  scene/data-block summaries, Python API + manual search, screenshots, thumbnail/viewport
  renders, and arbitrary Python execution inside a running Blender instance. Use when
  asked to create or modify meshes/materials/scenes in Blender, inspect a .blend file's
  contents, or generate assets (e.g. facade kits, props) for import into the Unreal
  project (see `unreal-agent-harness` skill for the Unreal side of that pipeline — Unreal's
  `import_file` takes FBX/OBJ only, not native .blend). Requires a running Blender 5.2
  instance on the Windows host with the MCP bridge server started (see below) — this is
  not a passive knowledge skill, it drives a live application.
---

# Blender MCP

Blender's own official MCP server (blender.org/lab/mcp-server), installed and wired up
in this repo. Two components:

- **Blender add-on** (`bl_ext.user_default.mcp`) — installed and enabled in the Windows
  host's Blender 5.2. Runs a TCP bridge server (default `localhost:9876`) inside Blender.
- **`blender-mcp`** — a Python MCP server (installed at `~/tools/blender_mcp/.venv`,
  wrapper on PATH at `~/.local/bin/blender-mcp`) that an MCP client (this session, or an
  Archon workflow node via `.archon/mcp/blender.json`) launches over stdio. It relays
  requests to the add-on's TCP bridge.

```
MCP Client (Claude Code / Archon node)  ⇐ stdio ⇒  blender-mcp  ⇐ TCP :9876 ⇒  Blender add-on
```

Registered in `.mcp.json` (interactive use — approve on first `claude` launch in this
repo) and `.archon/mcp/blender.json` (factory workflow nodes: add `mcp:
.archon/mcp/blender.json` to a node).

## ⚠️ Security — read before starting the bridge

Blender's own docs are blunt about this: **the MCP server executes LLM-generated Python
inside Blender with no sandboxing** — it can delete or exfiltrate anything the Blender
process can touch. Their own recommendation: use a VM, or a machine without access to
sensitive data. This repo does **not** auto-start the bridge for that reason (no
`use_autostart`, no cron/reboot launcher, unlike `archon-ui`) — starting it is a
deliberate, one-at-a-time action, not an always-on service.

Blender itself has a second, independent gate: network access is off by default and
must be explicitly enabled per-launch (`--online-mode` or the equivalent preference) —
confirmed while setting this up; a bare launch refuses to start the bridge with `Error:
Online access must be enabled in the system preferences`.

## Starting the bridge (you do this, not the agent, unless asked)

**GUI** (normal use — Blender open anyway):
Edit → Preferences → Add-ons → MCP → **Start** (or check **Auto Start** to persist
across restarts of *that Blender session* — still not system-wide autostart).

**Background** (no GUI, e.g. for a batch asset-generation pass):
```bash
"/mnt/c/Program Files/Blender Foundation/Blender 5.2/blender.exe" \
  --online-mode --background --command blender_mcp --host localhost --port 9876
```
(Flag order matters — `--online-mode` must come *before* `--command`, discovered the
hard way while setting this up.) This blocks the terminal; run it in its own session/
background job, and stop it (Ctrl-C, or kill the process) when done rather than leaving
it listening.

**Stopping**: GUI → same panel → **Stop**. Background: kill the `blender.exe` process.

## Installing (already done, documented for reproducibility)

```bash
git clone https://projects.blender.org/lab/blender_mcp.git ~/tools/blender_mcp
cd ~/tools/blender_mcp
uv venv .venv --python 3.10 && source .venv/bin/activate
uv pip install -r mcp/requirements.txt
uv pip install -e mcp
# Upstream pin bug: requirements.txt says mcp[cli]>=1.2.0 with no upper bound.
# mcp 2.0.0 (latest on PyPI) restructured mcp.server.fastmcp away entirely, so an
# unpinned install breaks at import with "No module named 'mcp.server.fastmcp'".
# Fix: pin below 2.0.0.
uv pip install "mcp[cli]<2.0.0"

# Build + install the Blender extension (WSL → Windows Blender, cross-boundary paths
# via `wslpath -w`; this worked directly, no need to copy the source to C:\ first):
BLENDER="/mnt/c/Program Files/Blender Foundation/Blender 5.2/blender.exe"
"$BLENDER" -c extension build --source-dir "$(wslpath -w ~/tools/blender_mcp/addon/blender_mcp_addon)"
"$BLENDER" -c extension install-file "$(wslpath -w ./mcp-1.0.0.zip)" --repo=user_default
"$BLENDER" --background --python-expr "
import bpy
bpy.ops.preferences.addon_enable(module='bl_ext.user_default.mcp')
bpy.ops.wm.save_userpref()
"
```

## Tools exposed (once the bridge is running)

Mostly inspection/analysis (`get_blendfile_summary_*`, `get_objects_summary`,
`get_object_detail_summary`, `search_api_docs`, `search_manual_docs`,
`get_python_api_docs`, screenshots, `render_thumbnail_to_path`,
`render_viewport_to_path`), plus `execute_blender_code` /
`execute_blender_code_for_cli` for actually doing things — arbitrary `bpy` Python,
which is how asset generation/editing actually happens (there's no dedicated
"create asset" tool; you write the Python).

## The pipeline into Unreal

Blender builds/edits the asset → export FBX/OBJ → `unreal-agent-harness`'s
`StaticMeshTools.import_file` on the Unreal side (glTF/GLB are rejected — FBX/OBJ only,
per that skill's troubleshooting notes).
