---
type: Added
title: Particles and the skis' spray
---

`engine/render/particles.hpp` is a pool of GPU particles any game can fill: a ring of births, the flight in a compute pass (gravity by each particle's share, a drag toward the still air, the terrain's ground burying a grain and settling a fine one) and a draw that lays each particle as a streak along its motion, softened against the depth, lit by the sun, the shadows and the sky, and kept out of TAA's smear. `plugins/skier/app/spray.hpp` reads what a skier throws off his state each frame: the sheet of a skid and of a hockey stop's brake, the curl off a carving ski's tail, powder fines that hang, the burst of a landing and the snow round a skier who is down. Fall Line throws every racer's spray into the pool, and the new `spray` lab shows a hard carve, a hockey stop, a landing and powder turns from the chase camera and from the side.
