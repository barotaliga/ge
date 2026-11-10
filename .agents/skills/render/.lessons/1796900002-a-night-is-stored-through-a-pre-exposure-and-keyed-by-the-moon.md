---
title: A night is stored through a pre-exposure, keyed by the moon once, and its faint bodies are drawn as seen
date: 2026-10-23
scope: engine/render/night.hpp, engine/render/night.cpp, engine/render/sky.wgsl, engine/render/stars.hpp, engine/render/wgsl/moons.wgsl, engine/render/moon_maps.cpp, games/fall_line/app/frame.cpp, games/shooters_edge/client/title.cpp, plugins/brush/app/bsp_sky.cpp
concepts: [night, moon, pre-exposure, half-float, key-light, set-night, eclipse, umbra, earthshine, stars, moon-maps, handover, twilight]
---

- A night is a millionth of a day: the frame stores light through a pre-exposure (white at about
  a quarter unit; 1 by day so no day bit moves) and every absolute reading divides it out.
- Keep the sun's sky table and add the moon's (swapping at a handover jumped 35 %); hand the key
  light over once the sun is 3° under every eye. Then `FrameView::sun_*` hold the MOON: dusk is
  `view.night.moon_key ? view.night.sun_elevation : view.sun_elevation`.
- `set_night` moves the moon into `sun_*`, so call it ONCE a frame with every `Moon` field (an
  eclipse's `shadow`, `umbra`, `penumbra`): a second call reads the moon as a risen sun.
- Faint parts (earthshine, an umbra) are drawn as their ratio to the disc's brightest point to a
  power (0.35, 0.5), and exempted from the rods' desaturation by direction. Measure a disc's
  brightest point, not its middle. An eclipsed moon keys the eye near 0.001 cd/m².
- Another world's moon may never rise on the map's day: scan `--day` and `--hour` first. Stars
  are a 6 x 160 x 160 cube, three slots a cell. Moon-map gaps are filled by reflecting about the
  data's edge (`render/moon_maps/SOURCES.md`).
