---
title: A game's own command is our command under our name, the game's spelling a layer synonym
date: 2026-11-22
scope: engine/console/console.hpp, games/shooters_edge/client/cvar_layers.cpp
concepts: [command-synonym, add-command-synonym, layers, ogg, music, usage-line]
---

`add_command` is not layered: a command registered while a game loads stays after it leaves. A
game's own command (Quake II's `ogg`) is therefore built once under our name (`music`, in the
engine) and the game's spelling added with `add_command_synonym` inside its layer, so it goes with
the game. The synonym runs ours with the words as typed: `a[0]` is the game's spelling, so a usage
line built from `a[0]` reads "Usage: ogg ..." in Quake II and "Usage: music ..." elsewhere.
