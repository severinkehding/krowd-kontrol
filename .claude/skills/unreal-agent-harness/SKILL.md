---
name: unreal-agent-harness
description: >
  Troubleshooting and reference knowledge for driving Unreal Engine 5.8 with an AI agent
  through the official Unreal MCP plugin (ModelContextProtocol) — connection setup, the
  capture/QA loop (CaptureViewport -> decode -> read -> correct), crash diagnosis, and
  what the ProgrammaticToolset sandbox can and cannot do. Use when the user hits "Unable
  to connect" from an Unreal MCP tool call, the Unreal Editor crashes or hangs on boot,
  CaptureViewport/SetCameraTransform stops tracking the viewport, a mesh imports at the
  wrong scale or is invisible, MCP port 8123/8000 is stuck or already in use, or before
  enabling the Unreal MCP plugin in a new project for the first time. Source:
  github.com/per-simmons/unreal-agent-harness (used with permission; curated subset —
  not the full repo, see below).
---

# Unreal Agent Harness

Curated reference from [per-simmons/unreal-agent-harness](https://github.com/per-simmons/unreal-agent-harness)
— a working harness for driving Unreal Engine 5.8 via the official Unreal MCP plugin,
with a capture/QA loop (act → capture → decode → read → correct) so an agent can see
what it built.

**What's here vs. what isn't.** This skill carries the general-purpose, reusable half of
that repo — connection setup, the QA loop, the crash/troubleshooting playbook, and the
Python API + toolset-sandbox reference. It deliberately drops the other half: that
repo's specific city-building content (Cesium/NYC streaming, PCG procedural-city
recipes, the chase-cam plane demo, Blender facade-kit job scripts, lighting recipes
tuned to one look). That's a different project's asset pipeline, not general Unreal
troubleshooting — pull it from the source repo directly if a future krowd-kontrol issue
genuinely needs it.

**The hard requirement this doesn't remove:** none of this works without a real Unreal
Editor 5.8 running, with the `ModelContextProtocol` + `AllToolsets` plugins enabled, on
a machine that actually has Unreal Engine installed. This repo's own automation (Archon
workflows, the cron orchestrator) runs headless on Linux and has no Unreal Editor to
drive — this skill is knowledge an agent can load, not a live connection this repo
maintains. Whoever is driving Unreal Editor interactively (Claude Code, MCP connected)
gets the full benefit; a fully headless factory workflow does not, until there's a
project + a machine with the editor open.

## When to reach for which file

| Situation | Read |
|---|---|
| First time enabling the MCP in a project | `references/getting-started.md`, then `references/mcp-enable.md` |
| `"Unable to connect"`, editor crash/hang, camera/capture stuck, wrong-scale import, port conflicts | `references/troubleshooting.md` — Ctrl-F the symptom |
| "Can the agent call an arbitrary UFUNCTION / run raw `unreal.` Python through MCP?" | `references/toolset-capabilities.md` — short answer: no, `ProgrammaticToolset` is a sandboxed tool-orchestration layer only |
| Writing headless/commandlet Python against the `unreal.` module (spawning actors, importing assets) | `references/python-api.md` |
| Running the capture/QA loop | `scripts/ue_qa.py` (`decode`, `latest`, `refdiff`) — see its docstring |
| Launching the editor with real logs instead of a silent max-scalability boot | `scripts/ue_launch.sh` — **macOS-specific template**, hardcoded paths, edit before use |
| Diagnosing the newest crash without dumping a multi-MB report into context | `scripts/ue_crashlog.sh` — **macOS-specific template**, hardcoded paths, edit before use |

## The loop, in short

1. **Act** — `mcp__unreal__call_tool` (e.g. `SceneTools.add_to_scene_from_asset`).
2. **Capture** — `EditorAppToolset.CaptureViewport` with `captureTransform` + `annotations`.
3. **Decode** — `python3 scripts/ue_qa.py decode --name NAME` → small PNG + JSON (camera
   pose + labeled actors).
4. **Read** — Read the PNG; check the JSON for spatial reasoning.
5. **Correct** — fix and repeat.

Two hard constraints (from `references/troubleshooting.md`'s Concurrency section, worth
repeating here since it's easy to violate by accident): always go through the agent's
own MCP tool layer for captures — raw HTTP to the MCP server returns empty for large
results (they stream on an async SSE channel the direct HTTP path doesn't see) — and
**one editor, one game thread**: never run two scene-mutating calls
(`ExecuteGraphInstance`, edits) concurrently. Read-only inspection can overlap; mutation
cannot.
