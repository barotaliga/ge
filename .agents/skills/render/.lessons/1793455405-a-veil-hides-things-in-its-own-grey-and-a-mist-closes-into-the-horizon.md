---
title: A fog hides things in its own grey, a reach's mist closes into the horizon's sky, and a sky type colours only what it reaches
date: 2026-10-23
scope: engine/render/wgsl/frame.wgsl, engine/render/fog.wgsl, engine/render/wgsl/valley_fog.wgsl, engine/render/sky_style.cpp, engine/render/wgsl/atmosphere.wgsl
concepts: [fog, mist, haze, veil, overcast, deck, exposure, reach, valley-fog, dust, sky-type, noise, wrap-lighting]
---

- Under a deck, key a white face lower (0.6, not 0.78), or snow and deck are one white sheet.
- Droplets and flakes scatter every colour alike, so what a fog or a fall hides fades to grey,
  and the sky must be seen through the same veil. Closing into the clear sky's blue was wrong at
  a glance; put a new weather beside the clear day's and ask what colour hidden things fade to.
- The draw distance's mist closes into the horizon's sky (the direction raised to it), not the sky
  behind the pixel (a dark under-horizon sea). Keep it out of `haze`, which the clouds call too
  (`haze_air` is the air alone), and make the sky pass draw the same colour where no surface is.
  Check the vista, aerial and lookback at every stop.
- A ground mist measures its height up from the terrain (`FogVolume::hug_ground`); a level box
  on a slope misses. Judge a fog in sun (the `fog` lab at 11 h).
- A medium's top lit off a noise takes the coarse octave's slope over two texels and a wrapped
  cosine, or a grazing sun turns it into camouflage.
- A dust sky needs a scale height near the air's (10 km) and asymmetry 0.55 to 0.65, or it stays
  below a mountain eye and only glows round the sun. A type cannot be "dim": the exposure keys to
  white, so darkness is absorption in every channel. Tune in the `sky` view, clear, at noon too.
