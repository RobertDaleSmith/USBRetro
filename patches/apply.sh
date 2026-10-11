#!/usr/bin/env bash
# Apply the submodule patches this repo needs.
#
# These land in git submodules under src/lib (which is gitignored, so the patch
# itself has to live here), and `make init` -- git submodule update plus the
# pinned pico-sdk/TinyUSB checkouts -- restores pristine sources and drops them.
# `make init` runs this afterwards; re-run it by hand after any submodule update.
#
# Idempotent: already-applied patches are skipped.
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
btstack="$here/../src/lib/pico-sdk/lib/btstack"

if [ ! -d "$btstack/src/ble/gatt-service" ]; then
    echo "error: no BTstack tree at $btstack (run 'make init' first)" >&2
    exit 1
fi

for patch in "$here"/*.patch; do
    [ -e "$patch" ] || continue
    name="$(basename "$patch")"
    if git -C "$btstack" apply --reverse --check "$patch" >/dev/null 2>&1; then
        echo "already applied: $name"
    elif git -C "$btstack" apply "$patch"; then
        echo "applied: $name"
    else
        echo "error: $name did not apply — BTstack may have moved on; re-cut it" >&2
        exit 1
    fi
done
