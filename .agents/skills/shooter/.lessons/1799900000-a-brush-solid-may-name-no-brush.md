---
title: A brush solid may name no brush model; check the index before reading the hulls
date: 2026-11-27
scope: plugins/shooter/trace.cpp, plugins/shooter/client.cpp
concepts: [traced-by-brush, modelindex, use-after-free, asan, solid-bsp]
---

`Server::models` holds only the map's brush models, but a program can set an entity solid as a
brush while its `modelindex` names something else (an alias model, a model in its own file). Read
past the end, the hulls are garbage: the crash showed only on another release's maps and only on a
Mac, and ASan reported it as a use after free because the bytes past the vector belonged to a freed
neighbour. Every reader of a brush solid's hulls goes through `traced_by_brush`, and anything else
is met as its box. When a crash shows only on the Mac, ask for an ASan run there first: Linux could
not reproduce it at all.
