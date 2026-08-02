#!/bin/sh
# awmaker - Abstracting Window Maker
#
# Fork of GNU Window Maker (GPL-2).
# Copyright (C) Rodolfo Garcia Penas (kix) <kix@kix.es>
# and individual contributors; see LICENSE for full attribution.
#
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Build awmaker (inside Docker) and run it, drawing its windows on the macOS
# X server (XQuartz).
#
# Why TCP and a dedicated server:
#   * Docker Desktop for macOS cannot use host unix sockets inside containers
#     (mounting /tmp/.X11-unix gives "Operation not supported"), so X11 must go
#     over TCP to the host (host.docker.internal:<display>).
#   * XQuartz normally runs quartz-wm on :0, which already claims display :0 as
#     a window manager; awmaker would refuse to start. So we launch a dedicated
#     XQuartz server on a free display (starting at 1) WITHOUT quartz-wm.
#   * The server is auth-gated with a fresh MIT-MAGIC-COOKIE-1 that we generate,
#     pass to the server with -auth, and inject into a temporary Xauthority that
#     is mounted read-only into the container.
#
# Steps
#   [1] Build (unless --no-build): shared libs + awmaker via script/docker-build.sh
#   [2] Start a dedicated XQuartz server on the first free display >= 1, TCP enabled
#   [3] Run awmaker in the container, DISPLAY=host.docker.internal:<display>
#   [4] On exit, stop the dedicated XQuartz server and remove temp files
#
# Usage (from the repo root):
#   ./script/docker-run.sh            # build + run awmaker
#   ./script/docker-run.sh --no-build # just run (assumes it is already built)
#
# Requires Docker Desktop and XQuartz installed on the host.

set -e

cd "$(dirname "$0")/.."

COMPOSE="docker compose -f docker/docker-compose.yml"

# --- Helpers ----------------------------------------------------------------
cleanup() {
    if [ -n "${XQ_PID:-}" ]; then
        echo "==> Stopping temporary XQuartz (pid $XQ_PID)..."
        kill "$XQ_PID" 2>/dev/null || true
        sleep 1
        pkill -9 -f "Xquartz :$DISP" 2>/dev/null || true
        pkill -9 -f "X11.bin --listenonly" 2>/dev/null || true
    fi
    [ -n "${XQ_AUTH:-}" ] && rm -f "$XQ_AUTH"
}
trap cleanup EXIT INT TERM

# --- Step 1: build -----------------------------------------------------------
case "$1" in
    --no-build)
        echo "==> Skipping build (--no-build)."
        ;;
    "")
        echo "==> Building the awmaker image (first run downloads deps)..."
        $COMPOSE build
        # Build awmaker (shared libs + binary). Note: we deliberately run the
        # plain build (no `make install`): in this container the local libtool
        # is configured with fast_install=needless, so the binary is linked
        # directly into src/awmaker without the .libs/ file that `make install`
        # would need, and `make install` fails. Instead docker-run.sh populates
        # the per-user ~/GNUstep data from the sources and puts the built
        # utilities (util/) on PATH (see Step 3).
        $COMPOSE run --rm build
        ;;
    *)
        echo "Unknown option: $1" >&2
        echo "Usage: ./script/docker-run.sh [--no-build]" >&2
        exit 1
        ;;
esac

# Locate the real ELF (libtool puts it in src/.libs/awmaker, some modes in src/awmaker).
AWMAKER_BIN=""
for cand in src/.libs/awmaker src/awmaker; do
    if [ -f "$cand" ] && [ -x "$cand" ] && [ "$(head -c 4 "$cand" 2>/dev/null)" = "$(printf '\177ELF')" ]; then
        AWMAKER_BIN="$cand"
        break
    fi
done
if [ -z "$AWMAKER_BIN" ]; then
    echo "ERROR: no awmaker ELF found. Run without --no-build first." >&2
    exit 1
fi

# --- Step 2: start a dedicated XQuartz server on a free display -------------
# Find the first display >= 1 not already in use (socket or TCP port busy).
DISP=""
n=1
while [ "$n" -lt 64 ]; do
    if [ ! -e "/tmp/.X11-unix/X$n" ] && ! lsof -iTCP:"$((6000 + n))" -sTCP:LISTEN >/dev/null 2>&1; then
        DISP=$n
        break
    fi
    n=$((n + 1))
