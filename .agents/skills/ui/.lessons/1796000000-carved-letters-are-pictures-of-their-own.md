---
title: Set a missing carved word from letter pictures cut at load, not from runs of the strips
date: 2026-11-13
scope: games/shooters_edge/menu/carved_letters.cpp, games/shooters_edge/menu/classic_view.cpp
concepts: [carved-letters, letter-pictures, speck, connected-parts, plates, plate-surface]
---

Pieces copied as rectangles from the classic strips drag in neighbours' pixels and land at
different heights. Cut each letter once at load into its own picture (`cut_letters`): a box per
piece, a 4-connected labelling that keeps only parts of at least 6 classic px (at the pack's
density squared), each piece set at its row on a common 20 px line; then a word is code (a pixel
between letters, a space 6 px). The strips' first band sits a pixel higher than the others, so its
pieces go one row lower. For the sequel's plates: cells of different pictures differ in surface
brightness, so shade every piece to the word's darkest surface; and a slip drawn as a plain space
must have plain columns on BOTH sides of it too, because the sampler's filter reaches one past the
box (a clean 3 px slip next to a serif still showed a tick).
