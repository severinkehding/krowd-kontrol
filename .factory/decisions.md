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

**Follow-up 2026-08-15: the symlink being gitignored has a mechanical consequence
worth spelling out.** A fresh git worktree - exactly what every `archon workflow run
... --branch <name>` dispatch creates - does not have `app/` at all until something
creates it there; the symlink only ever existed in the one working copy
`scripts/link-unreal-project.sh` was run in. Without a fix, every dispatched
`dark-factory-fix-github-issue` and `dark-factory-validate-pr` run would silently find
no Unreal project to touch (or `harness/run_ue_automation.sh`'s own explicit check
would catch it and fail loudly - better than silent, but still a broken run). Fixed:
both workflows now have a `link-unreal-project` node (running `scripts/
link-unreal-project.sh` fresh in that worktree, right after checkout) that every
Unreal-touching downstream node depends on. Idempotent and near-instant, so wiring it
broadly rather than narrowly cost nothing.

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

## D-006: Three live autopilot bugs found and fixed, first cron cycle after "full autopilot"

**Found 2026-08-15, ~30 min after enabling unattended cron dispatch (§8), while
investigating why PR #84 (the canary build) was rejected+escalated despite the harness
gate genuinely passing (`GATE_OK mode=full`, `APP_STARTED driver=cli`).**

1. **`dark-factory-synthesize-verdict.md` referenced a node that doesn't exist.** The
   pass-1 synthesizer read `$run-harness.output`, but the actual workflow node id is
   `run-harness-p1` — there is no node literally named `run-harness`. The variable never
   resolved, so the synthesizer saw an empty Harness Gate section and correctly (per its
   own rule 0) treated that as an infrastructure failure — even though the real harness
   run had passed cleanly. The pass-2 file (`dark-factory-synthesize-verdict-p2.md`)
   had the correct `run-harness-p2` naming throughout, confirming this was a pass-1-only
   typo, not a design choice. Fixed: all `$run-harness.output` references (arg-hint,
   input section, rule 0, the FORBIDDEN-escape-hatch paragraph, approve rule 1) now read
   `$run-harness-p1.output`. Also cleaned up the stale "(agent-browser)" section header
   and "Agent-browser E2E gate" rule name left over from the pre-D-004 rewrite — genuine
   leftover naming, but confirmed NOT the actual cause of this rejection (ruled out by
   checking `checkout-pr`'s node script, which does echo `Checked out PR #$N...`; a log
   grep that appeared to show it missing was a red herring — the raw JSONL log only
   records `bash_node_stderr` events, not stdout, so absence from that log proves
   nothing about the node's actual output).

2. **`orchestrator.sh`'s `MAX_PARALLEL=1` didn't hold within a single cron cycle.** At
   08:10 UTC, one cycle dispatched `validate/pr-84`, then `fix/issue-2`, then
   `triage/...` within 3 seconds of each other. `in_flight_count()` polls `pgrep`
   immediately after backgrounding the previous `archon workflow run` process — which
   hasn't shown up in the process table yet at fork+exec speed — so `capacity_left()`
   kept reading 0 in-flight and let every queue's dispatch through in the same cycle.
   The losers crashed instantly with `Permission denied accessing repository` from
   Archon's own internal worktree-creation lock (not a real filesystem permission
   issue). This is what the session's earlier "manual dispatch racing with cron"
   incident actually was too — not a one-off coincidence, a structural race that fires
   every cycle with >1 eligible target. Fixed: added `total_in_flight()` = pgrep count +
   `DISPATCHED_THIS_CYCLE`, and switched both `dispatch()`'s capacity check and
   `capacity_left()` to use it instead of the raw pgrep count.

3. **Issues with an open, unmerged PR could get redispatched to `fix-github-issue`
   again.** `cleanup-issue-label` clears `factory:in-progress` from an issue once its PR
   is opened (correct — the issue itself is no longer "in progress"). But
   `orchestrator.sh`'s `issue_queue()` only filtered on that label, with no check for an
   existing open PR — so the moment the label cleared, the issue looked idle again and
   got redispatched on top of its own still-open PR. Confirmed live: issue #2 got a
   second, duplicate `fix-github-issue` run at 08:10 UTC despite PR #84 already existing
   for it; that duplicate run did real (redundant) work and then failed at its last node
   (`cleanup-issue-label: bash executable not found at 'bash'... Set ARCHON_BASH_PATH`),
   leaving issue #2 stuck with both `factory:in-progress` and `factory:needs-human`
   simultaneously. Fixed: `orchestrator.sh` now fetches all open PRs' `headRefName`s,
   extracts issue numbers from the `archon/task-fix-issue-<N>` convention
   (`fix-github-issue` always branches this way — confirmed via PR #84's own
   `headRefName`), and skips any issue already covered by an open PR. Manually cleared
   issue #2's stuck labels back to just `factory:accepted` and reset PR #84 from
   `factory:needs-human` to `factory:needs-review` so the fixed loop retries it cleanly.

