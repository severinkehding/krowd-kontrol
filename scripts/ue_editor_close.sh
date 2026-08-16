#!/usr/bin/env bash
# Forcibly closes any running Unreal Editor (GUI or crash-reporter) so a headless
# UnrealBuildTool compile or a fresh MCP launch can proceed without colliding with a
# live session. Automation always takes precedence over an interactive session left
# open (2026-08-16 operator decision) - this does not ask, it just closes.
#
# Idempotent: safe to call when nothing is running (reports and exits 0).
#
# Why force-kill rather than something gentler: no MCP tool exposes a graceful
# "quit" (see .archon/commands/dark-factory-fix-issue.md's MCP section and
# .factory/decisions.md D-011/D-013 for why - the MCP sandbox deliberately has no
# console-command-execution or arbitrary-method-invocation tool). taskkill /F has
# been used repeatedly this session with no observed corruption - Unreal's own
# autosave/recovery already assumes a session can die uncleanly.

set -uo pipefail

PIDS=$(tasklist.exe 2>/dev/null | grep -iE "^UnrealEditor(-Cmd)?\.exe|^CrashReportClientEditor" | awk '{print $2}')

if [ -z "$PIDS" ]; then
  echo "UE_EDITOR_CLOSE_OK already closed"
  exit 0
fi

for PID in $PIDS; do
  taskkill.exe /F /PID "$PID" >/dev/null 2>&1 || true
done

sleep 1
REMAINING=$(tasklist.exe 2>/dev/null | grep -iE "^UnrealEditor(-Cmd)?\.exe|^CrashReportClientEditor" | wc -l)
if [ "$REMAINING" -eq 0 ]; then
  echo "UE_EDITOR_CLOSE_OK closed $(echo "$PIDS" | wc -w) process(es)"
  exit 0
else
  echo "UE_EDITOR_CLOSE_ERROR $REMAINING process(es) still present after taskkill" >&2
  exit 1
fi
