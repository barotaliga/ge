---
title: A night is stored through a pre-exposure, keeps the sun's sky beside the moon's, and its key light is the moon
date: 2026-10-23
scope: engine/render/night.hpp, engine/render/night.cpp, games/fall_line/app/frame.cpp, plugins/mountain/app/cloud_field.hpp
concepts: [night, moon, pre-exposure, half-float, key-light, sky-view, handover, twilight, dusk]
---

- A night is a millionth of a day, below a half float's smallest normal step. The frame stores
  light through a pre-exposure (the light on white brought to about a quarter unit, 1 by day so
  no day bit moves), and every absolute reading (the eye, a star) divides it out.
- Keep the sun's sky table always and add the moon's as a second, summed; swapping one table at a
  handover jumped the picture 35 %. Hand the KEY light over only once the sun is three degrees
  under every eye; the jump fell to 0.1 %. When a handover jumps, first check the lab aims both
  shots the same way.
- Once the moon is the key light, `FrameView::sun_elevation` and `sun_azimuth` hold the MOON's;
  the real sun is `Night::sun_elevation` (`Night::moon_key` says which). Anything asking how far
  dusk has fallen reads `view.night.moon_key ? view.night.sun_elevation : view.sun_elevation`.
  Reading the view's sun dealt a daytime sky at night, and only pictures showed it, because the
  cloud field's tests hand `dusk` in directly.
