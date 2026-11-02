---
type: Added
title: Config files and place names from the game's files
---

Shooter's Edge's `exec` also finds a config file in the game's packs and mod folder (through the classic game's adapter, `find_config`), after the player's settings folder and the folder the game runs in, so a mod's own cfg runs and the player's copy wins. A map's place names come through the adapter too (`open_locations`, the first game's `locs/<map>.loc`). The console gains `set_fallback_reader`, a host's reader tried after its search directories.
