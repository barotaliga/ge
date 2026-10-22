---
type: Added
title: GPU shots and recordings in modes
---

`ge shot --gpu` and `ge record --gpu --mode=lit|wireframe|depth|overlay` draw a run through the game's registered GPU renderer (`RendererSpec`, `GE_RENDERER` in `play/game.hpp`; the `Lens` in `play/session.hpp`), headless; the terrain and the instance families gain a wire probe (`encode_terrain_wire`, `encode_instances_wire`) for the wireframe.
