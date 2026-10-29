---
title: A function added to frame.wgsl's includes collides with any shader's own of that name
date: 2026-10-28
scope: engine/render/wgsl/frame.wgsl, engine/render/wgsl/surface.wgsl
concepts: [wgsl, include, name-collision, pipeline, shaders-check]
---

Every draw's shader includes frame.wgsl, and the include is flattened into one module, so a
function added there shares one namespace with every shader in the tree. The surface model's
`fresnel` met water_light.wgsl's own and the water's pipeline failed to build, which surfaced only
as `pipelines.finish` false and a crash on a null pipeline in `render_water_test`. Prefix a shared
function with its file's subject (`surface_fresnel`, `lamp_irradiance`), grep the tree's `.wgsl`
for the name before adding it, and run `make shaders-check` or every render test after touching
frame.wgsl's includes.
