---
type: Changed
title: The lobby is a screen of its own; F4 joins the warmup
---

The engine's lobby (`ge::server::Lobby`) keeps a player on the game's lobby screen until he joins the warmup (`warmup`, `sitout`, `lobby_on_map`, `LobbySettings::warm_on_join`) and refuses `start` below `min_players`; Shooter's Edge draws the lobby full screen over the main menu's backdrop, F4 joins the warmup, and a match needs two players.
