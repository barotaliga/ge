---
type: Added
title: Snow over time
---

A snow layer given a `Weather` (`engine/snow/time.hpp`: schedules for the fall, the air and the wind) changes with the clock, read in closed form rather than stepped: new snow, settling, tracks filled by the fall and the drift, melt and refreeze into spring snow, crust and ice, and a crust that bears what loads it under `crust_load` and breaks under more (`Pressed::broke`). `groom` levels a swath with its mass kept to the unit and sets the corduroy flag. `sample(layer, x, z, t)` reads the weathered column; a layer with no weather reads exactly as before. The new `snow-weather` lab charts all of it.
