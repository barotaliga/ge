---
title: A data file that says "read by the game" may have no reader; grep for one before trusting it
date: 2026-11-07
scope: games/shooters_edge/content/classic.assetmap, games/shooters_edge/client/own_sounds.cpp
concepts: [own-sounds, assetmap, silent, arena, search-path]
---

`content/classic.assetmap` said it was "read by the map cloner and the game", but nothing in the
code read its `sound` rows, so the built-in arena (no classic folder) was silent while every test
passed: tests and labs pack their own tones. Grep for a reader of any data file a header claims is
used. The cheap fix is to pack the mapped WAVs in memory as `sound/<classic path>` and mount the
pack with `add_archive`, so every player that reads the search path finds them unchanged. Mount it
only when no classic folder opened: `add_archive` searches the new pack first, so mounting it over
a classic game would override that game's own recordings.
