#!/usr/bin/env bash
# Idempotent local setup: symlinks app/ to the real KrowdKontrol Unreal project on the
# Windows filesystem. Run this once per machine (or after a fresh clone) — app/ is
# gitignored, not tracked, so nothing about this script's *target* is machine-portable
# and it should not be; only the script itself is checked in.
#
# Why a symlink and not a git-tracked copy: tried that first (git-tracked, LFS-ready,
# real per-worktree isolation). Unreal Editor could not open the project from its new
# WSL-hosted location ("Failed to open descriptor file ...") — a real, empirically
# confirmed WSL<->Windows compatibility gap, not a config mistake. Reverted. See
# .factory/decisions.md D-003 for the full story and what this means for MAX_PARALLEL
# (stays at 1 — a symlink gives zero per-worktree isolation, since every worktree's
# symlink resolves to the same external files).
#
# Usage: bash scripts/link-unreal-project.sh

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TARGET="${KROWD_KONTROL_UNREAL_PATH:-/mnt/c/Users/Admin/OneDrive/Dokumente/Unreal Projects/KrowdKontrol}"
LINK="$REPO_ROOT/app"

if [ ! -d "$TARGET" ]; then
  echo "ERROR: Unreal project not found at: $TARGET" >&2
  echo "Set KROWD_KONTROL_UNREAL_PATH if it lives somewhere else on this machine." >&2
  exit 1
fi

if [ -L "$LINK" ]; then
  CURRENT="$(readlink -f "$LINK")"
  WANT="$(readlink -f "$TARGET")"
  if [ "$CURRENT" = "$WANT" ]; then
    echo "LINK_OK app/ already points at $TARGET"
    exit 0
  fi
  echo "app/ is a symlink but points elsewhere ($CURRENT) — replacing." >&2
  rm "$LINK"
elif [ -e "$LINK" ]; then
  echo "ERROR: app/ exists and is not a symlink ($LINK) — refusing to overwrite. Move or remove it first." >&2
  exit 1
fi

ln -s "$TARGET" "$LINK"
echo "LINK_OK app/ -> $TARGET"
