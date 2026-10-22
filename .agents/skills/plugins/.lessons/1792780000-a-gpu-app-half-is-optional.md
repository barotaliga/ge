---
title: A presentation half that needs the GPU is built only when the GPU is
date: 2026-10-22
scope: cmake/Plugins.cmake
concepts: [plugin, app, ge-add-plugin, gpu, optional, tests, packaging]
---

`mountain/app` links `render`, which exists only with `GE_GPU`, while the release and packaging
builds run with the GPU off. `ge_add_plugin` therefore skips a half whose APP list names a target
this build does not make, a half's shaders are embedded under `if(TARGET ge_<name>_app)`, and a
test that includes `<plugin>/app/` is skipped when that half is missing (tests/CMakeLists.txt
reads the includes). Check a new half with `-DGE_GPU=OFF` as well as on: the half must vanish
cleanly, not fail the configure.
