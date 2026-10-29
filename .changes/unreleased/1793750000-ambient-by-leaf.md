---
type: Added
title: Ambient sound by leaf
---

`brush::Leaf::ambient` carries the water, sky, slime and lava levels a 29-family or BSP2 map stores per leaf, and `audio::AmbientBed` plays looped layers whose levels walk to the game's targets at a fixed rate, so a sound swells and fades rather than switching. Shooter's Edge plays the map's water and sky ambience off the leaf the listener stands in. New lab: `se-ambient`.
