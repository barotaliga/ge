---
title: A mapped lake's ground is the generator's, not the elevation model's flat lake
date: 2026-11-01
scope: plugins/mountain/tarn/mapped.cpp
concepts: [mapped-lake, real-face, surface, banks, water-box, shore-mask, overdraw]
---

The elevation model reads a lake flat, but a real face's map is the generator's mountain laid
over the face: alpine-1's largest lake has 25 m of ground across its shore. A surface at the
shore's lowest ground cut a 34 m pit, drawn as dark rock walls in the world views. Set the
surface low in the ground under the shore (its 30 % share), cut the rest to it, and grade the
banks to the ground round them with a slope cap (`clamp(y, rim - k·d, rim + k·d)`), which needs
no blend width. Two traps from the drawing side: the water surface is a plane over the body's
whole box, so an irregular lake's box takes in lower ground past its shore (the shore mask
keeps it in); and the `world` lab's overdraw probe reads the frame's last depth, which the water
writes, so it must be read from a frame drawn with the water hidden. Fall Line's frame froze one
lake only (`ice_body`); a map with several needs a sheet a body.
