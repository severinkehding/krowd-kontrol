#!/usr/bin/env bash
# Runs Unreal's Automation Framework headlessly against app/KrowdKontrol.uproject and
# reports pass/fail — on stdout (for appproc.py's smoke_contains / a future
# unit_count_pattern) and via exit code (0 = every matched test passed).
#
# Usage:
#   harness/run_ue_automation.sh --check-exe-only
#       Fast path (<1s, no engine launch): just confirms UnrealEditor-Cmd.exe exists.
#       This is what harness.config.json's cli.smoke_args uses — appproc.py's CliApp
#       hardcodes a 60s timeout on its smoke check with no way to configure it (that
#       file stays untouched, per harness/README.md), and a real Editor boot does not
#       reliably fit inside that window. Verified empirically: a bare `-version`
#       invocation alone took long enough to need killing — see
#       .factory/decisions.md D-004.
#   harness/run_ue_automation.sh <TestNameFilter>
#       Real path: boots the Editor headlessly, runs every Automation test whose name
#       starts with <TestNameFilter> (e.g. "KrowdKontrol.Smoke.", later
#       "KrowdKontrol.Unit."), and reports the result. This is what harness/e2e.py
#       calls, with its own generous, configurable timeout — not the smoke_args path.
#
# Paths resolve dynamically (wslpath, following app/'s symlink) rather than hardcoding
# this machine's personal path into a committed config file — see
# scripts/link-unreal-project.sh, CLAUDE.md's Environment section, and
# .factory/decisions.md D-003/D-004.

set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
FILTER="${1:?usage: $0 <--check-exe-only | TestNameFilter>}"
UE_CMD="${KROWD_KONTROL_UE_CMD_EXE:-/mnt/c/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe}"

if [ "$FILTER" = "--check-exe-only" ]; then
  if [ -f "$UE_CMD" ]; then
    echo "UE_CMD_PRESENT $UE_CMD"
    exit 0
  fi
  echo "UE_CMD_MISSING $UE_CMD" >&2
  echo "Set KROWD_KONTROL_UE_CMD_EXE if the engine is installed elsewhere." >&2
  exit 1
fi

if [ ! -L "$REPO_ROOT/app" ]; then
  echo "UE_AUTOMATION_ERROR app/ is not a symlink — run scripts/link-unreal-project.sh first" >&2
  exit 1
fi
if [ ! -f "$UE_CMD" ]; then
  echo "UE_AUTOMATION_ERROR UnrealEditor-Cmd.exe not found at: $UE_CMD" >&2
  echo "Set KROWD_KONTROL_UE_CMD_EXE if the engine is installed elsewhere." >&2
  exit 1
fi

UPROJECT_WIN="$(wslpath -w "$REPO_ROOT/app/KrowdKontrol.uproject")"

# Rebuild the module before testing (2026-08-17). Nothing in this gate compiled
# before — it silently tested whatever DLL the last build (from ANY branch's run)
# left behind, which produced three confirmed-false E2E/validation verdicts in one
# day: PR #126 was escalated for a component "missing from the loaded module" that
# was simply not in the stale binary, and PRs #120/#125 had findings partially
# rooted in observing another branch's build. An up-to-date build is a no-op
# (~8-10s with UBA); a stale one gets corrected right here, so the tests below
# always run against the source actually on disk. Skippable for callers that just
# built (KROWD_KONTROL_SKIP_UBT=1), and a failed compile is a loud gate failure —
# exactly what "loud, never silent" demands.
if [ "${KROWD_KONTROL_SKIP_UBT:-0}" != "1" ]; then
  UBT_DOTNET="${KROWD_KONTROL_UBT_DOTNET:-/mnt/c/Program Files/Epic Games/UE_5.8/Engine/Binaries/ThirdParty/DotNet/10.0/win-x64/dotnet.exe}"
  UBT_DLL="${KROWD_KONTROL_UBT_DLL:-C:\\Program Files\\Epic Games\\UE_5.8\\Engine\\Binaries\\DotNET\\UnrealBuildTool\\UnrealBuildTool.dll}"
  if [ -f "$UBT_DOTNET" ]; then
    echo "UE_BUILD_START KrowdKontrolEditor Win64 Development"
    if ! "$UBT_DOTNET" "$UBT_DLL" KrowdKontrolEditor Win64 Development -project="$UPROJECT_WIN" -WaitMutex >/dev/null 2>&1; then
      echo "UE_AUTOMATION_ERROR UnrealBuildTool compile failed — tests would run against a stale binary. Re-run the build directly for the compiler output." >&2
      exit 1
    fi
    echo "UE_BUILD_OK"
  else
    echo "UE_BUILD_SKIPPED dotnet not found at $UBT_DOTNET (set KROWD_KONTROL_UBT_DOTNET) — testing existing binaries" >&2
  fi
fi

REPORT_DIR="$(mktemp -d)"
trap 'rm -rf "$REPORT_DIR"' EXIT
REPORT_DIR_WIN="$(wslpath -w "$REPORT_DIR")"

# -nullrhi: no rendering, fine for Smoke/Unit tests (pure logic, no world). A future
# Screenshot.* test group needs real rendering and must NOT pass -nullrhi — branch on
# $FILTER here once that group exists rather than editing this unconditionally.
"$UE_CMD" "$UPROJECT_WIN" \
  -ExecCmds="Automation RunTests $FILTER; Quit" \
  -unattended -nopause -nosplash -nullrhi \
  -testexit="Automation Test Queue Empty" \
  -ReportOutputPath="$REPORT_DIR_WIN" \
  -log >/dev/null 2>&1

REPORT="$REPORT_DIR/index.json"
if [ ! -f "$REPORT" ]; then
  echo "UE_AUTOMATION_ERROR no report produced at $REPORT_DIR_WIN — the editor likely failed to launch or crashed before completing. Check app/Saved/Logs/ for the real log (see CLAUDE.md's Environment section for why that's a symlinked external path)." >&2
  exit 1
fi

python3 - "$REPORT" "$FILTER" <<'PYEOF'
import json, sys

report_path, filt = sys.argv[1], sys.argv[2]
data = json.load(open(report_path, encoding="utf-8-sig"))  # UE writes a UTF-8 BOM
# A test that logs a Warning (e.g. a deliberate null-safety code path) lands in
# succeededWithWarnings, not succeeded - it's still a pass, not a failure or a
# not-run. Omitting it here undercounts passing tests down to 0 whenever a test's
# intentionally-exercised warning-logging path fires, which silently looks
# identical to "no tests matched this filter".
succeeded = data.get("succeeded", 0) + data.get("succeededWithWarnings", 0)
failed = data.get("failed", 0)
not_run = data.get("notRun", 0)
total = succeeded + failed + not_run

if total == 0:
    print(f"UE_AUTOMATION_RESULT passed=0 total=0 filter={filt!r} — no tests matched this filter")
    sys.exit(1)

print(f"UE_AUTOMATION_RESULT passed={succeeded} total={total}")
if failed or not_run:
    for t in data.get("tests", []):
        if t.get("state") != "Success":
            print(f"UE_AUTOMATION_FAILED {t.get('fullTestPath')}: state={t.get('state')}")
    sys.exit(1)

print("UE_AUTOMATION_OK")
sys.exit(0)
PYEOF
