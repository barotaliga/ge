---
title: A Quake II recording's map needs its textures from files, its palette and each player's look
date: 2026-11-19
scope: games/shooters_edge/livemap/live_map.cpp
concepts: [live-map, overview, q2, version-38, wal, palette, model-255, players-look, covered]
---

Three things a Quake overview gets for free and a Quake II recording does not:

- A version 38 map keeps no pixels: `ba::load_scene` on the map's bytes alone draws every floor in
  a flat fallback colour. Hand it the textures through `TextureSource::pictures`
  (`classic::content::map_textures`, read from the game's files; the archives are in memory, so a
  plate thread may read them).
- The palette is Quake II's pictures' (`pics/colormap.pcx`), not `gfx/palette.lmp`: pick it by
  the recording (`recording_palette`, by its format: the early recordings carry older protocol versions), never by whichever game is loaded.
- A player's model 255 means "his own": the look in his configstring (`male/grunt`) is
  `players/male/tris.md2` worn in `players/male/grunt.pcx`; Quake's shirt and pants
  translation does not apply.

Before calling an item "wrongly dimmed" on a Quake II map, look for a ceiling face just under the
floor above it: its maps stack walkways a few units thick over rooms, and the covered test is right.
