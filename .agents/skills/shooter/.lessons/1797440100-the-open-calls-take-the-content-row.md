---
title: The shared open calls take a game's content row as an argument, never its adapter
date: 2026-11-15
scope: plugins/classic/content/open.hpp, games/shooters_edge/client/adapter_parts.cpp
concepts: [content-layer, open-model, open-sound, map-info, adapter-parts, shooter]
---

The calls that open a classic game's files by what they are (`map_info`, `map_textures`,
`open_model`, `open_sound`, `find_config`, `open_locations`, `stand_in`) live in
`plugins/classic/content/open.hpp`. One that needs a game's folders takes its content row
(`classicenhance::Profile`) as an argument; none reads a game profile or an adapter, so the plugin
stays free of any game. A game's adapter entry is a one-line forward that passes
`content_of(*self.game)`. Calls that read a game's program or demo rows (`list_mods`,
`find_program`, `list_demos`, `find_demo`) stay in the game until those rows are the plugin's.
