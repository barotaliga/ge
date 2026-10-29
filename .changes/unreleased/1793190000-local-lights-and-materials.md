---
type: Added
title: Local lights and physically based materials
---

Local point and spot lights in candela with flicker, pulses and step patterns, clustered so a scene's hundreds of lamps cost a pixel only the few that reach it (`render/lights.hpp`), and one physically based surface model (GGX over Lambert, metal and dielectric, emissive) that instance families take as `render::Surface`s (`render/surface.hpp`); `FrameView::lamp_lux` lets the exposure follow lit scenes at night.
