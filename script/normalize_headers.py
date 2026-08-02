#!/usr/bin/env python3
#
# Standardize the leading comment header of every C/H source file.
#
# Replaces the header with a single uniform short one that identifies the
# project as a fork of GNU Window Maker, preserves (via LICENSE) the original
# copyright attribution, credits the fork modifications, and states the GPL.
#
# Only the leading comment block(s) of each file are touched; the body of the
# file is left byte-for-byte unchanged.

import os
import re
import sys

HEADER = """/*
 * awmaker - Abstracting Window Maker
 *
 * Fork of GNU Window Maker (GPL-2).
 * Copyright (C) Rodolfo Garcia Penas (kix) <kix@kix.es>
 * and individual contributors; see LICENSE for full attribution.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */"""

ROOTS = ["src", "util", "WPrefs.app", "test"]
EXTS = (".c", ".h", ".m")


def rewrite(text):
    # Normalize a leading byte order mark and any blank line(s) at the very
    # top of the file, so the header always starts on the first line.
    text = text.lstrip("\ufeff")
    text = text.lstrip("\n")

    # Collapse repeated copies of the standard header. A stale normalize run
    # (e.g. a header edited between two runs, or a file that didn't start with
    # the exact header) can leave the header duplicated 2x at the very top;
    # keep only the first copy and whatever follows it.
    if text.startswith(HEADER):
        rest = text[len(HEADER):]
        while True:
            after = rest.lstrip("\n \t")
            if after.startswith(HEADER):
                rest = after[len(HEADER):]
            else:
                break
        text = HEADER + rest

    # Already normalized: file begins with exactly the standard header.
    if text.startswith(HEADER):
        return text

    # Headers that carry the GPL license text: replace the whole leading
    # comment block (including any single-line description comment before it).
    if "This program is free software" in text:
        li = text.index("This program is free software")
        j = text.index("*/", li) + 2
        i = text.index("/*")
        line_start = text.rfind("\n", 0, i) + 1
        block_end = text.find("\n", j)
        if block_end == -1:
            block_end = len(text)
        else:
            block_end += 1
        return text[:line_start] + HEADER + "\n" + text[block_end:]

    # No GPL header. If the file starts with a leading comment block that
    # ends before any code, replace it with the standard header so every
    # file carries the same header. Otherwise prepend the header.
    i = text.find("/*")
    if i == -1:
        return HEADER + "\n\n" + text
    j = text.find("*/", i)
    if j == -1:
        return HEADER + "\n\n" + text
    # Only treat as a replaceable header block if the first non-comment
    # content does not appear before the closing of this block.
    before = text[:j]
    stripped = re.sub(r"/\*.*?\*/", "", before, flags=re.S)
    if not stripped.strip():
        line_start = text.rfind("\n", 0, i) + 1
        block_end = text.find("\n", j)
        if block_end == -1:
            block_end = len(text)
        else:
            block_end += 1
        return text[:line_start] + HEADER + "\n" + text[block_end:]
    return HEADER + "\n\n" + text


def main():
    changed = 0
    for root in ROOTS:
        for dirpath, dirnames, filenames in os.walk(root):
            dirnames.sort()
            for name in sorted(filenames):
                if not name.endswith(EXTS):
                    continue
                fpath = os.path.join(dirpath, name)
                with open(fpath, "r", encoding="utf-8") as fh:
                    text = fh.read()
                newtext = rewrite(text)
                if newtext != text:
                    with open(fpath, "w", encoding="utf-8") as fh:
                        fh.write(newtext)
                    changed += 1
                    print("updated %s" % fpath)
    print("---- %d files updated" % changed)
    return 0


if __name__ == "__main__":
    sys.exit(main())
