---
title: A game's items, call words and sounds are the profile's rows; the bots never name a class
date: 2026-11-01
scope: games/shooters_edge/profiles/items.hpp, games/shooters_edge/bots/knowledge.cpp, games/shooters_edge/bots/ears.cpp
concepts: [item-row, call-word, heard-word, sound-table, game-profile, item-kind, classname]
---

`item_of`, `item_call`, `hears_as` and `sound_table` read `profiles::kClassicTables`: the item rows (class name, the spawnflags a variant needs, kind, amount, doubling, respawn, weapon bit, name), the words (call and heard, by kind and weapon bit) and the samples. To add or rename an item, edit a row, not the bots. Rows are tried in order, so put a variant (the mega health's spawnflag 2) before its plain row. A class name ending in `*` matches any rest of it. A field at -1 keeps the `ItemSpot` default.

`ItemKind` now lives in the profile (`bots/fighter.hpp` aliases it). The bots library reads `items.hpp` only, never `game_profile.hpp`: that header pulls in classic-enhance's content row, which the bots do not link.
