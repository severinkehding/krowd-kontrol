#!/usr/bin/env python3
"""The HTTP-level floor for "the app actually works" - not the full user journey.

Per-repo file. Once MISSION.md documents what krowd-kontrol actually does, this
should assert its hard invariants against a live process, the same way
dark-factory-experiment's e2e.py checks "no anonymous path reaches a
conversation" here. Until then there's no app for `app` (the driver from
serve.py) to call anything on, so this is unreachable in practice - make_driver
refuses before run_e2e is ever invoked.
"""
from __future__ import annotations


def run_e2e(app) -> int | None:
    """Return the number of steps verified, or None to signal failure."""
    raise NotImplementedError(
        "harness/e2e.py has no assertions yet - write them once there's an app "
        "and MISSION.md defines what 'working' means. See harness/README.md."
    )
