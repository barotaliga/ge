---
title: A fresh cloud container lacks SDL's X11 and audio headers; configure fails on XCURSOR
date: 2026-10-22
scope: tools/session-start.sh, cmake/SDL.cmake, .github/workflows/ci.yml
concepts: [session-start, apt, sdl, container, configure]
---

`cmake --preset debug` in a fresh container stops at SDL's "Couldn't find dependency package
for XCURSOR": only libx11-dev and libxext-dev of `GE_LINUX_PACKAGES` are preinstalled. The
session-start hook installs the missing ones before building, so check
`build/session-start.status` before installing anything by hand. The list is a YAML folded
block (`>-`): parse it as the lines indented deeper than the key, not up to the next key.
A cold debug build with Dawn and SDL downloads takes about 90 s on 4 cores; a warm one is a
no-op.
