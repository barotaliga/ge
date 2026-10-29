---
type: Added
title: A body falling into water answered by its size
---

`water/entry.hpp`'s `entry_of` sizes what a body does to the water off its mass, volume, footprint and speed: a crater with a raised rim that opens and closes, the jet thrown up as it closes, rings of the water it pushed aside, and a crown of drops (`entry_spray`, `jet_spray`). The crater is the ring's near field, so `sample`, a float and the drawn surface all see it. A pebble ripples; a player jumping in opens a crater over a metre across.
