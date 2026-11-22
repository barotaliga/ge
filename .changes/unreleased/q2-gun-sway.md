---
type: Added
title: The held weapon swayed as a server says
---

The view model's placement takes a game's own turn and shift on top
(`engine/viewmodel/place.hpp`'s `PlaceInput::game_turn` and `game_shift_m`). Quake II's held weapon
now sways as its server sends it, in a hosted game, a recording and a joined server.
