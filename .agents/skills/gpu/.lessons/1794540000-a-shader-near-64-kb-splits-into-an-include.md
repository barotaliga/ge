---
title: A shader grown near 64 KB splits its shared half into an include and its new pass into a file
date: 2026-10-31
scope: cmake/WgslEmbed.cmake, engine/render/liquid.wgsl, engine/render/wgsl/liquid_body.wgsl
concepts: [wgsl, embed, msvc, string-literal, 64-kb, split-shader, include, module-cmake]
---

The embedding fails a shader whose resolved text reaches 64 KB (one MSVC string literal). When a
new pass would push a file over, move what both passes read (the uniform's struct, the bindings,
the noise, the helpers) to `engine/render/wgsl/<name>_body.wgsl`, include it from both, and give
the new pass its own `.wgsl` with its own module (`gpu::create_shader` on it for that pipeline).
The new file must also be listed in `engine/render/module.cmake`'s embed list, or its generated
`<name>.wgsl.hpp` is never written and the include fails at compile time.
