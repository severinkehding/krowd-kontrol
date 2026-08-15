# Decisions

Product values the factory chose on its own, and the questions it stopped to ask.

**How this file works.** `FACTORY_RULES.md` §9 splits values in two. A **product**
value - a price, a rate, a default, a name, a layout - the factory may choose, record
here, and carry on; the merge is held for a human but the work is not blocked. A
**judgement** value - a lock, a floor, a tolerance, a sample size, a required marker -
it may never choose, because choosing one is tuning the judge.

**Ask a given decision once.** A second issue that needs the same answer references the
ID and carries on. It does not re-ask.

Append only. Newest at the bottom.

---

## D-001 · The browser-driven E2E holdout has no live app to connect to

**Status:** open · **Raised:** 2026-08-15 (repo bootstrap) · **Blocks:** auto-merge, permanently, until resolved

`harness/ci.py` (full mode) starts the app, runs `harness/e2e.py`'s assertions, and
tears the app back down - all inside one process, before `dark-factory-validate-pr.yaml`
reaches its `behavioral-e2e` node. That node is supposed to drive a real browser against
a running app as the holdout described in `FACTORY_RULES.md` §4, but nothing is left
listening for it to connect to. `dark-factory-behavioral-e2e.md` currently reports
`not_e2e_testable` / `app_booted: false` honestly rather than faking a pass, and
`dark-factory-synthesize-verdict(.md|-p2.md)`'s approve rules require `app_booted ==
true` unconditionally - so this gap does not let anything auto-merge silently. It just
means nothing can auto-merge at all until it's fixed.

This is the same shape of gap dark-factory-experiment's own `.factory/decisions.md`
D-002 documents (harness floor and browser journey living in two places) - not
something this port solved that the original repo didn't, and not something worth
half-solving without a real app to test the fix against.

**Recommendation:** once there's a real app, add a dedicated `start-app` bash node to
`dark-factory-validate-pr.yaml` that reads `harness.config.json`'s `http.start` /
`health_path` (the same fields `appproc.py` already reads), launches it in the
background with a PID + port file under `$ARTIFACTS_DIR`, and leaves it running for
`behavioral-e2e` to drive - mirroring dark-factory-experiment's `install-deps` +
`start-app` pattern, but staying driver-agnostic via `harness.config.json` instead of
hardcoding a stack. Tear it down in the workflow's teardown node, not inside
`harness/ci.py`.

This is a **judgement** value in the sense that "is the browser walk representative of
the real journey" cannot be decided by the factory - but the *mechanism* to leave an app
running is a product-neutral engineering task any human (or a factory issue, once
MISSION.md is real and this stops being infrastructure-shaped work reserved for humans
per `FACTORY_RULES.md` §1) can pick up.

**Update 2026-08-15: the "no live app to connect to" half of this is now resolved -
the "browser" half was always the wrong plan anyway (see D-004).** `harness.config.json`
now runs `driver: "cli"` against `app/`'s real Unreal project, and `harness/e2e.py`
runs a real, passing Automation Framework test through it
(`harness/run_ue_automation.sh`). `python harness/ci.py` (full mode) genuinely reports
`GATE_OK` end to end now - `APP_STARTED driver=cli`, a real `E2E_PASSED steps=1`, not a
stub. What's still open, narrower than before: `.archon/commands/
dark-factory-behavioral-e2e.md` and the `dark-factory-validate-pr.yaml` workflow node
still assume a browser-driven journey (D-004) rather than this new mechanism, and
`.factory/holdout/run.py` still doesn't exist (`HOLDOUT_ABSENT` in the gate output).
Auto-merge is still blocked - correctly - but the remaining blocker is D-004's
workflow-file rewrite plus a real holdout, not "nothing to test against" anymore.

---

## D-002 · Orchestrator cron auth: subscription login, not a dedicated API key

**Status:** resolved · **Raised:** 2026-08-15 (repo bootstrap, discovered live) · **Blocks:** nothing

`scripts/orchestrator.sh` originally hard-required `CLAUDE_API_KEY` in
`~/.archon/orchestrator.env` and exited (`STOPPED: CLAUDE_API_KEY not set`) every cycle
without it, on the assumption that headless/cron invocations needed a dedicated,
metered Anthropic API key separate from the machine's interactive Claude Code
subscription login. That assumption was never verified, and the key was never
populated - every 10-minute cron firing since initial setup exited immediately without
dispatching anything (confirmed via `~/.archon/logs/krowd-kontrol/cron.log`: dozens of
consecutive `STOPPED` lines, zero dispatches).

Tested directly: a headless `claude -p` invocation with `ANTHROPIC_API_KEY` and
`ANTHROPIC_AUTH_TOKEN` unset and no stdin attached (`< /dev/null` — simulating exactly
how cron spawns a process) authenticated and responded successfully, using the
subscription login credential already on disk from `claude login`
(`CLAUDE_USE_GLOBAL_AUTH=true` in `~/.archon/.env`, set at bootstrap for interactive
use). No API key is required for unattended dispatch after all.

**Decision:** `scripts/orchestrator.sh` now defaults to subscription login (global
auth) for cron runs, identically to interactive use - no separate Anthropic Console
account or billing setup. `~/.archon/orchestrator.env` becomes an optional override for
anyone who later wants unattended spend on its own dedicated, metered bill; unset, it
changes nothing. Every orchestrator cycle logs which auth mode is active
(`Auth: subscription login (global auth)` or `Auth: dedicated API key (...)`) so a
silent switch - or a silent break - is visible in `cron.log` without having to inspect
the env file.

**Accepted risk, actively monitored:** subscription OAuth logins can eventually need an
interactive browser re-login, and unlike an API key there's no hard expiry the factory
can predict ahead of time. Rather than leave that as a silent gap, `scripts/orchestrator.sh`
now runs `scripts/check-auth.sh` (one cheap headless Haiku call) at the start of every
cycle, before dispatching anything - it fails closed the same way the stop button does
(logs `Auth check FAILED` with the CLI's own error detail, skips the cycle, dispatches
nothing) rather than letting a lapsed login silently waste dispatches that would fail at
their first Claude call anyway. A lapse is now visible in `cron.log` within 10 minutes
instead of only being noticed whenever a human happens to check. This is a
**product**-shaped tradeoff (convenience and zero billing setup, traded for a real but
now-monitored failure mode) accepted by the human operator, not a judgement value the
factory chose on its own - recorded here for the same transparency reason as D-001, and
because a future session hitting `STOPPED: CLAUDE_API_KEY not set` again should find
this instead of re-deriving the fix.

---

## D-003 · The Unreal project is external and unisolated — `MAX_PARALLEL` held at 1

**Status:** open · **Raised:** 2026-08-15 (MISSION.md bootstrap) · **Blocks:** parallel dispatch above 1

krowd-kontrol's actual game content is an Unreal Engine project that lives outside
this git repository, on the Windows filesystem (see `CLAUDE.md`'s Environment
section), reachable read/write from WSL via `/mnt/c/...`. This was already decided
before MISSION.md existed (Git LFS would be needed before the first binary asset
lands, and that tradeoff hasn't been taken - see `CLAUDE.md`), but it has a
consequence for the factory's dispatch model that wasn't worked through until now.

Every `archon workflow run ... --branch <name>` dispatch isolates its work in its own
git worktree of *this* repo - that's the whole point of `--branch` (`README.md`: "it
isolates the run in its own git worktree so it can't collide with anything else").
That isolation only covers what's tracked in this repo's git history. The Unreal
project isn't tracked here at all, so it isn't duplicated per-worktree - every
concurrent dispatch that touches it would be reading and writing the exact same files
at the exact same external path, simultaneously, with none of the collision
protection the worktree model is supposed to provide. Two workflows both saving the
same `.umap` at once, or one editing an asset via Unreal MCP while another rebuilds
the same content via Blender MCP, is a real corruption risk - not a hypothetical one -
and nothing in the current orchestrator (`is_locked()`/`in_flight_count()`, keyed on
git branch name) has any way to detect or prevent it, because branch identity has
nothing to do with "does this issue touch the Unreal project."

**Decision:** `MAX_PARALLEL` defaults to **1**, not the scaffold's original 4 (see
`FACTORY_RULES.md` §8, `scripts/orchestrator.sh`). This serializes *all* factory
dispatch - not just Unreal-touching work - which is the blunt, cheap, correct-for-now
answer: at bootstrap issue volume there is no throughput cost to serializing
everything, and a coarse but definitely-safe rule beats a precise rule that requires
classifying each issue's content before dispatch (which the orchestrator has no cheap
way to do - it reads labels, not issue bodies).

**Update 2026-08-15, same day: (a) tried, and it doesn't work.** Copied the project's
real files (`.uproject`, `Config/`) into `app/`, git-tracked, with `git-lfs` installed
and `app/.gitattributes` committed *before* any binary landed - the correct ordering,
done properly. Then asked the project to be opened from that new WSL-hosted location
via the Windows-side Unreal Editor. It failed: `LogProjectManager: Error: Failed to
open descriptor file wsl.localhost/Ubuntu/home/severin/projects/krowd-kontrol/app/
KrowdKontrol.uproject` - confirmed in Unreal's own log, not just a dialog box, so this
is a real engine-level limitation opening a `.uproject` across the WSL<->Windows
network-share boundary, not a typo or a config mistake. Reverted: `git rm -r app/`,
`app/` is now gitignored and instead created by `scripts/link-unreal-project.sh` as a
local, unversioned symlink straight to the project's original Windows-native path -
restores full editor compatibility (nothing about where Unreal reads the project
changed) and keeps `app/` usable from WSL/Claude Code for direct file access.

**This closes option (a) above - it is not coming back without solving the underlying
WSL<->Windows compatibility gap, which is out of scope here.** A symlink provides
*zero* per-worktree isolation (every worktree's `app/` resolves to the identical
external files, same as an unlinked path would), so `MAX_PARALLEL` stays at 1 for the
same reason as before, now with more confidence that this is the durable state, not a
bootstrap one. Option (b) - classify issues at triage time and serialize only the ones
that actually touch `app/` - remains the only realistic path to raising it, and is
still speculative infrastructure not worth building until there's enough issue volume
and non-Unreal work to make it pay for itself. Until then: do not raise
`MAX_PARALLEL` above 1, and do not re-attempt copying the project into git-tracked
`app/` without a genuinely new plan for the WSL<->Windows open failure above - it has
now been tried once and confirmed not to work.

---

## D-004 · `agent-browser` is the wrong regression tool for a browser-less game

**Status:** open · **Raised:** 2026-08-15 (MISSION.md bootstrap) · **Blocks:** auto-merge (already blocked by D-001; this is a second, independent reason)

The generic factory scaffold's mandatory end-to-end regression gate
(`FACTORY_RULES.md` §3/§4, `.archon/commands/dark-factory-behavioral-e2e.md`, the
`dark-factory-validate-pr.yaml` workflow node) was ported from dark-factory-experiment
unedited and hard-requires `agent-browser` - a browser-automation CLI - for every
PR. krowd-kontrol has no browser and no web UI; it's a 2D Unreal Engine game. As
written, that gate could never pass, for any PR, ever - not a bootstrap gap that
closes once an app exists (like D-001), but a structural mismatch that would still be
wrong even with a finished game.

**Decision:** `FACTORY_RULES.md` §3/§4 and `harness/README.md` now describe the
correct-shaped tool instead: Unreal MCP-driven visual QA (the `unreal-agent-harness`
skill's `ue_qa.py` - viewport capture, decode, and agent judgment, optionally against
a reference frame via `refdiff`), with Unreal's built-in Automation Testing Framework
(via `harness/appproc.py`'s `cli` driver, not `http`) as the harder-pass/fail-signal
option once real automated tests exist for the game's systems. `MISSION.md`'s Quality
Standards section and `FACTORY.md`'s status both point here instead of at
`agent-browser`.

**Update 2026-08-15, same day: the Automation Testing Framework option is no longer
hypothetical - it's built, and it's the one actually wired in, not `ue_qa.py`.**
Sequence: enabled C++ on the project (Tools → New C++ Class in the Editor - the
`unreal-agent-harness` MCP toolset confirmed it cannot do this itself, a real
build-time task); wrote a first real test,
`app/Source/KrowdKontrol/Private/Tests/KrowdKontrolSmokeTest.cpp`
(`KrowdKontrol.Smoke.PipelineIsAlive`, `#if WITH_DEV_AUTOMATION_TESTS`-guarded); hit
and resolved two real toolchain gaps in turn (Windows 10 SDK 10.0.19041.0 missing
entirely, then NetFxSDK missing - both fixed by the human operator via Visual Studio
Installer, not by the factory); built the module for real via `dotnet.exe` +
`UnrealBuildTool.dll` invoked directly (three earlier attempts via `Build.bat` failed
on WSL/`cmd.exe` batch-file quoting and UNC-cwd quirks - direct `.exe` invocation
sidesteps all of it); ran the test headlessly and got a real, structured JSON report
(`succeeded`/`failed`/`notRun` counts, per-test `state`) - not a guessed schema, the
actual UE 5.8 output.

