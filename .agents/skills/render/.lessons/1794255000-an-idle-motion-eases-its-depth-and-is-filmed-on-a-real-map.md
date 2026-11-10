---
title: An idle motion that draws a new depth each cycle eases to it, or the gun jumps at every breath and every stop
date: 2026-11-09
scope: engine/viewmodel/place.cpp, engine/lab/labs/viewmodel_lab.cpp, games/shooters_edge/labs/gpu_se_client_lab.cpp
concepts: [viewmodel, breath, idle-sway, exertion, easing, chart, film, gif]
---

- A breathing gun drew a fresh depth at the start of each breath, where the curve sits at its
  lowest, so every new breath (and every stop, when the moving share jumped back to 1) was a step
  of a millimetre or two. Chart it first (`labs viewmodel`'s `breath`): the steps were plain on
  the line and invisible in a still. Ease the amplitude to its target over most of a second;
  keep the phase itself continuous by integrating the rate rather than multiplying a clock.
- A motion a few millimetres deep is only judged moving: `labs se-client --game=<pack> --hand=1
  --film=<n> --film-step=<s> --exertion=<0..1>` films the classic weapon on a real map, with the
  HUD, from rest or as after a run.
- A debug build's `make build` can loop forever on "build.ninja still dirty": the configure left
  build.ninja unchanged and older than `CMakeFiles/cmake.verify_globs`. `rm build/debug/build.ninja
  && cmake --preset debug`, then `ninja` in build/debug; and build only the targets you need, the
  test binaries fill the disk.
