---
title: A game wraps an engine command by keeping its fn, not by copying its body
date: 2026-11-15
scope: games/shooters_edge/client/team_chat.cpp, engine/console/console.hpp
concepts: [cfg-load, f-cfgload, add-command, commands, wrap-command, insert-order]
---

A game can add a step to an engine command (`cfg_load` running `f_cfgload` after the config).
To do that, take the command's `fn` from `Console::commands()` and register a new command under
the same name that calls it. `add_command` replaces the old command by name.

The ordering trick: `exec_file` inserts the file's text at the FRONT of the buffer. To run
something after that text, `insert` the follow-up first and then call the wrapped `fn`. The
file's text then lands in front of the follow-up.

Check the file is there with `read_file` before you insert. Otherwise the follow-up runs even
when nothing loaded.
