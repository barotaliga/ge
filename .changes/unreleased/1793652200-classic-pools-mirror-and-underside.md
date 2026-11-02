---
type: Changed
title: A classic map's water mirrors its room, lies still indoors and is seen from below at any depth
---

Under classic-enhance a map's pools now mirror the room they lie in (the map drawn again in the water's plane), not our daylight sky where the screen holds nothing, and the sun glitters only where the mirror shows the sky. Each pool's depth, its share of the weather's wind (`sv_wind`, `sv_winddir`: a pool in a closed hall lies still, one under a window facing the wind ripples) and its room's light are read off the map. Gusts break wind-blown water into patches of ripples and calm, and still water keeps a faint wandering stir. The surface seen from under the water is drawn from its real waves: Snell's window with the whole room above bent into it, total internal reflection mirroring the pool past it, from any depth (the eye is in the water whenever the map says so, at the bottom of a deep pool and in a flooded passage). Engine: `render::WaterViews` and `set_water_views`, the `water.below` pass, `WaterDraw::gusts`, `stir` and `daylight`, and `water::lay_waves` with a body's own wind.
