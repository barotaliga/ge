---
title: A GPU renderer registers beside the game, so a GPU-less build keeps its play contract
date: 2026-10-22
scope: engine/play/game.hpp, games/fall_line/play/main.cpp
concepts: [renderer, renderer-spec, ge-renderer, play-executable, gpu-shot, gpu-recording, lens]
---

The play executable links the game's simulation alone, and must still build where there is no
GPU. Put the GPU's pictures behind a second registration (`GE_RENDERER(spec)` beside
`GE_GAME(spec)`), compiled in only when the game's app half is built, and reach the run's state
from the renderer through a game-side accessor (`state_of(run)`, a dynamic_cast to the game's
own Run): nothing in the CPU contract changes, and `--gpu` on a build without one answers that
it has no renderer. Open the renderer once per shot or recording (the `Lens`): a frame carries
its tracks, spray and TAA history only if one renderer draws every frame in order.
