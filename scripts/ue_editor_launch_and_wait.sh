#!/usr/bin/env bash
# Launches the real Unreal Editor GUI (not -Cmd.exe) against app/KrowdKontrol.uproject
# and waits for its MCP server to come up, so a workflow node can do genuine MCP-driven
# work (behavioral-e2e visual verification, or implement's content-creation path)
# without a human having to time opening the Editor themselves. See
# .factory/decisions.md D-013.
#
# "Auto Start Server" must be enabled in this project's Editor Preferences
# (Model Context Protocol section) - confirmed on 2026-08-16 - otherwise the server
# never starts on its own and this will time out waiting for it.
#
# Usage: ue_editor_launch_and_wait.sh [timeout_seconds]
#   Prints UE_EDITOR_READY on success (MCP responding), exit 0.
#   Prints UE_EDITOR_LAUNCH_TIMEOUT and exits 1 if MCP never comes up in time -
#   loud, not silent, per the harness's own philosophy (harness/README.md).

set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TIMEOUT_S="${1:-180}"
UE_EXE="${KROWD_KONTROL_UE_EXE:-/mnt/c/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe}"
MCP_URL="${KROWD_KONTROL_MCP_URL:-http://127.0.0.1:8000/mcp}"

if [ ! -L "$REPO_ROOT/app" ]; then
  echo "UE_EDITOR_LAUNCH_ERROR app/ is not a symlink - run scripts/link-unreal-project.sh first" >&2
  exit 1
fi
if [ ! -f "$UE_EXE" ]; then
  echo "UE_EDITOR_LAUNCH_ERROR UnrealEditor.exe not found at: $UE_EXE" >&2
  exit 1
fi

# Automation always takes precedence (2026-08-16 operator decision) - closing
# whatever's there, human session or a leftover automated one, guarantees a known,
# fresh state rather than reusing a session that might be stale or mid-something-else.
"$REPO_ROOT/scripts/ue_editor_close.sh"

UPROJECT_WIN="$(wslpath -w "$REPO_ROOT/app/KrowdKontrol.uproject")"

echo "UE_EDITOR_LAUNCHING $UPROJECT_WIN"
nohup "$UE_EXE" "$UPROJECT_WIN" >/dev/null 2>&1 &
disown

ELAPSED=0
POLL_INTERVAL=3
while [ "$ELAPSED" -lt "$TIMEOUT_S" ]; do
  # Check curl's own exit code explicitly rather than a shell `||` fallback -
  # some curl builds still print a placeholder via -w on connection failure, and
  # a `|| echo "000"` alongside that produces a concatenated, non-empty string
  # (e.g. "000000") that a naive != "000" check wrongly treats as a real response.
  # Confirmed live 2026-08-16: this exact bug produced a false "ready" at 0s
  # elapsed while UnrealEditor.exe had only just been launched.
  HTTP_CODE=$(curl -s -o /dev/null -w "%{http_code}" --max-time 2 "$MCP_URL" 2>/dev/null)
  CURL_EXIT=$?
  if [ "$CURL_EXIT" -eq 0 ] && [ -n "$HTTP_CODE" ]; then
    echo "UE_EDITOR_READY mcp_http_code=$HTTP_CODE elapsed=${ELAPSED}s"
    exit 0
  fi
  sleep "$POLL_INTERVAL"
  ELAPSED=$((ELAPSED + POLL_INTERVAL))
done

echo "UE_EDITOR_LAUNCH_TIMEOUT MCP did not respond at $MCP_URL within ${TIMEOUT_S}s - check 'Auto Start Server' is enabled in this project's Editor Preferences (Model Context Protocol section), and app/Saved/Logs/ for a crash." >&2
exit 1
