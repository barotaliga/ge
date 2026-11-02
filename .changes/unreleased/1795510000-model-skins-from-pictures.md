---
type: Added
title: Model skins from PNG or TGA pictures with alpha, models and sounds behind the game adapter
---

Shooter's Edge reads every model and a demo's sounds through the classic game's adapter (`open_model`, `open_sound`), so a later game adds its own readers without touching the window. A PNG or TGA beside a model (`progs/player.mdl_0.png`) stands in for that skin, and its see-through texels are not drawn (the morph pass's new `kMarkHole`).
