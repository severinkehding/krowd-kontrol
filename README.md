# krowd-kontrol

A [Dark Factory](https://github.com/coleam00/dark-factory-experiment) — a repo built,
reviewed, and merged almost entirely by AI coding agents, running on
[Archon](https://github.com/coleam00/archon) as the workflow harness.

**Current state: bootstrap.** The harness, governance files, and workflows are wired up
and verified end-to-end, but `MISSION.md` is still a placeholder — see `FACTORY.md` for
exactly what that means and `harness/README.md` for the validation gate's current
state. There's no app yet.

## How this works

Read `FACTORY.md` for the five-component breakdown (workflow-driven repo, trigger,
deployment, guidance layer, validation harness) and `FACTORY_RULES.md` for the full
operating rules — triage criteria, quality gates, protected files, the stop button.
`MISSION.md` will define what krowd-kontrol actually is once that's decided.

## Skills

`.claude/skills/` has 68 game-dev skills from
[gamedev-skills/awesome-gamedev-agent-skills](https://github.com/gamedev-skills/awesome-gamedev-agent-skills)
(Godot, Unity, Unreal, Bevy, Phaser, PixiJS, three.js, LÖVE, pygame, Roblox, plus
engine-neutral disciplines, genres, and shipping workflows), on top of `archon` and
`agent-browser`. A `router` skill auto-detects engine + task and loads only what's
relevant — you don't name skills yourself, in this session or in factory-dispatched
workflows.

```bash
npx skills update gamedev-skills/awesome-gamedev-agent-skills   # pull latest
npx skills remove gamedev-skills/awesome-gamedev-agent-skills   # uninstall
```

Also installed: `unreal-engine-cpp-pro` (UObject hygiene, GC, reflection macros, performance
patterns) from [sickn33/agentic-awesome-skills](https://github.com/sickn33/agentic-awesome-skills)
— one exact skill pulled from their 2,000+ catalog, not a bulk install. That catalog's own
maintainers warn against installing it wholesale (it includes skills flagged `critical`/
`offensive`-use-only); this one audited clean (`risk: safe`, zero command/network/credential/
filesystem/privilege signals). Pull another skill the same way, always audit first:

```bash
npx agentic-awesome-skills audit --skills <skill-id>
npx agentic-awesome-skills --path "$(pwd)/.claude/skills" --skills <skill-id>
```

**`unreal-agent-harness`** — Unreal MCP troubleshooting (`"Unable to connect"`, editor
crashes/hangs, stuck camera captures, port conflicts), the capture/QA loop, and a
Python/toolset-sandbox reference, curated from
[per-simmons/unreal-agent-harness](https://github.com/per-simmons/unreal-agent-harness)
(used with permission — that repo has no license, so this is a hand-picked subset, not
a mirror; its city-building demo content stays in the source repo).

**Unreal MCP itself** — per Epic's [official docs](https://dev.epicgames.com/documentation/unreal-engine/unreal-mcp-in-unreal-editor),
wired up against the real `KrowdKontrol.uproject`: `ModelContextProtocol` + `AllToolsets`
plugins are enabled, and the client side is registered in `.mcp.json` (this session) and
`.archon/mcp/unreal.json` (factory workflow nodes) — plain HTTP at
`http://127.0.0.1:8000/mcp`, no relay process needed (WSL2 reaches the Windows host's
loopback directly). **The server itself isn't started** — that's a one-command manual
step (`ModelContextProtocol.StartServer` in the Editor's console) left to whoever has
the Editor open, same reasoning as Blender below. Nothing this repo's headless
cron/Archon automation can drive on its own — it's live for whoever *is* driving Unreal
Editor interactively.

**`blender-mcp`** — a *live* MCP connection (not a static knowledge skill) to Blender's
own official [MCP server](https://www.blender.org/lab/mcp-server/), installed and
enabled on the Windows host's Blender 5.2 for generating/editing assets that feed into
the Unreal project. Registered in `.mcp.json` (this session) and
`.archon/mcp/blender.json` (factory workflow nodes). **Not auto-started** — Blender's
own docs warn the server executes LLM-generated Python with no sandboxing, so starting
the bridge is a deliberate action, not an always-on service. See the skill for exactly
how, and for an upstream dependency-pin bug (`mcp[cli]>=1.2.0` with no upper bound)
worth knowing if you ever reinstall it.

## Contributing

Once `MISSION.md` is filled in for real: file an issue. Don't open a PR — the factory
will. Until then, every issue gets rejected with a placeholder-state reason regardless
of content — see `FACTORY_RULES.md` §0.

## Setup (one-time, this machine)

```bash
# Bun + Archon CLI (built from source — archon.diy's installer may be blocked on
# some networks; source works identically)
curl -fsSL https://bun.sh/install | bash
git clone https://github.com/coleam00/archon ~/tools/archon && cd ~/tools/archon && bun install
mkdir -p ~/.local/bin && cat > ~/.local/bin/archon <<'EOF'
#!/usr/bin/env bash
exec bun "$HOME/tools/archon/packages/cli/src/cli.ts" "$@"
EOF
chmod +x ~/.local/bin/archon

# jq (used by orchestrator.sh and several workflow nodes)
curl -fsSL -o ~/.local/bin/jq https://github.com/jqlang/jq/releases/latest/download/jq-linux-amd64
chmod +x ~/.local/bin/jq

# gh, authenticated
gh auth login

# Global Archon config — interactive session use rides your `claude` login
mkdir -p ~/.archon && echo "CLAUDE_USE_GLOBAL_AUTH=true" > ~/.archon/.env

# uv (Python) — also needed by blender-mcp below
curl -LsSf https://astral.sh/uv/install.sh | sh

# blender-mcp — see .claude/skills/blender-mcp/SKILL.md for the extension
# build/install steps (needs Blender itself, on whatever host runs it)
git clone https://projects.blender.org/lab/blender_mcp.git ~/tools/blender_mcp
cd ~/tools/blender_mcp && uv venv .venv --python 3.10
source .venv/bin/activate && uv pip install -r mcp/requirements.txt
uv pip install -e mcp && uv pip install "mcp[cli]<2.0.0"   # pin — see skill for why
cat > ~/.local/bin/blender-mcp <<'EOF'
#!/usr/bin/env bash
exec "$HOME/tools/blender_mcp/.venv/bin/blender-mcp" "$@"
EOF
chmod +x ~/.local/bin/blender-mcp
```

Verify: `archon doctor` and, from this repo, `archon validate workflows && archon validate commands`.

## Web dashboard

Archon ships a web UI — conversations, live workflow-run progress, and a
drag-and-drop workflow builder. It's **machine-level, not repo-level**: one
instance covers every Archon project registered on this machine (krowd-kontrol
included, auto-registered the first time a workflow runs against it), the same
way the CLI is.

**Always on**: a small wrapper script (`~/.local/bin/archon-ui`, not part of
this repo — it's Archon-level, not krowd-kontrol-level) starts it, and cron
keeps it up — `@reboot` for a cold machine start, plus a `*/10 * * * *` check
that's a no-op if it's already running (same self-heal pattern as
`scripts/orchestrator.sh`).

```text
http://localhost:5173
```

```bash
archon-ui status   # is it up right now
archon-ui start    # start it (safe to run if already running)
archon-ui stop     # stop both processes
```

Logs: `~/.archon/logs/archon-server.log` and `archon-web.log`. Built from
source (`~/tools/archon`), so it's the same install this whole setup already
uses — no separate download.

## Run something right now (no cron needed)

```bash
cd krowd-kontrol

archon workflow list                                              # see all 4 factory workflows + Archon's bundled defaults
archon workflow run dark-factory-triage --branch triage/$(date +%s) "Triage open issues"
archon workflow run dark-factory-fix-github-issue --branch fix/issue-42 "Fix issue #42"
archon workflow run dark-factory-validate-pr --branch validate/pr-17 "Validate PR #17"
```

Always use `--branch` — it isolates the run in its own git worktree so it can't collide
with anything else. Long runs: add `run_in_background: true` if scripting this, or just
open a second terminal.

## Let it run itself (the cron loop)

Already installed as `*/10 * * * *` via `crontab -e`, pointed at
`scripts/orchestrator.sh`. It reads GitHub labels, dispatches in priority order (fix/validate
PRs → accepted issues → untriaged issues), and does nothing if nothing needs doing.

**Turn it on**: it needs a real Anthropic API key — put one in `~/.archon/orchestrator.env`:

```bash
echo "CLAUDE_API_KEY=sk-ant-..." > ~/.archon/orchestrator.env
chmod 600 ~/.archon/orchestrator.env
```

Without a key it exits safely every cycle (`STOPPED: CLAUDE_API_KEY not set`) — no key,
no dispatches, no cost.

**Watch it**:

```bash
tail -f ~/.archon/logs/krowd-kontrol/cron.log        # orchestrator's own dispatch log
ls ~/.archon/logs/krowd-kontrol/                     # per-dispatch workflow logs
archon isolation list                                # active worktrees right now
```

## Stop it

Two independent kill switches, both fail closed (network down or unreadable = stopped,
never "carry on"):

```bash
touch .factory-stop                          # local, works offline, checked first
gh issue create --title x --label factory:stop   # remote, reachable from a phone
```

Remove the file / remove the label to resume. Nothing needs restarting — the next cron
tick just re-checks.

## Change what the factory does

- **Scope** (what it's allowed to build): edit `MISSION.md`.
- **Process** (triage rules, quality gates, protected files): edit `FACTORY_RULES.md`.
- **Code conventions**: edit `CLAUDE.md`.
- **The workflows themselves**: `.archon/workflows/*.yaml` + the prompts they load from
  `.archon/commands/*.md`.

All of the above are on the protected-files list — the factory can propose changes to
everything else, never to these. Edit them locally, commit, push; the next dispatch
picks it up, no restart needed.
