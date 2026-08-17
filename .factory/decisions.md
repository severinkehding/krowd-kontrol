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

**Correction to the above, same day:** the changelog-only approach was proven
insufficient the very first time it ran for real, not just theoretically weaker.
`behavioral-validation` is a pure diff-reader by design (`allowed_tools: []`, holdout
discipline) — PR #85's diff contained only `app-changelog/issue-82.md`'s prose, which
from that reviewer's perspective is indistinguishable from a coder claiming work was
done with nothing to back it up. It correctly returned `solves_issue: "no"` with high
confidence and PR #85 was rejected and closed. Worse: `synthesize-verdict`'s reject
path re-queues the issue back to plain `factory:accepted` with no escalation, so left
running the loop would have redispatched the identical, doomed-to-repeat work
indefinitely.

**Actual fix, same day:** `create-pr` now copies the real `.h`/`.cpp`/`.Build.cs` file
*contents* (per `implementation.md`'s Files Changed table) into a tracked
`app-source-tracked/` mirror, alongside the (now secondary, context-only) changelog.
`app/` itself is untouched — still a symlink, still what the Editor/harness build
against — this is a plain-text copy for review purposes, not a live link, so it can't
reopen the WSL/Editor cross-boundary failure that made `app/` untracked in the first
place, and it never touches `.uasset`/`.umap`/`Content/`/`Binaries/`/`Intermediate/`,
so none of the original size/LFS concerns apply either. Every existing reviewer just
works on a real diff now — no new pre-fetch node, no per-reviewer special-casing.
Manually applied to both already-rejected/pending PRs (#85, #86), confirmed via `gh pr
view --json files` that both diffs now contain the real source, reopened #85 and
re-labeled both `factory:needs-review` for the loop to re-validate for real.

## D-010: Same $run-harness naming bug from D-006, unfixed in a second file

**Found 2026-08-15**, checking PR #86's status after it landed on `factory:needs-human`
with the exact same verbatim boilerplate rejection text D-006 fixed for PR #84
("Validator prerequisites failed or no app exists yet; cannot render a substantive
verdict"). D-006 fixed `dark-factory-synthesize-verdict.md`'s stale
`$run-harness.output` reference, but `dark-factory-behavioral-e2e.md` had the exact
same bug (I'd actually already noticed and written down the stale reference while
investigating D-006, but only fixed the file I was looking at, not the one the note
itself pointed at). Confirmed live: PR #86's log shows 5
`dag_node_output_ref_unknown_node` warnings for `$run-harness.output`, and that run's
`behavioral-e2e-p1` reported `app_booted: false` with reasoning citing no harness
result being provided — despite the harness gate genuinely passing
(`GATE_OK mode=full`, `UNIT_PASSED tests=3`, per the PR body). `app_booted == false`
is unconditionally not approve-compatible per `synthesize-verdict`'s own rules, so
this alone routes to the same infrastructure-failure rejection.

Unlike `synthesize-verdict-p1`/`synthesize-verdict-p2` (genuinely separate files),
`behavioral-e2e-p1` and `behavioral-e2e-p2` share one command file
(`dark-factory-behavioral-e2e.md`) — depends_on `run-harness-p1` and `run-harness-p2`
respectively. A single hardcoded suffix can't be correct for both, so this file now
references both `$run-harness-p1.output` and `$run-harness-p2.output` and instructs
the reviewer to use whichever one actually resolves for that pass — the same pattern
`apply-verdict`'s bash node already uses correctly for this exact shared-file problem.

**Standing lesson, not just this one bug**: when a fix touches one AI command file
because of a variable-reference bug, grep the entire `.archon/commands/` directory for
the same broken pattern before considering it closed — this is the second time the
identical typo class caused a real rejection because only one of two affected files
got fixed. Reset PR #86 from `factory:needs-human` back to `factory:needs-review` so
the fixed pipeline retries it.

## D-011: Best-effort MCP wiring for the automated loop (closes part of D-005)

**2026-08-16.** Wired `mcp:` config into the workflow nodes that can actually use it,
now that D-005's "Editor open + headless build collide" question has a real answer
(tested live: they do collide — `Unable to perform hot reload with multiple targets`
— even with Live Coding disabled; not resolved, just now confirmed rather than
assumed). Given that, the operator's own call: keep the Editor open when doing
interactive/MCP work, pause the loop (`.factory-stop`, now self-expiring — see below)
while it's open, resume when done. This makes MCP **best-effort** for automated
dispatches specifically — reachable only when the operator happens to have left it
open — not a guaranteed capability. That's an accepted tradeoff, not a gap to close
further right now.

Wired:
- `implement` (`dark-factory-fix-github-issue.yaml`) — `.archon/mcp/unreal-and-blender.json`
  (new, merges both existing per-tool configs). For issues needing Editor-only content
  (levels, scene/actor placement, imported assets) that can't be produced as text.
  `dark-factory-fix-issue.md` now has an explicit "attempt first, report honestly if
  unreachable" procedure (§5.2a) — codifies the exact good behavior issue #56/PR #99
  already demonstrated on its own (self-reporting the missing level as
  "environment-blocked" rather than faking one) as the standing pattern, not
  something to leave to chance each time.
- `behavioral-e2e-p1`/`behavioral-e2e-p2` (`dark-factory-validate-pr.yaml`) —
  `.archon/mcp/unreal.json`. `dark-factory-behavioral-e2e.md` rewritten so Phase 1
  (real MCP visual verification) is attempted first on every run, with Phase 0 (the
  honest "not independently verified" report) now explicitly the fallback for a
  failed connection attempt, not the default path.

**Related, same day: `scripts/factory-stop.sh` now self-expires a local kill file
after 4 hours** (`FACTORY_KILL_FILE_MAX_AGE_S`) — directly motivated by this pattern
(pause the loop, leave the Editor open, resume when done) needing a guarantee that a
forgotten/crashed pause can't silently halt the loop indefinitely. See
`FACTORY_RULES.md`'s stop-button section for the full account; tested all three code
paths (fresh stop, expired stop, no stop) directly before trusting it.

**Not attempted**: a self-managing mechanism where the workflow itself launches/closes
the Editor around MCP-needing steps. Given the confirmed build collision, that would
also need to coordinate against concurrent headless builds — real orchestration work,
not something to build reactively while chasing a single PR's blocker. Left as a
bigger future option if best-effort turns out to be too unreliable in practice.

## D-012: The pipeline-wide GATE_FAILED bug was a regex/literal mismatch, not concurrency

**2026-08-16.** `KrowdKontrol.Unit.StationPowerUpComponent` had been failing
deterministically — 100% reproducible, every run, regardless of whether the Editor
was open or closed — since issue #60 first landed it. Two different PRs (mine
manually testing, and the automated loop's own PR #100) both hit it and both guessed
wrong causes: PR #100's build attributed it to a concurrent-Editor DLL/hot-reload
collision (a real phenomenon this session independently confirmed exists for actual
*builds*, per D-011, but not what was happening here). Because `harness/ci.py`'s
`GATE_OK` is a hard requirement for every PR regardless of what it touches, this one
broken, unrelated test was silently on track to block every future PR indefinitely.

**Actual root cause, confirmed by reading this engine version's own header
(`AutomationTest.h`) rather than guessing from memory:** `FAutomationTestBase::
AddExpectedError`'s `IsRegex` parameter **defaults to `true`** — patterns are regular
expressions unless told otherwise. The test's
`AddExpectedError(TEXT("OrderedLights[1] is null"), ..., 1)` call never passed that
4th argument, so `[1]` was interpreted as a regex character class (matching a single
literal `1`, no brackets) rather than literal text — it could never match the actual
logged string, which contains literal `[` `]` characters. The sibling call
(`"OrderedLights is empty"`, no bracket/regex metacharacters) worked by coincidence,
since a bracket-free string means identical behavior under literal or regex
interpretation — which is exactly why only one of the two calls failed and why the
failure was rock-solid deterministic rather than flaky.

Fixed by passing `IsRegex = false` explicitly on both calls (matching what both
were actually written to mean — plain substrings, not patterns), confirmed via a
real rebuild + full harness run: `GATE_OK mode=full`, `UNIT_PASSED tests=16` (up
from 15, the previously-permanently-failing test now genuinely passes). Synced the
tracked mirror to match.

**Process note:** this is the second time this session an AI-authored PR's own
plausible-sounding root-cause diagnosis (D-011's "concurrent Editor" theory) turned
out to be wrong on independent verification, not because the reasoning was
unreasonable given what that PR's own dispatch could observe, but because a
deterministic, code-level bug can look identical to an environmental one from a
single test run. Worth remembering next time a PR blames a failure on "the shared
environment" rather than the code: reproduce it in isolation before trusting that
framing.

## D-013: Self-managed Editor lifecycle — closes D-011's "best-effort" gap for real

**2026-08-16.** D-011 wired MCP into `implement` and `behavioral-e2e` but left it
best-effort: reachable only if the operator happened to have the Editor open with
the right timing. Called out directly by the operator as not actually autonomous —
correct. "Close it when not using MCP" just moves the coordination burden onto a
human instead of removing it.

**Decision: automation always takes precedence.** If a live Editor session (human-
launched or a leftover from a prior dispatch) is in the way of what the workflow
needs to do next, it gets force-closed, no negotiation. This is what makes a fully
scripted lifecycle possible instead of another best-effort layer.

**What got built, tested live against the real project before being trusted:**

- `scripts/ue_editor_close.sh` — force-closes any `UnrealEditor.exe`/
  `UnrealEditor-Cmd.exe`/`CrashReportClientEditor.exe`, idempotent (`UE_EDITOR_CLOSE_OK`
  either way). No MCP tool exposes a graceful quit (confirmed: the sandbox
  deliberately has no console-command-execution or method-invocation tool), so this
  is `taskkill /F` — already proven safe across many uses this session.
- `scripts/ue_editor_launch_and_wait.sh [timeout_s]` — closes first (same reasoning),
  launches `UnrealEditor.exe` (the real GUI, not `-Cmd.exe`) against
  `app/KrowdKontrol.uproject`, polls the MCP endpoint until it genuinely responds or
  times out loudly (`UE_EDITOR_LAUNCH_TIMEOUT`, not silent). Depends on "Auto Start
  Server" being enabled in this project's Editor Preferences (confirmed on,
  2026-08-16) — without it the server never comes up on its own and this always
  times out.
  - **Found and fixed a real bug in this script before trusting it**: the readiness
    poll's `curl ... || echo "000"` fallback could concatenate with curl's own
    placeholder output on a connection failure, producing `"000000"` — which a naive
    `!= "000"` check wrongly treated as a real response, reporting `UE_EDITOR_READY`
    at 0 seconds elapsed while the Editor had only just been launched. Fixed by
    checking curl's actual exit code explicitly. Confirmed the fix live: 9s to a
    genuine `405` response, verified independently via a real
    `mcp__unreal-mcp__list_toolsets` call, not just trusting the script's own report.
  - Also directly confirmed the launched window is real and visible on the operator's
    desktop (not a background/headless process that merely happens to serve MCP) —
    checked after the operator reported not having seen it appear during an earlier,
    very fast launch→verify→close test cycle; a slower, left-open relaunch confirmed
    it was there the whole time, just missed in the short window.

**Wired into both workflows:**

- `dark-factory-validate-pr.yaml`: `ensure-editor-closed-p1/p2` before
  `run-harness-p1/p2` (so the headless UBT compile never fights a live session);
  `launch-editor-for-e2e-p1/p2` after `run-harness-p1/p2` and before
  `behavioral-e2e-p1/p2` (so E2E gets a session that's actually current, not stale —
  this is what PR #101 hit); `close-editor-after-e2e-p1/p2` as best-effort cleanup;
  `teardown-app` (already ran unconditionally at the end regardless of pass-1/pass-2
  path) now also force-closes as the hard guarantee. Also removed a dead
  `agent-browser close` call left over from before D-004.
- `dark-factory-fix-github-issue.yaml`: `ensure-editor-closed` before `implement`
  (same build-collision reasoning). `implement` itself stays on-demand rather than
  auto-launched — most issues never touch Editor-only content, and launching
  unconditionally would cost ~10-15s+ of dead time on all of them for no benefit.
  `dark-factory-fix-issue.md` §5.2a now tells it exactly which scripts to call and
  when, including the explicit requirement to close the Editor itself before
  finishing so it doesn't block the very next headless build.

**What this doesn't solve:** two humans/processes wanting the Editor at the same
moment still can't both have it — automation wins, full stop, per the decision
above. If the operator is using it interactively when the loop needs it, it will be
closed out from under them. Acceptable given the explicit instruction, but worth
remembering if it ever surprises someone mid-session.

## D-014: D-013's close step was a silent no-op under cron — bare tasklist.exe/taskkill.exe don't resolve

**2026-08-16.** Discovered live while watching PR #102's `validate-pr` run at the
operator's request ("observe and see if it still crashes"). It didn't crash — it did
something quieter and worse: `ensure-editor-closed-p1` reported `UE_EDITOR_CLOSE_OK`
in 12ms, and `launch-editor-for-e2e-p1` reported ready in 295ms, both suspiciously
fast for genuine tasklist/taskkill/cold-Editor-boot work. Checked Windows process
start times directly (`Get-Process ... StartTime`): two `UnrealEditor.exe` processes
were running concurrently — one started ~12:21 PM (the operator's own manually-
launched session, from an earlier "starting unreal now" test), one started ~13:39 PM
(this workflow's own `launch-editor-for-e2e-p1` spawn) — neither had killed the
other.

**Root cause:** `scripts/orchestrator.sh`'s crontab entry sets an explicit `PATH`
(added to fix `bun`/`gh` resolution under cron — see the orchestrator PATH note
elsewhere in this log) that excludes any Windows directory. `ue_editor_close.sh`
called `tasklist.exe`/`taskkill.exe` bare. WSL's default Windows-PATH interop only
supplies those when the shell's own `$PATH` includes something like
`/mnt/c/Windows/System32` — true in an interactive shell, **false** under cron's
override. Every invocation resolved to "command not found" (exit 127), swallowed by
`2>/dev/null`, leaving `PIDS` empty — so the script unconditionally printed
`UE_EDITOR_CLOSE_OK already closed` regardless of what was actually running. All of
today's D-013 testing that looked clean was run interactively (this operator's own
Bash tool calls, full PATH) and never exercised the actual cron code path — the bug
was invisible until a real dispatch hit it.

**Consequence:** every `ensure-editor-closed*`/`close-editor-after-e2e*` node across
every real dispatch since D-013 landed has been a false-success no-op. Concretely for
PR #102: `behavioral-e2e-p1`'s 367-second holdout review almost certainly connected
via MCP to the operator's long-running manual session (over an hour of uptime,
definitely warm) rather than a fresh session matching this PR's actual build — the
verdict it produced should not be trusted as evidence about PR #102 specifically,
independent of whether its conclusion happened to be right.

**Fix:** `ue_editor_close.sh` now calls `/mnt/c/Windows/System32/tasklist.exe` and
`/mnt/c/Windows/System32/taskkill.exe` by absolute path (overridable via
`KROWD_KONTROL_TASKLIST_EXE`/`KROWD_KONTROL_TASKKILL_EXE`), matching the convention
`ue_editor_launch_and_wait.sh` already used for `UE_EXE`. Verified by re-running the
script under `env -i PATH="<the exact cron PATH>"`: before the fix, `command not
found`; after, it correctly found and killed both orphaned `UnrealEditor.exe`
processes. Committed to `main` (`aa73c37`) and pushed.

**Process note, same shape as D-012's:** a script that "worked" in every manual test
this operator ran can still be dead code under the one environment that actually
matters (cron). Interactive verification of an automation script is not verification
of the automation — the only real test is watching it run for real, which is
precisely what this operator's "observe and see if it still crashes" request forced.
Don't treat a clean interactive dry run of an unattended script as sufficient
evidence again without also checking it under the actual restricted invocation
environment (PATH, env, cwd) it will really run under.

## D-015: A rejected/closed PR's leftover worktree could permanently stick an issue in-progress

**2026-08-16.** Found during a routine "any failed/stuck?" status sweep (operator
request), not a crash — the opposite: silence. Issues #55 and #74 both showed
`factory:in-progress` with timestamps ~5+ hours stale, no open PR, and no active
process. `issue_queue()` already excludes `factory:in-progress` and
`factory:needs-human` (D-009's fix) — neither applied here, since nothing had ever
labeled these `factory:needs-human`, and nothing had ever cleared `in-progress`
either. Genuinely invisible: not flagged as needing a human, not eligible for
redispatch, not in flight.

**Root cause:** `dark-factory-validate-pr.yaml`'s `checkout-pr` step already prunes a
stale worktree pinned to the PR branch (left behind by a preceding
`fix-github-issue` run) before checking the PR out — see the existing comment there
citing `dark-factory-experiment`'s rationale. Nothing did the reverse. `validate-pr`'s
own `checkout-pr` worktree (`task-validate-pr-100`, `task-validate-pr-89`) stays
pinned to `archon/task-fix-issue-{55,74}` after that validate-pr run finishes —
whether the PR merges, gets rejected, or gets closed — with no teardown step for the
worktree itself. A later `fix-github-issue` redispatch on the same issue then fails
at `git worktree add` (`fatal: 'archon/task-fix-issue-55' is already checked out at
'.../task-validate-pr-100'`) before a single DAG node runs — before it can even
reach `cleanup-issue-label`. The label set at dispatch time (`factory:in-progress`)
never gets cleared, and the process that would have cleared it never existed long
enough to try.

**Immediate fix:** manually removed both stale worktrees (uncommitted leftovers
stashed first, not discarded — see D-013's investigation for what those leftovers
actually were), cleared both `factory:in-progress` labels by hand.

**Systemic fix:** `scripts/orchestrator.sh`'s `dispatch()` now prunes any worktree
pinned to `archon/task-<branch>` immediately before launching, mirroring
`checkout-pr`'s existing pattern. Safe specifically because `is_locked()` already ran
first in `dispatch()` — anything still pinned to the branch at that point is
guaranteed stale, not a live collision with a real in-flight run. Uses `--force`
(matching `checkout-pr`'s own precedent) since this runs unattended under cron with
no human present to stash first, and anything sitting uncommitted in a *finished*
validate-pr worktree is disposable review-run byproduct, not real work — the
branch's actual committed history is untouched by removing the worktree.

**Pattern note:** this is the third distinct "an issue's own successful-looking
history quietly stops it from ever being touched again" bug found today (see D-009,
and the earlier same-session `-vN` branch-suffix dispatch bug), each with a different
root cause but the same shape: something that looks like an ending (workflow exits,
label update, PR closes) doesn't actually reach the step that was supposed to signal
it. Worth treating "is anything stuck with no error and no visible cause" as its own
recurring category to sweep for, not just individual bugs to fix one at a time.

## D-016: `harness/run_ue_automation.sh` never rebuilds — a "green gate" can mean the
PR's own source was never compiled

**2026-08-17.** Found while self-fixing PR #125's review findings (issue #122). One
finding asked to re-verify a suspect test case by actually running `harness/ci.py`
full mode. Doing that for real (not just re-reading the changelog's claimed output)
surfaced that `harness/run_ue_automation.sh` launches `UnrealEditor-Cmd.exe` directly
against whatever `Binaries/Win64/UnrealEditor-KrowdKontrol.dll` already exists on
disk — it never invokes UnrealBuildTool. Checked timestamps: that DLL was last built
09:56 that morning, a full hour before PR #125's own commit (11:07). Its
"`UNIT_PASSED tests=31`"/"`GATE_OK mode=full`" changelog claim was real output from a
real run, but of a binary that predated the PR's own source changes entirely — the
gate was green because it validated old code, not because the new code worked.

Manually invoking `Engine/Build/BatchFiles/Build.bat` (via
`/mnt/c/Windows/System32/cmd.exe`, callable from WSL through interop; needed a
temp-`.bat` wrapper file since inline `cmd /c "..."` quoting kept mis-splitting on
`C:\Program Files\...` paths) rebuilt for real and immediately surfaced two genuine
bugs in the PR's own test additions that neither the harness nor the prior code
review had caught, because neither had ever actually compiled this source:
1. `TestEqual(..., FVector::Dist(...), 0.0f)` — `FVector::Dist` returns `double`;
   comparing it directly against a `float` literal is an ambiguous overload (`C2666`)
   between `TestEqual`'s `FString`/`TCHAR*` parameter forms. Every one of this PR's
   new test cases using this exact shape failed to compile.
2. `UGameplayStatics::GetPlayerPawn` needs the test's manually-spawned
   `APlayerController` registered in `World::PlayerControllerList`, which normally
   happens via `AController::PostInitializeComponents` — `CreateNewMap()`'s editor
   world never drives that pass, so `Controller->Possess(Pawn)` alone (the fix a
   code-review pass had recommended, citing an existing sibling test as precedent)
   was not sufficient; a direct `World->AddController()` call was required. Compounding
   it, the raw `APawn` used as "the player" has no default `RootComponent`, so
   `SetActorLocation()` on it was a silent no-op the whole time.

**Separately, and higher-stakes:** `app/` is a single symlink to one physical
directory on the Windows host (`CLAUDE.md`'s Environment section), shared by every
worktree that links it — not scoped per-branch, per-PR, or per-worktree. Mid-fix,
removing this PR's accidentally-bundled `IThreatState`/`GetThreatState()` code (see
PR #125's own review findings) broke the live build, because `MusicSubsystem.cpp/.h`
and its test — real, in-progress work for issue #25, sitting only as uncommitted
files on the shared filesystem, in no branch, no commit, anywhere in this repo's git
history — already depended on it. The two tasks were never in the same worktree or
branch; they collided purely because `app/` is shared mutable state. Resolved this
time by restoring the live `app/EnemyBase.h`/`.cpp` to keep that code (protecting
issue #25's in-flight work) while keeping the git-tracked `app-source-tracked/`
mirror correctly scoped without it — an intentional, documented divergence between
the two, not a mistake. This is the same concurrency-isolation problem D-003/D-004
already named as unsolved; this is a second, concrete instance of it actually
destroying a colleague's uncommitted work, not just a theoretical risk.

**Not fixed here** (out of scope for a PR-review self-fix; both need deliberate
follow-up):
- `harness/run_ue_automation.sh` (or `ci.py`/`harness.config.json`) should invoke a
  real build before running Automation tests, or the gate should refuse to claim
  `GATE_OK` off a binary older than the source it's meant to validate.
- `app/`'s shared-filesystem concurrency problem needs an actual fix (per-worktree
  checkout of the live project, a lock, or similar) — CLAUDE.md's `app-source-tracked/`
  carve-out (D-009) solved the "can't open a PR" symptom but not this one.
