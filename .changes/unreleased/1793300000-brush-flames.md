---
type: Added
title: A classic map's torches drawn as burning gas
---

The v2 BSP renderer draws each wall torch and large flame of a classic map as a `render::Flame` where its model burns (`plugins/brush/app/bsp_flames.hpp`, `bsp_fires`), flickering with the light the map casts for it, its light lifted to the gas's centre and softened over the flame's height. `PointLight::fire` names which lights are fires, for a pack that hides the classic models.
