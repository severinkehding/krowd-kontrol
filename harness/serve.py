#!/usr/bin/env python3
"""Placeholder for the app's real boot entrypoint.

Once there's an app, `harness.config.json`'s `http.start` should point here
(e.g. `"start": "python harness/serve.py --port {port}"`), and this file's job
is to start the real process and refuse loudly with a named reason if required
config/env is missing - see dark-factory-experiment's harness/serve.py for the
pattern this is modeled on (it hard-refuses rather than letting the app crash
into a health-check timeout that reads as "not testable").

Until then, appproc.py's HttpApp already refuses loudly on its own because
`http.start` is empty in harness.config.json - this file being empty is not a
gap, it's unreached code. Delete this docstring and replace main() with the
real thing in the same commit that adds app/.
"""
from __future__ import annotations

import sys


def main() -> int:
    print("SERVE_NOT_IMPLEMENTED no app configured yet - see harness/README.md", file=sys.stderr)
    return 1


if __name__ == "__main__":
    sys.exit(main())
