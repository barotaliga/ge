---
title: Before a menu row toggles a cvar, grep that the window reads it; a cvar that exists is not a feature that works
date: 2026-11-01
scope: games/shooters_edge/menu/main_menu.cpp, plugins/classicenhance/switches.hpp
concepts: [menu, cvar, dimmed, ce-switches, settings]
---

Every `ce_` switch is a registered cvar, so a `needs` check lights its row, yet fifteen of them
(`ce_decals`, `ce_reflections`, `ce_flicker`, `ce_glow`, `ce_explosions`, ...) are read by nothing in
the window. A row for one of them would be a toggle that does nothing. Grep the field
(`grep -rnE "(ce|on)(\.|->)<field>\b" games/shooters_edge plugins/brush`) before adding a row, and
leave the row out (or `built = false`) until the window reads it. Likewise a setting read only when
the world is made (`ce_textures`, `ce_models`, `ce_rebake`, `ce_bloom`, `r_bloom`) needs the world
made again (`world_stale`), or the menu's change shows only after the next map. The window's real
cvar list is one command away: `SDL_VIDEODRIVER=offscreen build/debug/games/shooters_edge/shooters_edge --frames=1 +cvarlist +cmdlist`.
