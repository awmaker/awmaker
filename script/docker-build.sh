#!/bin/sh
# awmaker - Abstracting Window Maker
#
# Fork of GNU Window Maker (GPL-2).
# Copyright (C) Rodolfo Garcia Penas (kix) <kix@kix.es>
# and individual contributors; see LICENSE for full attribution.
#
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Build awmaker inside the Docker container.
#
# The host's awmaker tree is mounted at /workspace/awmaker (read-write), so all
# generated files and binaries are written back to the host. NOTHING is
# installed outside the working tree:
#   * shared libraries  -> <tree>/build-libs
#   * awmaker binary    -> <tree>/src/awmaker (no system install unless
#                          STAGE_DIR is set, and then still inside the tree)
#
# Steps
#   [1] Build & install the shared libraries (wrlib, WINGs) into ./build-libs
#       from the third_party/wmaker-crm submodule (script/build-libs.sh).
#   [2] autogen + configure + make awmaker, linking against ./build-libs
#       via pkg-config.
#
# Environment knobs:
#   PREFIX           local prefix where awmaker would be staged (default:
#                    <tree>/build-libs/stage). Used only if INSTALL=1.
#   INSTALL          if set to 1, run `make install` into PREFIX (still local).
#   JOBS             parallelism for make (default = nproc)
#   CONFIGURE_ARGS   extra arguments forwarded to ./configure

set -e

cd /workspace/awmaker || { echo "ERROR: not the awmaker tree (/workspace/awmaker)"; exit 1; }

if [ ! -d "third_party/wmaker-crm/WINGs" ]; then
    echo "ERROR: the wmaker-crm submodule is not checked out."
    echo "On the HOST run:  git submodule update --init third_party/wmaker-crm"
    exit 1
fi

JOBS="${JOBS:-$(nproc)}"
LIBS_PREFIX="/workspace/awmaker/build-libs"

echo "==> [1/4] Building shared libraries into $LIBS_PREFIX (local)..."
./script/build-libs.sh

export PKG_CONFIG_PATH="$LIBS_PREFIX/lib/pkgconfig"
echo "    PKG_CONFIG_PATH=$PKG_CONFIG_PATH"

echo "==> [2/4] Generating autotools files (autogen.sh)..."
# autogen.sh runs autoreconf (its configure-launching tail is dead code because
# of an early 'exit 0') and regenerates INSTALL-WMAKER / README.i18n.
./autogen.sh

PREFIX="${PREFIX:-$LIBS_PREFIX/stage}"
CONFIGURE_ARGS="${CONFIGURE_ARGS:-}"

echo "==> [3/4] Configuring awmaker (prefix $PREFIX, local)..."
# shellcheck disable=SC2086
./configure --prefix="$PREFIX" --disable-silent-rules $CONFIGURE_ARGS

echo "==> [4/4] Building awmaker (${JOBS} jobs)..."
make -j"$JOBS"

if [ "${INSTALL:-0}" = "1" ]; then
    echo "==> Staging awmaker into $PREFIX (local, inside the tree)..."
    make install
fi

echo "==> Done."
echo "    Binary: $(pwd)/src/awmaker"
if [ "${INSTALL:-0}" = "1" ]; then
    echo "    Staged to: $PREFIX"
else
    echo "    (not installed; set INSTALL=1 to stage it locally)"
fi
