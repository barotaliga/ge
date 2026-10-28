---
title: At night a FrameView's sun is the moon; read the real sun from Night
date: 2026-10-28
scope: games/fall_line/app/frame.cpp, engine/render/night.cpp, plugins/mountain/app/cloud_field.hpp
concepts: [night, moon, key-light, sun-elevation, dusk, clouds, scud, low-level-jet]
---

Once the moon outshines the twilight, `set_night` makes it the KEY LIGHT. `FrameView::sun_elevation` and `sun_azimuth` then hold the moon's, and the real sun's move to `Night::sun_elevation` (`Night::moon_key` says which is which). Anything that asks "how far has dusk fallen" must read `view.night.moon_key ? view.night.sun_elevation : view.sun_elevation`. Reading the view's sun at night saw the moon 52 degrees up and dealt a daytime sky, with no evening jet and no scud. The bug showed only in pictures. The cloud field's tests passed, because they hand `dusk` in directly.

To see motion, shoot a sequence (the `world` lab's `--at=0,0.25,…`) with the wind set ACROSS the view (`sv_winddir` near the view's heading minus 90 degrees). A wind along the line of sight moves a cloud toward or away from the lens, and a sequence of it looks still.
