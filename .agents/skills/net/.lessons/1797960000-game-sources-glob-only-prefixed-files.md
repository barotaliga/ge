---
title: Shooter's Edge's net/ library globs only qw_, q2_ and q1_ files; a new file needs the prefix
date: 2026-11-22
scope: games/shooters_edge/CMakeLists.txt, games/shooters_edge/net/
concepts: [cmake, glob, undefined-symbol, source-prefix, net]
---

`shooters_edge_classic` builds `net/qw_*.cpp`, `net/q2_*.cpp` and `net/q1_*.cpp` (and two named
files), not every file in `net/`: a new `net/remote_macros.cpp` compiled into nothing and the test
failed to link with "undefined symbol". Name a file there for the protocol family it serves
(`qw_remote_macros.cpp` for the join that speaks both), or add it to the CMake list on purpose.
