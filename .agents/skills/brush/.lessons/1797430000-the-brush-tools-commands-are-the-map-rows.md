---
title: The brush tool's commands are the toolkit's map rows in ge and in a game's CLI
date: 2026-11-15
scope: engine/toolkit/map.cpp, plugins/brush/cli/commands.cpp, plugins/brush/tool.cpp
concepts: [brush, map, toolkit, secli, bsp-main, tool-runner, map-info]
---

`map info`, `ents`, `liquids`, `slice` and `check` are rows in `engine/toolkit/map.cpp` whose
body is the brush tool (`bsp_main`): `ge map ...` builds the repo's `brush` executable, `secli map
...` calls `bsp_main` in its own process through `plugins/brush/cli`'s `run_tool`. A new tool
command is a branch in `tool.cpp`, then a row (its flags and the text `map_answer` prints) in
`map.cpp`; the tool's own `next:` words are dropped, since the row names the calling program. A
game with several in-process tools sets `Spec::tool` to a small dispatcher by the tool's name
(secli: "brush" to the brush plugin, the rest to progs).
