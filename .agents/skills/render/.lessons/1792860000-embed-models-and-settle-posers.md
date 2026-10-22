---
title: A game's model is compiled in, and a frame's posers settle on a still
date: 2026-10-22
scope: games/fall_line/app
concepts: [skinning, skinned-mesh, embed, glb, poser, reach-box, figures]
---

A model a game draws goes into the binary with `ge_embed_binary` (cmake/EmbedBinary.cmake) and is
read with `assets::load_gltf(bytes, name)`, so the window and every lab run from any directory.
A skinned family holds one mesh a racer, each with its own `Skin`, and the frame poses only the
racers drawn this frame (`encode_skinning` over the first `racers` skins). Keep the reach fixed
and held by a test (`skier::pose_reach` over the ride scenarios): a reach measured off one pose
would clamp the next one on the GPU. A poser's lags belong to the run, so a lab's still frame or
a camera jump passes `settle` to start every poser afresh, and the window passes the steps' dt.
