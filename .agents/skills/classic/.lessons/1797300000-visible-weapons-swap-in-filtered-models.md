---
title: Visible weapons are swapped in where the window filters models, so every client path gets them
date: 2026-11-19
scope: games/shooters_edge/client/world_models.cpp, plugins/classic/vweps.cpp
concepts: [vwep, visible-weapons, filtered-models, demo-placed, listen, stand-in, model-load]
---

A carried weapon is a second model drawn on the body's frame, so it belongs in
`filtered_models` (`world_models.hpp`), which every path goes through (the listen server's
`place_models`, a demo's or a remote game's `demo_placed`). The paths only fill `Placed::vwep`;
the list's files are loaded with the model list (`ModelLoad::vweps`, appended after the effect
models) so a missing file is stood in at load time, never in the frame. Every brace-initialised
`ModelLoad` in the labs breaks (`-Wmissing-field-initializers`) when a field is added: grep for
`ModelLoad{` and `const cl::ModelLoad load{` across `games/shooters_edge/` first.
