---
title: A pass that has nothing to draw this frame still opens, or the GPU times shift rows
date: 2026-10-22
scope: engine/render/shadows.hpp
concepts: [pass-table, recorder, gpu-ms, timestamps, cached, cascade]
---

The frame recorder indexes each pass's GPU time by the order the passes open, so a cached pass
that is skipped on a frame (a cascade not due) moves every later row's timing onto the wrong
name. `encode_sun_shadows` opens the pass anyway, with the depth attachment read-only and no
draws: it costs nothing and keeps the table's rows and the recorder's slots in step. Do the
same for any pass that is only sometimes needed.