That report format is what `harness/run_ue_automation.sh` now parses for real
(`UE_AUTOMATION_RESULT passed=N total=N`, `UE_AUTOMATION_OK` on a clean pass). One
constraint shaped the wiring: `harness/appproc.py`'s `cli` driver hardcodes a 60s
timeout on its boot-time smoke check with no config override (that file stays
untouched, per `harness/README.md`), and a real Editor boot does not reliably fit that
window - confirmed empirically (a bare `-version` invocation alone had to be killed
after hanging past 30s). So `harness.config.json`'s `cli.smoke_args` does a fast,
launch-free `--check-exe-only` file-existence check instead, and the real Automation
Framework run happens in `harness/e2e.py` via `app.run(..., timeout=240)`, governed by
`e2e_timeout_s` (300s) like any other e2e journey. `python harness/ci.py` (full mode)
now reports `GATE_OK` genuinely - `APP_STARTED driver=cli`, `E2E_PASSED steps=1` - not
a stub.

**Update 2026-08-15, same day: `dark-factory-behavioral-e2e.md` and the
`dark-factory-validate-pr.yaml` node are rewritten - `agent-browser` is gone from
both.** But this surfaced a sharper problem than "wrong tool," worth its own entry:
see D-005 for why the node still can't do genuine independent verification even now,
and stays honestly at `not_e2e_testable` until that's resolved.