done
if [ -z "$DISP" ]; then
    echo "ERROR: no free X display found (0-63 all busy)." >&2
    exit 1
fi
echo "==> Using dedicated display :$DISP"

# Fresh cookie for this display, stored in an Xauthority for -auth.
XQ_AUTH=$(mktemp)
COOKIE=$(xxd -l 16 -p /dev/urandom | tr -d '\n')
xauth -f "$XQ_AUTH" add ":$DISP" MIT-MAGIC-COOKIE-1 "$COOKIE"

echo "==> Starting dedicated XQuartz on :$DISP (no quartz-wm, TCP enabled)..."
/opt/X11/bin/Xquartz ":$DISP" -listen tcp -iglx -noreset -auth "$XQ_AUTH" &
XQ_PID=$!
sleep 4

# Wait until the TCP port is actually listening (X11.bin may need a moment).
for i in 1 2 3 4 5; do
    if lsof -iTCP:"$((6000 + DISP))" -sTCP:LISTEN >/dev/null 2>&1; then
        break
    fi
    sleep 1
done
if ! lsof -iTCP:"$((6000 + DISP))" -sTCP:LISTEN >/dev/null 2>&1; then
    echo "ERROR: XQuartz on :$DISP is not listening on TCP $((6000 + DISP))." >&2
    exit 1
fi
echo "==> XQuartz listening on TCP :$DISP"
echo ""
echo "    IMPORTANT: press Cmd-Option-A (View -> Full Screen) in the XQuartz"
echo "    window NOW. awmaker is a window manager and places the dock/clip/"
echo "    panels against the edges of the X root window. XQuartz starts in"
echo "    'rootless' mode where that window is not shown, so everything would"
echo "    appear scattered across the macOS desktop. Full Screen gives it a"
echo "    contiguous surface so the dock and panels land on the screen edges."
echo ""

# --- Step 3: run awmaker in the container -----------------------------------
# The container connects over TCP to host.docker.internal:$DISP, so its
# Xauthority must carry the cookie under that host name. macOS `xauth` rejects
# that display name, so we inject the cookie INSIDE the container (Linux xauth
# resolves host.docker.internal). The raw cookie is passed via env var.
echo "==> Running awmaker in the container on XQuartz :$DISP..."
echo "    (Ctrl-C to stop)"
DISPLAY="host.docker.internal:$DISP" \
    LD_LIBRARY_PATH="/workspace/awmaker/build-libs/lib" \
    AWMAKER_COOKIE="$COOKIE" \
    $COMPOSE run --rm \
        -e DISPLAY \
        -e LD_LIBRARY_PATH \
        -e AWMAKER_COOKIE \
        run 'set -e
             GS=$HOME/GNUstep
             # Populate the per-user data dirs from the sources so awmaker can
             # find icons, pixmaps/backgrounds and logos without a system-wide
             # install. awmaker looks these up via DEF_ICON_PATHS /
             # DEF_PIXMAP_PATHS under $HOME/GNUstep.
             mkdir -p "$GS/Library/Icons" "$GS/Library/WindowMaker/Pixmaps" "$GS/Library/WindowMaker/Backgrounds"
             cp -n /workspace/awmaker/WindowMaker/Icons/* "$GS/Library/Icons/" 2>/dev/null || true
             cp -n /workspace/awmaker/WindowMaker/Pixmaps/* "$GS/Library/WindowMaker/Pixmaps/" 2>/dev/null || true
             cp -n /workspace/awmaker/WindowMaker/Backgrounds/* "$GS/Library/WindowMaker/Backgrounds/" 2>/dev/null || true
             export XAUTHORITY=$(mktemp)
             # Put the compiled source utilities (wmsetbg, wdwrite, getstyle,
             # seticons, ...) on PATH. They are built into util/ as ELF binaries,
             # not system-installed.
             export PATH="/workspace/awmaker/util:$PATH"
             xauth add "host.docker.internal:'$DISP'" MIT-MAGIC-COOKIE-1 "$AWMAKER_COOKIE"
             exec "/workspace/awmaker/'$AWMAKER_BIN'"' || true

echo "==> awmaker exited."
