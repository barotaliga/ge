---
title: A sequel recording's effect bits are not the first game's: read them through sequel_look
date: 2026-11-15
scope: games/shooters_edge/client/demo_video.cpp
concepts: [sequel, effects, render-fx, shell, glow, gib, muzzle-flash, demo-video, morph-shell]
---

The demo path copied a sequel entity's `effects` straight into `Placed::effects`, which the
window's effects read as the first game's bits: the sequel's gib bit (2) is the first game's
muzzle flash, so every flying gib lit up like a gun, and its BFG (128) and hyperblaster (64)
lit as the first game's red and blue glows. A sequel entity now goes through
`client/sequel_look.hpp` (`sequel_placed`): its own bits become the look (shell, pulse, spin,
see-through) and `Placed::effects` is cleared; its shots come from its bits (`sequel_missile`),
not from model names, since the sequel's models never matched the first game's names. A shell
is a second `MorphDraw` with `shell_push` set (the morph shader pushes along the normal and
paints one flat colour in the scene's white); its colour and push ride the draw's spare
`skin.z`/`skin.w`, so `GpuDraw` stays 256 bytes. Before trusting a demo picture, scan the
recording for the bits you changed (a scratch test over `next_frame`): demo1 of the full game
has gibs and bolts at 24-40 s, demo2 grenades at 27-32 s, and neither has a power-up.
