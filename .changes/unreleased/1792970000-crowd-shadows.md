---
type: Added
title: Each shadow-atlas tile culls its own casters, and Fall Line's crowd casts shadows
---

- **The engine:** the figures' shadow atlas (render/shadows.hpp) gives every tile a caster slot of its own, culled by that tile's box. A moving family makes `kMaxShadowFigures` caster slots, and a figure is drawn only into the tiles it can shade, so the atlas's cost follows the figures near the tiles rather than a family's size.
- **Fall Line:** the amateurs nearest the eye cast shadows, a tile each after the racers' (`shadow_centres`). The `crowd-look` lab takes `--hour` (11 by default, the sun on seed 38's runs).
