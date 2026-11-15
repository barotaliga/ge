---
title: Make a mod switch between frames, never from inside the console command that asks for it
date: 2026-11-15
scope: engine/server/main.cpp, engine/server/spawn.cpp, tests/server_cfg_test.cpp, engine/lab/labs/server_launch_lab.cpp
concepts: [mod-switch, server-cfg, layers, commands-file, hosted-command, console, gesample]
---

The `mod` command only records which mods were asked for. The run loop makes the switch after
the console's run returns (`server_switch_mods`, then `server_close` and `server_open`). Reading
the layers runs the console's buffer itself (`exec`, the command line, `drain`), so doing it from
inside a command would nest one run in another. A switch first resets every server value to its
fallback. Without that, a value only the old mod set (arena's `sv_map dm3`) stays after the
switch. The command line is kept in `ServerCfg::line` and read again last, so the flags and the
cfg handed over still win; a test of the switch has to set what it checks in a mod's cfg, never
by a flag. A rebuilt engine module needs its binary rebuilt too: `make test T=...` builds only
the tests, so a `gesample` from before the change refuses the new flag (`cmake --build
build/debug --target ge_sample_server`).
