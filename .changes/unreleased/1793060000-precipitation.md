---
type: Added
title: Snow, sleet and rain on the wind
---

`engine/wind/precip.hpp` says what falls and how hard for any game: one dial from none to a
storm and its rain share, the rate in mm/h, the tiers (light, moderate, heavy, storm), snow above a
freezing level and rain under it, the drops' and flakes' pace, how far one sees through it, and the
wet ground's film and puddles. The falling snow is now `engine/render/precip.hpp` and draws rain
too: drops as thin clear streaks, sleet as both, every particle swirled by the wind's eddies, the
rain's splashes on whatever ground the depth shows, and a wet ground that darkens and mirrors the
sky (slush on snow). Fall Line rains under a snow line picked by hand (`sv_snowline`), with the
cvars `r_precip` (was `r_snowfall`), `r_precip_density`, `r_splashes` and `r_wet`, and the labs
`precip` and `precip-air`.
