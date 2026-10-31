---
title: A faint sky body is drawn as the eye sees it, found by its day and hour before a shot, and read from a table
date: 2026-10-23
scope: engine/render/sky.wgsl, engine/render/stars.hpp, engine/render/night.cpp, engine/render/wgsl/moons.wgsl, engine/render/moon_maps.cpp
concepts: [moon, earthshine, eclipse, as-seen, stars, star-catalogue, cube-map, other-moons, moon-maps, lab-check]
---

- Earthshine is thousandths of the lit limb and an umbra a thousandth of the penumbra: draw each
  faint part as its ratio to the disc's brightest point to a power (0.35, 0.5), capped at what it
  would be alone. A perceptual post pass (the rods' desaturation) then misreads it as dim; exempt
  the disc by direction rather than pre-compensating (an eclipse turned magenta).
- Shoot a disc close-up under a clear sky and measure its brightest point, not its middle (a
  crescent's centre is dark). Aim at a known figure computed from the sky's own turn.
- Stars are a 6 x 160 x 160 cube of cells, three slots each (one lost hundreds, three lost 19
  faint ones), RGBA8 at the table's quantisation, read three by three so an edge star is whole.
- Another world's moon near the day's period may never rise on the map's day: scan `--day` and
  `--hour` (a throwaway test over `night_look`) for elevation over 15 and phase 30 to 100 degrees,
  then shoot `moon1`..`moon4`. Add its light over the sky and hide only the stars behind it; the
  air's light lies in front of it.
- Real moon maps (public-domain global mosaics; `render/moon_maps/SOURCES.md` has the recipe):
  zero texels are gaps, fill them by reflecting about the data's edge, not with a row mean.
