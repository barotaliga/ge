---
title: A command whose body is a plugin's is a toolkit row that names its tool
date: 2026-11-15
scope: engine/toolkit/qc.cpp, engine/cli/qc.cpp, plugins/progs/cli/, engine/gamecli/toolkit.cpp
concepts: [toolkit, tool, plugin, gamecli, ge, qc, in-process, hand-off]
---

`ge` never links a plugin, so a shared command whose body is a plugin's (qc: the progs tool) cannot
call it directly. The row stays in `engine/toolkit/` and names the tool (`Row::tool = "qc"`); the
body calls `Call::tool(program, "qc", args)`. `ge` fills the runner by building the repo's tool
executable and reading its output; a game's CLI fills it from the plugin's `cli/` library, which
runs the tool in its own process (`Spec::offers`, `Spec::tool`). The tool takes the program's name
as argv[0] and spells its next steps with it, so the tool's own executable sets argv[0] to "ge".
A game's CLI hands a row only the flags the row declares: its common flags (`--basedir`) would
otherwise reach the tool and be refused as unknown.
