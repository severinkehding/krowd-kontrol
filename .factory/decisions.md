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
