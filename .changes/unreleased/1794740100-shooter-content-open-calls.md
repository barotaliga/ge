---
type: Changed
title: The shared open calls for models, sounds, map textures and configs are the shooter plugin's
---

Opening a classic game's map info, map textures, models (by kind: vertex models, sprites, brush models, skeletal models), sounds, configs, place names and stand-in pictures moves from Shooter's Edge's client to `plugins/shooter/content/open.hpp`, so any shooter game on the engine opens any classic game's files with the same calls. Shooter's Edge's adapter forwards to them and behaves as before; status lines about an unreadable picture now begin "content:".
