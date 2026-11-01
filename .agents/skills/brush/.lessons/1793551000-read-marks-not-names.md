---
title: Downstream of the reader, read a texture's marks, never its name or a family's flags
date: 2026-11-01
scope: plugins/brush/map.hpp, plugins/brush/read_v29.cpp, plugins/brush/read_v38.cpp, plugins/brush/read_v46.cpp, plugins/brush/app/draw_data.cpp, plugins/brush/rebake.cpp
concepts: [texture-mark, sky, liquid-kind, teleport, next-frame, family, renderer, light-bake]
---

`brush::Texture` carries `sky`, `liquid` (a `Liquid` kind) and `next` (the animation's next frame), set by each family's reader: 29 from names (`liquid_by_name`), 38 from the texinfo's flags, its chain and the brushes' contents, 46 from the shader's flags and contents. Code past the reader reads those marks, so a new family is a reader change only. Animation goes through `build_texture_anims` (anim.hpp), which handles names and chains alike. Do not write a new `name.find("lava")` or `rfind("sky", 0)` anywhere downstream.

The marks are not in `map_digest`, because each one follows from what is read. The v38 fixture writer sets the sky and warp flags and the `+<digit>` chain from the fixture's names, so a v38 fixture shows the marks. The sample fixture has no such names, so the golden bytes did not move.

To prove a refactor like this, run the labs that reach the renderer (bsp, gpu-bsp, gpu-bsp-fog, gpu-bsp-weather, se-world, se-client, classic-climate, rooms, enhance) with `--smoke --check-digests=tests/digests`. `ge digest --affected` did not find them for a change in plugins/brush.
