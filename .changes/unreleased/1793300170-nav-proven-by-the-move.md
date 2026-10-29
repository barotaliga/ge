---
type: Added
title: Navigation proven by the player move
---

`plugins/shooter/nav.hpp` builds a first-person map's navigation graph by asking the player move itself: floor nodes sampled off the map's hulls, and a walk, drop or jump link kept only where `player_move` really carries a player there, so bots planning on it can do nothing a player cannot, and a change to the move changes the graph. Movers such as doors are left out and tagged on the links they close. Shooter's Edge's `se-nav` lab pictures it on a new nav course map and on the test arena.