Also still open: `app/Source/`'s test code is **not git-tracked** (it lives inside the
gitignored `app/` symlink target - see D-003) - it exists only on this machine's
Unreal project folder (OneDrive-synced, but not versioned by this repo's git
history). A future "real unit tests" pass should note this when it happens, not
assume test code is backed up the way everything else in this repo is.

---

## D-005 · The E2E holdout has no independent verification mechanism, even now

**Status:** open · **Raised:** 2026-08-15 (behavioral-e2e rewrite) · **Blocks:** auto-merge (narrower reason than D-001/D-004 now — see below)

Rewriting `dark-factory-behavioral-e2e.md` away from `agent-browser` (D-004) surfaced
a sharper question than "which tool": what would a **genuine** holdout for this
project even look like, and is it something unattended dispatch can actually run?

**Re-running the Automation Framework tests in this node would not be a real
holdout.** The whole point of the original browser-driven design is that the
validator forms its *own* judgment from observable behavior, independent of anything
the builder wrote or claimed - a gamed unit test can't fool a human (or AI) actually
looking at the screen. If this node just re-ran `harness/run_ue_automation.sh` with
the same test filter `run-harness` already ran upstream, it would be re-executing the
builder's own test code and calling that "independent" - the exact failure mode the
holdout pattern exists to prevent. So the rewritten command explicitly refuses to do
this (see its "Your Sole Purpose" and `NOT_A_SUBSTITUTE_CHECK` success criterion)
rather than quietly substituting a weaker check that would look like progress but
isn't.

