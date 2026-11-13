---
title: A classic menu row with no words in the pack is cut from the strips' letters; a demo frame needs its fx run up
date: 2026-11-12
scope: games/shooters_edge/menu/classic_view.cpp, games/shooters_edge/labs/gpu_se_attract_lab.cpp
concepts: [carved-letters, carved-words, strip-print, select-game, se-attract, arrival-flash]
---

A new row on a classic root (SELECT GAME, DEMOS) reads as the original only when its letters are
cut from the pack's own strips: dump `mainmenu.lmp` and `sp_menu.lmp` to PNG in scratch, crop each
word at 10x with a grid every 5 px, and take whole letters whose outline touches no neighbour
(the font's O is two halves, so a c is the left half of a small o). Pieces go in `kCarvedWords`
under the strips' print; `sp_menu`'s strip sits a pixel higher (dy 1). The menu test counts the
pieces drawn from `sp_menu`, so a new carved word changes that count.

A lab drawing one frame of a played recording must step the effects (`demo_fx_step`) from time 0
up to the picture's moment: called once at its time, the local player's arrival flash is at its
brightest and the whole frame is white.
