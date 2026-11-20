---
type: Added
title: Quake II's server holds each client to its rate
---

A client of Quake II's server is sent no more than its `rate` asks (none: 5000 bytes a second; 100 to 15000), held further to the engine's `sv_minrate` and `sv_maxrate`: a player out of those bounds is told so and set back, as in Quake. When the last ten frames sent add up to more than the rate, the next frame is held back, and the frame after it tells the client how many were held.
