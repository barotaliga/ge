---
type: Changed
title: The shooter's bots are a plugin of their own, plugins/shooterbots
---

The pilot, the nav, the senses, the aim, the dodge, temperament and traits, team talk's meanings and the recipes read off people moved out of the shooter kit into `plugins/shooterbots` (`ge_shooterbots`, namespace `ge::shooterbots`). A game that links the kit alone no longer carries the bots; a game with bots links `ge_shooterbots` beside `ge_shooter`. `aim_angles` stays in the kit's `vec.hpp`.
