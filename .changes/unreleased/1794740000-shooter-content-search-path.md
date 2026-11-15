---
type: Changed
title: The classic search path and load order are the shooter plugin's
---

The search path over a classic install (base, bundle and mod folders; packs, zips and loose files in each game's own order; saves, the palette, a map's light, sky boxes, the release's words by key) and each classic game's load order move from Shooter's Edge's client to `plugins/shooter/content/` (`ge::shooter::content`), so any shooter game on the engine opens any classic game's files through the same calls. The bundled client's folder is now the load order's data (`LoadOrder::client_folder`). Shooter's Edge's behaviour is unchanged.
