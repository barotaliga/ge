---
title: A game's own value is a scope on the cvar, and a config line must be set by name to reach it
date: 2026-11-18
scope: engine/console/cvar.cpp, engine/console/cvar_layers.cpp, engine/console/console.cpp, engine/console/cfg.cpp
concepts: [scope, scoped, common, game-override, synonym, reading-config, cfg-save-pair]
---

A per-game override is kept on the cvar itself (`Cvar::scoped`, `Cvar::common`), not as a second
cvar: the live value is the loaded scope's own or the common one, and saving writes `common` plus
one `seta <game>:<name>` line per scope. Whether a line is "from a config" is the line's `Origin`
having a file (`Console::run` sets `Cvars::set_reading_config`). The trap: the console's dispatch
found the cvar through the synonym and called `Cvars::set(Cvar&, ...)`, which never sees the name
the line used, so the synonym's override rule never ran. Set by NAME (`Cvars::set(name, ...)`)
wherever the spelling matters.
