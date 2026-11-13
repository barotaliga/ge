---
title: Another game's figure plays this game's poses by its frames' names, not their numbers
date: 2026-11-13
scope: games/shooters_edge/app/figure_frames.hpp
concepts: [player-figures, frame-map, figure-frames, md2-in-first-game, frame-names]
---

A figure copied in from another game has its own frame numbering (one game's runs start at 0,
6, 12 ...; the other's stand is 0-39, run 40-45), so the server's frame number means nothing to
it. Both files name their frames, so the kind of each run comes from its name's stem (stand, run,
attack, pain, death) and the n-th run of a kind plays the other figure's n-th (mod its count),
stretched across. One trap: a frame number of three digits or more carries a variant in its first
digit (`pain203` is the second pain's third frame); strip every digit and the three pains (and the
three deaths) merge into one run of twelve.
