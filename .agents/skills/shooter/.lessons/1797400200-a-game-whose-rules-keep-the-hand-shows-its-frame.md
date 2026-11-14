---
title: A game whose rules keep the hand shows the hand's frame, not a fire clip
date: 2026-11-13
scope: games/shooters_edge/client/held.cpp
concepts: [held-weapon, view-model, sequel, gun-frame, frame-track, weapon-look, palette]
---

The first game's hand guesses a shot from the program's `attack_finished` and plays a `fire` clip;
the sequel's rules step the hand themselves (activate, fire, idle, deactivate runs on one frame
counter), so `step_held` asks `sequel_of(rules)` first and draws `players[0].hand.frame` between
its changes with `track_frame`, as a recording's weapon frame is drawn. Its weapons outnumber our
own set (11 against 8), so anything indexed by `view::Weapon` must guard the index
(`weapon_look`). Its view models' skins are indexed into the game's palette, which lives in
`pics/colormap.pcx`: without it they draw black but lit, which looks like a lighting bug.
