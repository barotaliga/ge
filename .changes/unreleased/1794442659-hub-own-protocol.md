---
type: Added
title: Shooter's Edge's servers appear on the hub
---

A server whose game speaks its own protocol now beats to the masters in `sv_master` and answers a hub's status asks too: the core takes those datagrams off the game's socket before the game reads it (`Transport::on_public`). Shooter's Edge's server says its map, mode, score screen, limits and every player, so `shooters_edge_server +set sv_master <hub>` is listed by `shooters_edge_hub`.
