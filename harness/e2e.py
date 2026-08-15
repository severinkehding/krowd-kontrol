#!/usr/bin/env python3
"""The floor for "the app actually works" - not yet the full user journey.

Runs the KrowdKontrol.Smoke.* Unreal Automation Framework suite via the `cli` driver
(harness/run_ue_automation.sh) and asserts it passed. This is real: it boots the
Editor headlessly and runs a real, passing test - not a stub. What it is NOT yet is a
genuine gameplay journey (herd an enemy to a target zone, etc.) - MISSION.md's core
loop (`01-core-gameplay-loop.md`) has no playable content to walk yet. Extend this
once it does, following dark-factory-experiment's e2e.py shape: assert hard
invariants from MISSION.md against the live process, not implementation details. See
harness/README.md and .factory/decisions.md D-004.
"""
from __future__ import annotations

import re


def run_e2e(app) -> int | None:
    """Return the number of steps verified, or None to signal failure."""
    rc, out, err = app.run("KrowdKontrol.Smoke.", timeout=240)
    print((out + err).strip(), flush=True)

    if rc != 0 or "UE_AUTOMATION_OK" not in out:
        return None

    m = re.search(r"UE_AUTOMATION_RESULT passed=(\d+) total=(\d+)", out)
    return int(m.group(1)) if m else 1
