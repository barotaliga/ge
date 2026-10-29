---
title: Another world's moon is up only on some nights; find its day and hour before shooting it
date: 2026-10-29
scope: engine/render/night.cpp, engine/render/wgsl/moons.wgsl, engine/render/moon_maps.cpp, games/fall_line/app/views.cpp
concepts: [moons, other-moons, orbit, lab-check, aim-at-moon, as-seen, airlight, moon-maps, mosaic, no-data]
---

A moon whose period is near the sky's day hardly moves against it. Io's 1.77 days leave it about 157 degrees a day behind the stars, so on seed 38's own day (30) it never rises at night. A lab shot at the map's hour often shows only ground. Before shooting, scan days and hours for an elevation over 15 degrees and a phase of 30 to 100 degrees. A throwaway test that calls `night_look` over `--day` and `--hour` and prints each moon's elevation and phase takes seconds. Then point the `world` lab's `moon1`..`moon4` or `moons` views at it with `--day` and `--hour`.

Draw another world's moon the way the Earth's moon is drawn: add its light on top of the sky and hide only the stars behind it (`on_moon`'s cover). Compositing it over the sky as an opaque body painted its dark side black against a bright sky, such as the hell type's glow. The air's light lies in front of the moon, not behind it.

A procedural face never looked like a moon. The real ones are a download away: the USGS Astrogeology mosaic bucket (public domain) holds global mosaics of most named moons as uncompressed 8-bit GeoTIFFs, which PIL reads with `Image.MAX_IMAGE_PIXELS = None`. Check each one for no-data before baking it. Zero texels are gaps, and a whole hemisphere can be missing (Miranda, Triton), so fill gaps by reflecting the ground about the data's edge, not with a row mean, which leaves bands. Some dark areas are real but badly lit imagery: Phobos's SRC mosaic has a black lobe where its Viking mosaic is clean. A grey map, normalised to its mean, plus a small colour-over-luminance map is about 300 KB a moon. `render/moon_maps/SOURCES.md` lists the recipe.
