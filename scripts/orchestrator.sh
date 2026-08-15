#!/usr/bin/env bash
# The trigger. Pure bash, no LLM, on a cron. See FACTORY_RULES.md §8 for the
# priority order and safeguards this implements, and FACTORY.md component #2.
#
# Unlike dark-factory-experiment's orchestrator (a script living outside the repo
# on a VPS, deliberately stateless), this one runs on the operator's own machine
# and is kept in-repo for the same transparency reason — it holds no state of its
# own; everything it reads is visible in this repo's issues, PRs and labels.
#
#   */10 * * * * REPO_DIR/scripts/orchestrator.sh >> LOG_DIR/orchestrator.log 2>&1
#
# Requires: gh (authenticated), jq, archon (on PATH), git.

set -uo pipefail

cd "$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)" || exit 1

# Unattended auth: rides the machine's Claude Code subscription login by
# default (CLAUDE_USE_GLOBAL_AUTH=true in ~/.archon/.env — the same
# credential interactive use rides). Confirmed to work headless with no TTY
# and no ANTHROPIC_API_KEY set (2026-08-15) — see .factory/decisions.md D-002.
# Optional override below for anyone who wants a dedicated, metered Anthropic
# API key instead of subscription auth — see ~/.archon/orchestrator.env's own
# header for how.
ORCHESTRATOR_ENV="${FACTORY_ORCHESTRATOR_ENV:-$HOME/.archon/orchestrator.env}"
if [ -f "$ORCHESTRATOR_ENV" ]; then
  set -a
  # shellcheck disable=SC1090
  . "$ORCHESTRATOR_ENV"
  set +a
fi
if [ -n "${CLAUDE_API_KEY:-}" ]; then
  AUTH_MODE="dedicated API key ($ORCHESTRATOR_ENV)"
else
  AUTH_MODE="subscription login (global auth)"
fi

REPO="${FACTORY_REPO:-severinkehding/krowd-kontrol}"
# Default 1, not the scaffold's original 4: per-target locking only isolates this
# repo's own git worktrees, not the actual Unreal project (lives outside this repo —
# MISSION.md Hard Invariant 8), which is shared, unisolated state across any
# concurrent dispatch. Raise once that project is git-tracked and isolable per
# worktree — see FACTORY_RULES.md §8 and .factory/decisions.md D-003.
MAX_PARALLEL="${MAX_PARALLEL:-1}"
LOG_DIR="${FACTORY_LOG_DIR:-$HOME/.archon/logs/krowd-kontrol}"
mkdir -p "$LOG_DIR"

log() { echo "[$(date -u +%Y-%m-%dT%H:%M:%SZ)] $*"; }

log "Auth: $AUTH_MODE"

# Prove the auth mode above actually works right now, not just that it was
# selected — catches a lapsed subscription login (or a bad key override)
# before wasting a dispatch on work that would fail at its first Claude
# call anyway. Fails closed, same shape as the stop button below.
if AUTH_CHECK_OUTPUT=$(bash scripts/check-auth.sh 2>&1); then
  log "Auth check: OK"
else
  log "Auth check FAILED — exiting without dispatching."
  while IFS= read -r line; do log "  $line"; done <<< "$AUTH_CHECK_OUTPUT"
  exit 0
fi

# ─────────────────────────────────────────────────────────────────────────
# 1. THE STOP BUTTON — checked before anything else is read, fails closed.
# ─────────────────────────────────────────────────────────────────────────
if ! bash scripts/factory-stop.sh; then
  log "STOPPED — exiting without dispatching."
  exit 0
fi

# ─────────────────────────────────────────────────────────────────────────
# 2. IN-FLIGHT ACCOUNTING — per-target lock + MAX_PARALLEL cap.
# ─────────────────────────────────────────────────────────────────────────
# Parses running `archon workflow run ... --branch <name>` processes. The
# branch name IS the target identity (fix/issue-N, validate/pr-N, triage/*),
# so grepping ps output for it is a cheap, dependency-free lock.

in_flight_count() {
  pgrep -f "archon .*workflow run" 2>/dev/null | wc -l | tr -d ' '
}

is_locked() {
  local branch="$1"
  pgrep -af "archon .*workflow run" 2>/dev/null | grep -qF -- "$branch"
}

DISPATCHED_THIS_CYCLE=0

