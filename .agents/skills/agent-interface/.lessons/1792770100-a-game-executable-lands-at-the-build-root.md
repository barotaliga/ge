---
title: An executable ge runs by path must be put at the build root
date: 2026-10-22
scope: games/fall_line/CMakeLists.txt, scaffold/CMakeLists.txt.in, engine/cli/play.cpp
concepts: [play, cmake, runtime-output-directory, ge-play]
---

`ge play` and friends run `build/<preset>/play`, but CMake writes an executable into the binary
directory of the `CMakeLists.txt` that adds it (`build/debug/games/fall_line/play`). Any target
`ge` or the Makefile runs by path sets `RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}`, in this
tree and in the scaffold alike. The play executable answers each command with one JSON line, and
`play_answer` reads the LAST line starting with `{`: a sanitizer's report or the game's own
stderr may come first, so never assume the output is the answer alone.
