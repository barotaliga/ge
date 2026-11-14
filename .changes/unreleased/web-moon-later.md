---
type: Changed
title: Web viewers fetch the moon's face only when a night sky shows it
---

A web build leaves the moon's albedo map out of its wasm and writes it beside the viewer as `moon.data` (`cmake/WebMoonFace.cmake`). The viewer fetches it the first night the moon is up, drawing a plain disc with the phase's light until then (`render::fetch_moon_face`, `set_moon_face`). Shooter's Edge's web viewer's first visit drops from 1.84 MB to 1.36 MB gzipped and its in-browser player's from 3.10 MB to 2.61 MB; native builds are unchanged.
