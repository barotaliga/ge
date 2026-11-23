---
title: A mission pack's status bar reads the items word, and the HUD keeps the rises
date: 2026-11-23
scope: games/shooters_edge/hud/layout.cpp, games/shooters_edge/hud/classic_art.cpp, games/shooters_edge/hud/status.hpp
concepts: [items-word, items2, mission-pack, pickup-flash, pack-art, rose-at]
---

The eight `Weapon`s cannot say a pack's guns or items, so `Status::items` and `in_hand` carry the
program's bits whole (`items2` from bit 23, put there by the server's `items_word` and by
`status_of` on a listen server), and layout.cpp reads them only when the set's `HudArt::pack` says
which pack's pictures loaded (`pack_art` takes a pack's set only when every picture is there). A
producer that leaves `items` 0 keeps the old drawing exactly. The pickup flash needs history a
snapshot has not got: `Hud::rose_at` stamps each bit's rise in `fill_hud`, so the first fill after
a map loads flashes every owned gun, as the original did; a lab that sets items by hand fills the
HUD once and lets a second and a half pass before its pictures (`--fields` in se-client).
