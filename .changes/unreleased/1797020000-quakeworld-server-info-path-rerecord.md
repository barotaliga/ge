---
type: Added
title: QuakeWorld's server info keys, sv_gamedir and sv_highchars, the model check, path, rerecord, fullinfo and the chase camera's side step
---

A QuakeWorld server's info now carries the mod program's `*progs` checksum and `*cheats ON` when started with cheats (the server rule `cheats 1`). The operator's `sv_gamedir` changes the game folder the server names, `sv_highchars 0` keeps players' names and info to plain characters, and a player whose player or eyes model differs from the server's is announced to everyone. A Quake single-player program's developer lines show only with `developer 1`. In the window, `path` lists where the game's files come from, `rerecord <name>` joins the last server again and records from the start, `fullinfo` sets your whole user info at once, `cam_side` (Quake's `chase_right`) moves the third-person camera to the side, and an old Quake config's `_cl_name`, `_cl_color` and `port` lines work.
