---
type: Added
title: The sequel's server sends each client only what it may see
---

The sequel's server now cuts each client's frame to the entities its eye may see (the visible rows round it, beams by the hearable row, effects within 400 units), over our own rules and a hosted game library alike: on q2dm1 an eye is sent a quarter of the map's entities on average and a whole frame's entity list shrinks from 1722 bytes to about 400. Directions go as their index into the protocol's table of 162 (`classic/protocol/directions.hpp`), so a hosted library's sparks and splashes fly their way and our client reads the way back. A hosted library's turned brush entities are traced and tested in their own frame. New lab `se-sequel-cut`; `brush::box_leaves`, `clusters_seen_from` and `clusters_heard_from`.
