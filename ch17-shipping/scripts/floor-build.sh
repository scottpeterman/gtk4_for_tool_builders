#!/bin/bash
# Build and test gtklab against the oldest GTK it supports.
#
# usage: scripts/floor-build.sh <prefix>
#   <prefix> is where a GTK 4.8 build is installed (e.g. ~/floor/gtk48).
# On a Debian 12 machine or container, GTK 4.8 is the system GTK: run
# `meson setup build-floor` directly instead.
set -euo pipefail
PREFIX="$1"
PCDIR=$(find "$PREFIX" -name gtk4.pc -printf '%h\n' | head -1)
export PKG_CONFIG_PATH="$PCDIR"
export LD_LIBRARY_PATH="$(dirname "$PCDIR")"
pkg-config --modversion gtk4
rm -rf build-floor
meson setup build-floor --buildtype=debug
meson compile -C build-floor
meson test -C build-floor
echo "Built against GTK $(pkg-config --modversion gtk4). Run: LD_LIBRARY_PATH=$LD_LIBRARY_PATH build-floor/gtklab"
