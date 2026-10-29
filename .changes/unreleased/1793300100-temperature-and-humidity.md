---
type: Added
title: Temperature and humidity
---

The air's temperature and moisture for any game (`engine/wind/climate.hpp`, `mist.hpp`, `cold.hpp`; docs/climate.md): `air_at` gives the temperature, dew point, frost point, humidity and wet bulb at a place and hour from a day's `Climate` (the diurnal curve, the lapse with height, the night's cold pool in low ground, water and wet ground moistening the air); `mist_at` lays radiation, valley, hill, steam and advection fog with its density, depth and visual range; `rain_share_of` and `snow_level` decide snow or rain by the wet bulb; and `cold.hpp` gives dew, hoar frost, rime, black ice, the breath's cloud, lake ice by Stefan's law and what it bears, new snow's density, a ski's glide and the snow's feel. The `climate` lab pictures a valley's night, where fog lies, and each effect.
