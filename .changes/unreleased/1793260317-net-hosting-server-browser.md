---
type: Added
title: Hosting from inside a game and the server browser
---

`engine/net` hosts a game from inside a game in one call (`hosting.hpp`): it answers the LAN's probes and the browsers' pings, asks the home router for a forward and closes it when the game ends, registers with the rendezvous and, for a public game, keeps its seats and phase listed there, unlisting it the moment it stops; a game can be LAN-only, invite-only or listed, and the same calls run a listen server and a dedicated one. The rendezvous now keeps that list and answers it a page at a time. `directory.hpp` is a server browser's model: the LAN's games and the list's merged into one row each, every game pinged, and views filtered (full, empty, playing, locked, ping, name) and sorted (ping, players, name), with the route to join each. The LAN advert carries the session's id and a locked flag, and a broadcast on the simulated network stays on its own LAN. The `net-host` lab plays an evening of hosted games and draws both browsers.
