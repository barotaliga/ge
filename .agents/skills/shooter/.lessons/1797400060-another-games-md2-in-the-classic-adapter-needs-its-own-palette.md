---
title: Another game's md2 opened by the classic adapter needs its skin's own palette, and its view model its frames picked
date: 2026-11-14
scope: games/shooters_edge/client/classic_adapter.cpp, games/shooters_edge/profiles/remix_arsenal.hpp, games/shooters_edge/client/main.cpp
concepts: [classic-remix, cr-items, md2, palette, held-weapons, view-model, arsenal]
---

`parts::open_model` reads a second-family model's skins as palette INDICES (the pcx's own palette
is thrown away), so the classic adapter paints them through the classic palette: the sequel's
brown shotgun came out blue. The classic adapter's `open_model_` now puts each md2 skin through
its own palette as a stand-in picture (`OpenedModel::skins`), which both the world's models and the
held set take. A sequel view model holds every animation in one file (up, fire, idle, down); the
classic hand takes frame 0 as rest and the rest as the shot, so keep only the idle frame and the
firing frames (`profiles::held_frames`). The classic rules need no change to wear another game's
weapons: they read the arsenal rows from `arsenal_weapon<N>`, so `listen.opt.arsenal` with the
other game's names, `listen.opt.files` on the remix's files (played over other) and the window's
models and sounds read through the same files are the whole switch; it applies from the next map.
