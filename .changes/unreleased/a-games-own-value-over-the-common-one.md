---
type: Added
title: A game's own value of a setting over the series' common one
---

A config can set a value for every game and another for one game: `gamma 1` beside Quake II's own spelling `vid_gamma 0.9` gives Quake II 0.9 and every other game 1, whichever line comes first. In the console, `<game>:<name> <value>` (or `game:<name>` for the game loaded) sets a game's own value, `unset <game>:<name>` takes it away, and a plain typed value sets the common one. Each game's own values are saved in its own file, and `cfg_save <name>` writes `<name>.cfg` with `<name>.<game>.cfg` beside it, which `exec` reads together.
