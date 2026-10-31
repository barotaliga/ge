---
type: Changed
title: Shooter's Edge's window draws the classic status bar in the packs' own pictures
---

When the game folder's packs carry the classic status bar pictures, Shooter's Edge's window now draws its classic status bar and inventory bar in them (read straight from the packs at start, no tool run first) instead of the game's free art. `hud_art auto|classic|free` picks the art (`auto`, the default, uses the packs' pictures whenever they are there) and switches live beside `hud_style`. The `se-client` lab draws the HUD over its `--game` views, with `--hud` to pick the art.
