---
type: Added
title: Sunlit water throws caustics from its real waves and light shafts under it
---

The sun's light the surface focuses now plays over what lies under the water, from the waves' own curvature: a net of bright lines on a sunlit bed, sharp near the surface and soft deep down, seen from below and through the surface from above, none under still water. Under the surface the same focusing lights shafts that fan down toward the sun and sway with the waves. Both appear only where the sun reaches the water: a classic map's pools are traced toward the sun through the map, so a pool in a wall's shade under the open sky stays unlit. The surface seen from below no longer hides the bed under its coarse rings. Engine: `UnderwaterLook::shafts` (and `caustics`, now on in every look but the classic tint), `WaterDraw::sunlit` and `render::set_water_sunlit`.