dispatch() {
  local workflow="$1" branch="$2" message="$3"

  local n
  n=$(in_flight_count)
  if [ "$n" -ge "$MAX_PARALLEL" ]; then
    log "SKIP (MAX_PARALLEL=$MAX_PARALLEL reached, $n in flight): $workflow $branch"
    return 1
  fi
  if is_locked "$branch"; then
    log "SKIP (locked, already in flight): $workflow $branch"
    return 1
  fi

  local logfile="$LOG_DIR/${branch//\//_}-$(date -u +%Y%m%dT%H%M%SZ).log"
  log "DISPATCH: archon workflow run $workflow --branch $branch \"$message\" (log: $logfile)"
  nohup archon workflow run "$workflow" --branch "$branch" "$message" \
    > "$logfile" 2>&1 &
  disown
  DISPATCHED_THIS_CYCLE=$((DISPATCHED_THIS_CYCLE + 1))
  return 0
}

capacity_left() {
  [ "$(in_flight_count)" -lt "$MAX_PARALLEL" ]
}

# ─────────────────────────────────────────────────────────────────────────
# 3. PRIORITY 1 — validate/fix PRs (oldest first). One workflow handles both
#    factory:needs-review and factory:needs-fix — dark-factory-validate-pr
#    folds fix + re-validate into a single dispatch (see FACTORY_RULES §8).
# ─────────────────────────────────────────────────────────────────────────

pr_queue() {
  {
    gh pr list -R "$REPO" --state open --label "factory:needs-review" \
      --json number,createdAt --jq '.[] | [.createdAt, .number] | @tsv' 2>/dev/null
    gh pr list -R "$REPO" --state open --label "factory:needs-fix" \
      --json number,createdAt --jq '.[] | [.createdAt, .number] | @tsv' 2>/dev/null
  } | sort -u | sort -k1,1 | cut -f2
}

while IFS= read -r pr_number; do
  [ -z "$pr_number" ] && continue
  capacity_left || break
  dispatch "dark-factory-validate-pr" "validate/pr-$pr_number" "Validate PR #$pr_number"
done < <(pr_queue)

# ─────────────────────────────────────────────────────────────────────────
# 4. PRIORITY 2 — implement accepted issues, highest priority first, not
#    already in-progress. Orchestrator stamps factory:in-progress itself on
#    dispatch (the workflow's own cleanup-issue-label node clears it later).
# ─────────────────────────────────────────────────────────────────────────

issue_queue() {
  gh issue list -R "$REPO" --state open --label "factory:accepted" \
    --json number,labels 2>/dev/null | jq -r '
      map(select((.labels | map(.name) | index("factory:in-progress")) == null))
      | map({number, prio: ((.labels | map(.name) | map(select(startswith("priority:"))) | .[0]) // "priority:low")})
      | map(. + {rank: ({"priority:critical":0,"priority:high":1,"priority:medium":2,"priority:low":3}[.prio] // 3)})
      | sort_by(.rank)
      | .[].number
    '
}

while IFS= read -r issue_number; do
  [ -z "$issue_number" ] && continue
  capacity_left || break
  if is_locked "fix/issue-$issue_number"; then
    log "SKIP (locked, already in flight): dark-factory-fix-github-issue fix/issue-$issue_number"
    continue
  fi
  gh issue edit "$issue_number" -R "$REPO" --add-label "factory:in-progress" 2>/dev/null || true
  dispatch "dark-factory-fix-github-issue" "fix/issue-$issue_number" "Fix issue #$issue_number"
done < <(issue_queue)

# ─────────────────────────────────────────────────────────────────────────
# 5. PRIORITY 3 — triage untriaged issues, last. Serializes with itself
#    (only one triage run at a time, ever) regardless of MAX_PARALLEL.
# ─────────────────────────────────────────────────────────────────────────

untriaged_count() {
  gh issue list -R "$REPO" --state open \
    --search '-label:factory:triaging -label:factory:accepted -label:factory:rejected -label:factory:needs-human -label:factory:in-progress -label:factory:rate-limited' \
    --json number --jq 'length' 2>/dev/null || echo 0
}

if capacity_left; then
  if pgrep -af "archon .*workflow run" 2>/dev/null | grep -qF "triage/"; then
    log "SKIP (triage already in flight — triage serializes with itself)"
  else
    N=$(untriaged_count)
    if [ "${N:-0}" -gt 0 ]; then
      dispatch "dark-factory-triage" "triage/$(date -u +%Y%m%dT%H%M%SZ)" "Triage open issues"
    fi
  fi
fi

log "Cycle complete. Dispatched $DISPATCHED_THIS_CYCLE workflow(s). In flight: $(in_flight_count)/$MAX_PARALLEL."
exit 0
