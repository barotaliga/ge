---
title: A blade's head is its own segment, and the draw's shader is near its size cap
date: 2026-10-28
scope: engine/render/grass.wgsl
concepts: [flowers, seed-heads, triangle-strip, wgsl-size, shader-split]
---

A strip has one width a vertex, so a flower widened at the tip over the last ordinary segment
draws a green flag from the stem out to the petals. Move the last segment's lower vertex up to
the head's foot (`1 - flower_size / height`) and switch the colour there, so the head is that
segment alone. The draw's shader resolves (with frame.wgsl and the shadows) to close to the 60 KB
embedding cap (cmake/WgslEmbed.cmake); the scatter's half of the shared WGSL now lives in
`wgsl/grass_scatter_common.wgsl` so the draw does not carry it. Put new draw-only WGSL in
grass.wgsl and keep shared code that the draw does not call out of grass_common.wgsl.
