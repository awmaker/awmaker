# awmaker - Abstracting Window Maker
#
# Fork of GNU Window Maker (GPL-2).
# Copyright (C) Rodolfo Garcia Penas (kix) <kix@kix.es>
# and individual contributors; see LICENSE for full attribution.
#
# SPDX-License-Identifier: GPL-2.0-or-later
#
# awmaker build container
#
# Goal: compile awmaker on a macOS host using Docker (the host itself lacks
# the X11/autotools toolchain). The container provides the toolchain and the
# host's awmaker tree is mounted read-write via a volume, so build artifacts,
# configured files and even the generated binaries land back on the host.
#
# Usage:
#   docker compose -f docker/docker-compose.yml build
#   docker compose -f docker/docker-compose.yml run --rm build
#
# The default entrypoint runs `script/docker-build.sh` (autogen+configure+make).

# Debian bookworm is intentionally old-stable: it matches the project's
# supported Linux environment and its packages are stable/ABI-safe.
FROM debian:bookworm-slim

ENV DEBIAN_FRONTEND=noninteractive

# Base toolchain
RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential \
        autoconf \
        automake \
        libtool \
        autoconf-archive \
        gettext \
        pkg-config \
        gawk \
        make \
        file \
        ca-certificates \
        git \
        xauth \
    && rm -rf /var/lib/apt/lists/*

# X11, fonts and required X extensions
RUN apt-get update && apt-get install -y --no-install-recommends \
        libx11-dev \
        libxext-dev \
        libxmu-dev \
        libxinerama-dev \
        libxrandr-dev \
        libxft-dev \
        libxrender-dev \
        libxt-dev \
        libxpm-dev \
        libfontconfig1-dev \
        x11proto-core-dev \
        x11proto-xext-dev \
        x11proto-xinerama-dev \
    && rm -rf /var/lib/apt/lists/*

# Test/runner apps useful inside the container (drawn on the host X server)
RUN apt-get update && apt-get install -y --no-install-recommends \
        xterm \
        x11-apps \
        x11-utils \
        xfonts-base \
    && rm -rf /var/lib/apt/lists/*

# Optional image-format libraries + helper libs (auto-detected by configure)
RUN apt-get update && apt-get install -y --no-install-recommends \
        libpng-dev \
        libjpeg-dev \
        libtiff-dev \
        libwebp-dev \
        libgif-dev \
        libjxl-dev \
        libbz2-dev \
        zlib1g-dev \
        libbsd-dev \
    && rm -rf /var/lib/apt/lists/*

# Pango (optional; gives better UTF-8/i18n text layout in WINGs). Configure
# uses --enable-pango=auto, so this is merely present but not forced.
RUN apt-get update && apt-get install -y --no-install-recommends \
        libpango1.0-dev \
    && rm -rf /var/lib/apt/lists/*

# i18n tools (msgfmt/xgettext) are needed by autogen/configure
RUN apt-get update && apt-get install -y --no-install-recommends \
        gettext-base \
    && rm -rf /var/lib/apt/lists/*

# Where the host awmaker tree is mounted
WORKDIR /workspace/awmaker

# Keep the toolchain available; the actual build is driven per-invocation
# so that the volume-mounted source can be rebuilt repeatedly.
CMD ["/bin/bash"]
