---
type: Added
title: Light globes glow and bubbles rise as the engine draws them
---

With `ce_flares` and `ce_bubbles` on, Shooter's Edge draws the classic pack's light-globe sprite as a soft warm glow and its bubble sprite as small clear bubbles with a bright rim and a highlight, each wobbling as it rises. Which pack files the engine replaces (the explosion sprite, the flare, the bubbles, the torches' painted flames) is now read from classic-enhance's replacement table instead of being written into the game. The engine makes the two pictures at load (`engine/render/sprite_shapes.hpp`), and the mod stands a globe where a map hangs one. `se-models` shows both, with `--progs` to play the campaign variant.
