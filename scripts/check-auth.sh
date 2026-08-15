#!/usr/bin/env bash
# Verifies the orchestrator's unattended auth path actually works right now —
# not which mode scripts/orchestrator.sh *selected* (that's a static string
# check), but whether a headless, no-TTY Claude Code invocation can actually
# authenticate and get a real response. Cheap: one Haiku call, ~10 tokens.
#
# Called automatically once per orchestrator.sh cycle (see there) so a
# lapsed subscription login shows up in cron.log within 10 minutes instead
# of silently doing nothing until someone notices. See FACTORY.md component
# #2 and .factory/decisions.md D-002 for why this exists.
#
# Usage: bash scripts/check-auth.sh
# Prints AUTH_OK or AUTH_FAILED to stdout either way (so a `log()` caller
# can echo it straight through); exits 0 on success, 1 on failure with the
# CLI's own error output on stderr.

set -uo pipefail

OUTPUT=$(env -u ANTHROPIC_API_KEY -u ANTHROPIC_AUTH_TOKEN \
  claude -p "Reply with exactly: OK" --model haiku 2>&1 < /dev/null)
STATUS=$?

if [ "$STATUS" -eq 0 ] && [ "$(echo "$OUTPUT" | tr -d '[:space:]')" = "OK" ]; then
  echo "AUTH_OK"
  exit 0
fi

echo "AUTH_FAILED"
{
  echo "check-auth.sh: headless subscription login did not respond as expected."
  echo "--- claude output (exit $STATUS) ---"
  echo "$OUTPUT"
  echo "-------------------------------------"
  echo "If this is an auth error, run 'claude /login' interactively to refresh."
  echo "To fall back to a dedicated API key instead, see ~/.archon/orchestrator.env."
} >&2
exit 1
