#!/usr/bin/env bash
# The stop button. Both halves of it.
#
#   bash scripts/factory-stop.sh          exit 0 = clear to dispatch, exit 1 = STOPPED
#
# In the original dark-factory-experiment this file's orchestrator lived outside the
# repo on a VPS (/opt/dark-factory/orchestrator.sh), specifically so it held no state of
# its own - everything it read was visible here as issues, PRs and labels. This repo's
# orchestrator (scripts/orchestrator.sh) runs on the operator's own machine instead and
# is kept in-repo for the same transparency reason, just without the VPS. Either way the
# CHECK lives here, versioned and readable, and the orchestrator calls it first:
#
#   if ! bash scripts/factory-stop.sh; then exit 0; fi
#
# Two mechanisms, on purpose, because they fail in different places.
#
#   1. A LOCAL kill file. Works with the network down, which is when you most want it.
#   2. A REMOTE label: any open issue tagged `factory:stop`. Reachable from a phone,
#      which is the entire reason it exists at 2am.
#
# Both are checked before anything else is read.

set -uo pipefail

REPO="${FACTORY_REPO:-severinkehding/krowd-kontrol}"
KILL_FILE="${FACTORY_KILL_FILE:-${FACTORY_WORKDIR:-.}/.factory-stop}"
STOP_LABEL="factory:stop"
# How long a kill file is honored before it self-expires. Pauses here are almost
# always short, deliberate ones (e.g. "Editor's open for MCP work, don't collide
# with a headless build") - but nothing enforced that they actually got resumed.
# A pause left in place by a forgotten `rm`, a crashed session, or a conversation
# that just never got back to it would otherwise silence the loop indefinitely
# with no signal to anyone that it happened. Auto-expiry bounds that blast radius
# without needing a human to remember - override with FACTORY_KILL_FILE_MAX_AGE_S.
KILL_FILE_MAX_AGE_S="${FACTORY_KILL_FILE_MAX_AGE_S:-14400}"  # 4 hours

# --- 1. the local half -------------------------------------------------------
if [ -f "$KILL_FILE" ]; then
  AGE_S=$(( $(date +%s) - $(stat -c %Y "$KILL_FILE" 2>/dev/null || stat -f %m "$KILL_FILE") ))
  if [ "$AGE_S" -ge "$KILL_FILE_MAX_AGE_S" ]; then
    echo "STOP_EXPIRED: $KILL_FILE was ${AGE_S}s old (limit ${KILL_FILE_MAX_AGE_S}s) — auto-resuming. Last reason was: $([ -s "$KILL_FILE" ] && head -1 "$KILL_FILE" || echo "(none given)")"
    rm -f "$KILL_FILE"
    # fall through to the remote check below rather than exiting 0 here directly -
    # a stray factory:stop issue should still hold even if the local pause just expired
  else
    echo "STOPPED: $KILL_FILE present (${AGE_S}s old, expires at ${KILL_FILE_MAX_AGE_S}s). Remove it to resume."
    [ -s "$KILL_FILE" ] && echo "reason: $(head -1 "$KILL_FILE")"
    exit 1
  fi
fi

# --- 2. the remote half, and it FAILS CLOSED ---------------------------------
# This is the polarity that matters and it is easy to get backwards. The obvious design
# is "the factory runs while a label is absent" - but an absent label cannot be told
# apart from an API call that failed to return it, so a network blip reads as "carry on"
# and the stop button works only while the network does.
#
# Inverted here: the label is something you ADD, and ANY error listing it counts as
# stopped. An unreadable stop button is a stop button you do not have.
if ! HITS=$(gh issue list -R "$REPO" --label "$STOP_LABEL" --state open \
              --json number,title --jq '.[] | "#\(.number) \(.title)"' 2>&1); then
  echo "STOPPED: cannot read the stop state from GitHub, halting: $HITS"
  exit 1
fi

if [ -n "$HITS" ]; then
  echo "STOPPED: an open issue carries $STOP_LABEL"
  echo "$HITS" | sed 's/^/  /'
  exit 1
fi

echo "STOP_CHECK_OK: no kill file, no $STOP_LABEL issue"
exit 0
