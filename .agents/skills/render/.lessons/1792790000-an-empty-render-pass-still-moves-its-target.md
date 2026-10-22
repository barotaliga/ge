---
title: A render pass with nothing to draw still loads and stores its whole target
date: 2026-10-22
scope: plugins/mountain/app/trails.cpp
concepts: [render-pass, load-op, store-op, empty-pass, bandwidth, trails, timing-row]
---

The fine trail map's pass opened every frame to keep its timing row, with nothing to stamp on
most frames, and cost 5.8 ms on the software adapter: a render pass loads and stores every texel
of its 2048 x 2048 attachment whether it draws or not (on a tiling GPU too, a whole target's worth
of bandwidth). Opening an empty compute pass under the same name keeps the row and costs nothing.
Open the render pass only when it draws or clears; the frame's pass table still holds the order.
