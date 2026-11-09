---
title: The change reach spreads by the header an object passes on, never by its library's directory
date: 2026-11-09
scope: engine/cli/reach.cpp
concepts: [change-reach, affected, digest, ninja-deps, over-reach, gpu-build]
---

Spreading a reach through "every header in a directory a reached library compiles in" exploded in
a GPU build. engine/render/viewmodel.cpp includes engine/viewmodel/held.hpp, so a change there
reached ge_render. Every lab including any engine/render header followed, which was 100 labs,
mostly Fall Line world labs, and all their digests held. A CPU-only build never shows it, because
ge_render is not built there. Before you call a reach right, probe it in a build with the GPU on
(`ge digest --affected --list` after a one-line probe commit). Labs smoke in CI still runs every
lab, so a reach that is a little narrow costs a later failure, not a missed one.
