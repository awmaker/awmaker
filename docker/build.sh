#!/bin/sh
# awmaker - Abstracting Window Maker
#
# Fork of GNU Window Maker (GPL-2).
# Copyright (C) Rodolfo Garcia Penas (kix) <kix@kix.es>
# and individual contributors; see LICENSE for full attribution.
#
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Convenience wrapper to build awmaker with Docker from the host (macOS).
#
# It mounts the current awmaker tree read-write into the container and runs
# the container build script (libs -> build-libs, then awmaker in src/).
#
# Usage (from the awmaker repo root):
#   ./docker/build.sh                 # build (no install)
#   ./docker/build.sh --install       # build + stage awmaker into build-libs/stage
#   ./docker/build.sh shell           # drop into a shell inside the container
#
# Nothing is installed outside the working tree.
# Requires Docker Desktop (macOS).

set -e

cd "$(dirname "$0")/.."

COMPOSE="docker compose -f docker/docker-compose.yml"

echo "==> Building the awmaker-builder image (first run downloads deps)..."
$COMPOSE build

case "$1" in
    shell)
        echo "==> Opening a build shell. Source is at /workspace/awmaker (rw)."
        $COMPOSE run --rm --entrypoint /bin/bash build
        ;;
    --install)
        echo "==> Building and staging awmaker into build-libs/stage..."
        INSTALL=1 $COMPOSE run --rm build
        ;;
    "" )
        echo "==> Building awmaker (no install)..."
        $COMPOSE run --rm build
        ;;
    *)
        echo "Unknown option: $1" >&2
        echo "Usage: ./docker/build.sh [--install|shell]" >&2
        exit 1
        ;;
esac