**The genuine equivalent would be visual: Unreal MCP screenshot capture
(`CaptureViewport` + the `unreal-agent-harness` skill's `ue_qa.py`), independently
inspected against the issue's stated behavior** - the same spirit as a browser
screenshot, adapted to a game viewport. Building this for real needs two things,
neither of which exists yet:

1. **MCP tool access on the workflow node** - an `mcp:` config pointing at
   `.archon/mcp/unreal.json`, which this node currently doesn't have
   (`allowed_tools: [Bash]` only).
2. **A live Unreal Editor GUI session reachable at the moment this node runs.** The
   MCP server is hosted *inside* the GUI Editor process, not `UnrealEditor-Cmd.exe`
   (confirmed directly this session - closing the GUI Editor for an unrelated Live
   Coding lock immediately took down `http://127.0.0.1:8000/mcp`). The GUI Editor is
   not left running by default (`CLAUDE.md`'s Environment section), and this repo's
   unattended dispatch model (cron, `MAX_PARALLEL=1`, nobody watching) has no
   mechanism to guarantee one is up when a validate-pr run fires.

**Open question, not decided here:** should the factory require a live Editor session
for validation to fully clear (i.e., PRs simply wait / land on `factory:needs-human`
whenever nobody's left the Editor open), or is there a lighter-weight independent
signal worth designing instead (e.g., a second, *differently-scoped* Automation test
group that the validator writes itself from the issue, blind to the builder's tests -
closer to the original holdout spirit without needing a live GUI at all)? Both are
legitimate; this is a product/process judgment call for a human, not something to
guess at while also trying to close out a documentation pass. Until decided:
`behavioral-e2e` stays at `not_e2e_testable`, `run-harness`'s real `GATE_OK` and the
other pass-1/pass-2 reviewers remain what actually gates a PR, and that is a correct,
intentional state - not a regression.