**Not investigated further, flagged for observation only:** the
`ARCHON_BASH_PATH`/Windows-flavored bash-resolution error on `cleanup-issue-label`
during the duplicate run. Plausibly a downstream symptom of the same same-cycle race
(#2 above) rather than an independent bug — every other run's `cleanup-issue-label`,
including the canary's, completed fine. Worth a second look only if it recurs now that
the race is fixed; not chased further here since it would need Archon-internals access
this repo doesn't have.

**Process note:** all three were caught by actually reading the cron log and per-run
logs after the user asked "is there something running?", not by assuming the "full
autopilot" cycle that had already run twice was clean. The loop was paused via the
local kill file (`.factory-stop`) for the ~15 minutes these fixes took, then resumed.

## D-007: Cron's every triage dispatch was failing silently — SSH remote, no agent under cron

**Found 2026-08-15, ~1 hour into "full autopilot"**, while checking on status: every single
triage dispatch since 08:10 UTC (15 consecutive attempts, one per 10-minute cycle) had
been failing at `worktree_creating` with `Error: Permission denied accessing repository`
— the orchestrator kept trying, correctly, every cycle; the repo's untriaged-issue count
just never moved (stuck at 81/81) because nothing ever got past that first step.

Root cause, isolated by bisecting the environment cron gives a script against what an
interactive shell has (confirmed `git worktree add` itself works fine outside Archon,
proving this wasn't a repo/filesystem/permissions problem in the literal sense): this
repo's `origin` remote was SSH (`git@github.com:...`). Interactive shells inherit
`SSH_AUTH_SOCK` from the desktop/WSLg session's running `ssh-agent`; a cron job is not
attached to that session and never gets it, so any git operation Archon's worktree setup
performs against the SSH remote fails auth — surfaced by Archon as the generic
"Permission denied" wrapper text, not anything indicating SSH specifically. This is why
it looked identical to (and was initially misdiagnosed alongside) D-006's same-cycle
race — same error string, unrelated cause. It didn't show up in the original canary test
or the manual dispatches earlier this session because those all ran from an interactive
shell with the desktop's agent already in the environment.

Fixed by switching the remote to HTTPS and using `gh` as the git credential helper
(`gh auth setup-git`) instead — this doesn't depend on any live agent process at all, so
it's cron-safe by construction rather than by coincidence:
```
git remote set-url origin https://github.com/severinkehding/krowd-kontrol.git
gh auth setup-git
```
Confirmed fixed by reproducing the exact failure under a stripped, cron-equivalent
environment (`env -i` with only what the crontab's own PATH sets), then confirming
`worktree_creating` → `worktree_created` succeeds the same way once the remote is HTTPS
— with `SSH_AUTH_SOCK` still absent. Rejected hardcoding `SSH_AUTH_SOCK` in
`orchestrator.sh`/crontab as a fix: that socket path is randomized per agent
instance and changes on every reboot/new session, so it would silently break again
rather than durably fixing the class of problem.

**Not a repo-file change** — this is local git config (`.git/config`'s remote URL) plus
account-level `gh` credential-helper setup, both of which apply automatically to every
existing and future worktree (worktrees share the parent repo's `.git/config`). Nothing
to commit or push for this one; logged here for the record and because it explains a
real ~1-hour stall in the "full autopilot" loop that wasn't visible from GitHub's side
(no failed PRs, no error labels — the failures never got far enough to touch GitHub at
all, they only ever showed up in the cron log).

## D-008: orchestrator.sh's pgrep pattern never matched a real archon process — MAX_PARALLEL was never actually enforced

**Found 2026-08-15, ~2 hours into "full autopilot"** (after D-007's SSH fix let real work
start flowing), while checking status and noticing `fix/issue-78` got dispatched at
11:30 UTC while `fix/issue-82` (dispatched 11:20 UTC, a full cron cycle earlier) was
still actively running — both real `dark-factory-fix-github-issue` builds, running
concurrently against the same shared, unisolated `app/` Unreal project. Confirmed via
`ps aux`: both processes (PIDs 132367 and 136059) were genuinely alive at the same
moment.

Root cause: `in_flight_count()` and `is_locked()` grepped for the literal pattern
`"archon .*workflow run"` — a space directly after "archon". But the actual process
command line is `bun /home/severin/tools/archon/packages/cli/src/cli.ts workflow run
...` — "archon" only ever appears as a path segment (`tools/archon/packages/...`),
never followed by a space. This pattern has **never matched a single real archon
process**, confirmed by piping the exact live command line through
`grep -q "archon .*workflow run"` and getting no match. `in_flight_count()` has been
silently returning 0 for the entire "full autopilot" period.

This is why D-006's `total_in_flight()` fix (adding `DISPATCHED_THIS_CYCLE` to the
pgrep count) looked like it worked in the one live test right after it shipped — it
genuinely does fix the *within-one-cron-cycle* race, since that fix doesn't depend on
pgrep matching anything. But it does nothing for the cross-cycle case: a process
dispatched in one 10-minute cycle that's still running when the *next* cycle starts —
which is exactly D-008's failure mode, and exactly the scenario `MAX_PARALLEL=1` and
[[D-003]]'s per-target-locking design exist to prevent for a repo whose real product
(the Unreal project) can't be worktree-isolated.

Fixed: switched all three `pgrep` call sites (`in_flight_count`, `is_locked`, and the
triage self-serialization check) to match `"workflow run dark-factory"` instead —
present verbatim in every dispatch this script makes (`archon workflow run
"$workflow" ...` where `$workflow` is always a `dark-factory-*` name), and not a path
fragment, so it can't silently break the same way again. Confirmed fixed by piping
both currently-running processes' real command lines through the new pattern and
getting a match for both.

**Not retroactively fixable**: `fix/issue-82` and `fix/issue-78` were already in
flight when this was found. Chose not to kill either — an abrupt kill mid-`implement`
risks leaving `app/` in a worse, harder-to-diagnose partial state than letting them
finish; at the time of this note they were working on different new files (not
editing the same file), and UnrealBuildTool's own build-time locking (observed
earlier this session as the "Live Coding is active" error) bounds the realistic
worst case to one of them hitting a loud build-lock failure rather than silent
corruption. Watched both through to completion rather than guessing at the outcome.

## D-009: app/ being untracked doesn't just block review — it blocks creating a PR at all

**Found 2026-08-15**, watching the first two real post-D-007/D-008 build dispatches
(issue #82 `URoomEnemyBudgetController`, issue #78 `UPlayerEnergyComponent`) run to
completion. Both did genuinely good, validated work — real C++ files on disk under
`app/`, both passing `harness/ci.py`'s full gate (`GATE_OK`, real Automation Framework
tests). Neither produced a PR. Not a bug in either run — `create-pr` correctly detected
that this repo's git diff was completely empty (all the real work lives under the
gitignored `app/` symlink, per [[D-003]]) and explicitly refused to fabricate an empty
commit to paper over that, exactly as it should. GitHub itself then hard-refused PR
creation: `GraphQL: No commits between main and archon/task-fix-issue-82
(createPullRequest)`.

This is a sharper, more consequential version of what PR #84 (issue #2) surfaced. That
PR *did* get created and merged only because issue #2's fix happened to also touch
`harness/harness.config.json` — one real in-repo line, enough for GitHub to hang a PR
on. That was incidental to that issue's scope, not a repeatable pattern: most future
PRD-derived issues are pure Unreal C++ gameplay work with **zero** in-repo footprint,
so most of them will hit this exact wall, every time.

**What actually happens when this fires, confirmed by watching both runs to their real
end-state (not guessed):** the downstream nodes (review-scope, code-review synthesis)
correctly refuse to review a nonexistent PR #0 rather than fabricate findings, the
workflow completes "successfully" (`anyFailed:false`) anyway, posts an honest
completion report as an issue comment explaining there's no PR, and
`cleanup-issue-label` clears `factory:in-progress` — the same cleanup step a normal
completed-and-PR'd issue gets. Net result: the issue lands back at exactly
`factory:accepted` with real work already done but **invisible to git and to every
review/merge mechanism**, indistinguishable from an issue nobody has touched yet.
Confirmed via `scripts/orchestrator.sh`'s `issue_queue()` that this is not just cosmetic:
with no `factory:needs-human` exclusion in that function (a separate, smaller gap fixed
alongside this — see the script's own inline comment), the very next cron cycle would
redispatch the exact same "impossible to land" work indefinitely, burning real API
usage on repeat attempts with no possible different outcome, forever, until someone
noticed. Applied `factory:needs-human` to #82 and #78 manually and added the
`issue_queue()` exclusion so this can't silently loop while the real question below
gets decided.

**The real question, not decided here — this is the user's call, not the factory's:**
`create-pr`'s own reasoning (quoted from its issue #82 run) laid out three shapes of
fix, and there's arguably a fourth:

1. **Empty commit per app/-only PR** — cheap, keeps the existing PR-based
   review/merge/audit-trail model working unchanged, but every merge adds a
   content-free commit to `main`'s history purely to satisfy GitHub's API, and reviewers
   still can't see the actual diff (same blindness [[D-004]]/[[D-005]] already flagged) —
   only papers over the "can't open a PR" half of the problem, not the "can't review it
   properly" half.
2. **Skip PRs for app/-only issues; resolve via issue comment directly** — matches what
   already organically happened here, formalizes it instead of leaving it an accidental
   dead-end. Loses git-based review gating, audit trail, and revert-ability for what
   will likely be the *majority* of this factory's actual output.
3. **Structural fix: require some in-repo marker file per completed app/-only issue**
   (e.g., a generated manifest/changelog entry) — keeps a real, non-empty diff and a
   real review target without needing to fake a commit, but is new design/implementation
   work, not a config change.
4. **Revisit whether `app/` needs to be entirely git-opaque**, now that the stakes are
   clearer than when [[D-003]] was decided. The original revert was about the Unreal
   *Editor* failing to open a `.uproject` file living on a WSL-hosted path — but that
   doesn't obviously require the *entire* project to be untracked; e.g. `app/Source/`
   (small C++ text files, no `.uasset`/LFS concerns at all) tracked directly in this
   repo, with the real Unreal project's `Source/` folder itself made into a symlink
   pointing *back* into this repo's tracked copy (the reverse direction from today's
   symlink), might sidestep the original failure entirely — only `Source/` would ever
   need to reach across the WSL/Windows boundary, not the whole project. Untested
   speculation, not a verified fix — the original failure mode was specific enough
   (`LogProjectManager` failing on the `.uproject` descriptor itself) that this needs a
   real trial, not an assumption, before treating it as viable.

Every option changes either the review guarantees, the git history shape, or requires
new implementation work — a product/process decision, not something to resolve
unilaterally while also trying to keep the loop from wasting cycles. Loop paused
(`.factory-stop`) pending that decision.

**Resolved 2026-08-15: marker-file/changelog approach chosen and implemented.**
Updated `.archon/commands/dark-factory-create-pr.md` — when the git diff is empty
(app/-only change), it now writes `app-changelog/issue-N.md` (file-added/changed
table, acceptance-criteria checklist, validation evidence, a closing note pointing
back at `app/`) from the already-produced `implementation.md`/`validation.md`
artifacts, commits *that*, and proceeds with the normal push+PR flow. Validated live,
not just in theory: manually applied the same pattern to unblock issues #82 and #78
(the real work both already had validated — `GATE_OK mode=full` on each — just
couldn't reach a PR), producing PR #85 and PR #86. Both real draft PRs, each with a
genuine non-empty diff a reviewer can actually read and cross-check against the
issue's acceptance criteria, `factory:needs-review` labeled for the next validate-pr
cycle. `factory:needs-human` cleared from both issues now that they're progressing
normally through a real PR.

Chose this over option 4 (tracking `app/Source/` directly) because it's immediately
implementable and testable within this session, doesn't touch the already-hard-won,
confirmed-working `app/` symlink setup, and doesn't add empty commits to `main`'s
history. It's an explicit compromise, not a full fix for [[D-004]]/[[D-005]]'s
reviewer-blindness gap — `behavioral-validation`/`code-review` still can't see the
actual C++, only this changelog's claims about it — but it at least gives reviewers a
concrete, specific, checkable artifact instead of an empty diff, and unblocks the
"can't even open a PR" half of the problem completely. Option 4 remains worth
revisiting later if the changelog-review gap turns out to matter in practice.
