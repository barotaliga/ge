---
title: make libm-check scans every games/*/labs file, GPU labs included
date: 2026-10-21
scope: games/fall_line/labs, tools/check-libm.sh
concepts: [libm-check, transcendental, gpu-lab, cmath]
---

A game's labs sit beside its simulation, so `tools/check-libm.sh` holds them to the
simulation's rule: no `std::tan`, `std::sin` and the like, even in a GPU lab that only
draws a picture. Take angles from what the engine already computed (a `render::sun_direction`
vector gives a tangent as y over the plan length, `std::sqrt` is allowed), or call
`ge::core::libm`. Run `make libm-check` before pushing any lab under `games/`.

`make libm-check` reads `git ls-files`, so a NEW lab file passes it locally until it is staged:
run it after `git add` (the `taa` lab's `std::sin` reached CI this way).
