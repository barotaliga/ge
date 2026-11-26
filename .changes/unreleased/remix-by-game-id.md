---
type: Changed
title: Classic remix names a game by its id
---

The classic remix switches that pick a game (`cr_items`, `cr_models`, `cr_physics`, `cr_sounds`, `cr_monsters`, `cr_menu`) take the game's id instead of a number: `q1` for Quake, `q2` for Quake II, empty (the default) for the played game's own. A config that set them by number plays with the game's own until they are set again.
