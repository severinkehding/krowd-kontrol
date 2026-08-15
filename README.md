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
```

Verify: `archon doctor` and, from this repo, `archon validate workflows && archon validate commands`.

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
