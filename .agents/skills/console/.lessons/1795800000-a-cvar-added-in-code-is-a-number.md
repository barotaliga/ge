---
title: A cvar added in code is a number unless its spec says text, and a fallback reader comes last
date: 2026-11-02
scope: engine/console/cvar.hpp, engine/console/console.cpp, games/shooters_edge/client/game_adapter.cpp
concepts: [cvar-kind, cvar-spec, fallback-reader, exec, config-file, test]
---

`CvarSpec::kind` defaults to `CvarKind::number`, so a test that adds `{.name = "x"}` and execs
`set x pack` reads back `"0"` with no problem logged: give a text cvar `.kind = CvarKind::text`.
To let `exec` find a file somewhere other than the search directories (a game's packs), use
`set_fallback_reader`, not `set_files`: `set_files` replaces the directories and their writer,
while the fallback is tried only after the directories hold nothing of the name, so the player's
own copy still wins and `cfg_save` still writes to the first directory. The startup files run in
`ge::console::start`, before a game's files open, so a pack's own `default.cfg` never runs at
startup.
