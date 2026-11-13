---
title: The sequel's menu plates are monospaced cells; build a missing word as a plate of whole-height cells
date: 2026-11-13
scope: games/shooters_edge/menu/classic_view.cpp, games/shooters_edge/client/sequel_adapter.cpp, games/shooters_edge/labs/gpu_se_attract_lab.cpp
concepts: [carved-letters, plates, sequel-menu, strip-print, se-attract, demo-none]
---

The sequel's main menu words (`pics/m_main_*.pcx`, 32 px tags) are set on an even 16.8 px pitch
from x 19: cell k runs from 19 + round(16.8 k) to the next. A word the plates lack is a plate of
its own: GAME's left end (0..19), a whole-height cell a letter (the plate's edges run on across
the cells), GAME's right end (86..99). Find letter runs by thresholding brightness (about 50) in
rows 8..24 and plain columns by the lowest maximum over rows 2..26: only QUIT's 66..68 is clean
enough to repeat as a space. Missing letters are pieced over a cell: hide a stroke with a patch
of the recess's plain ground (OPTIONS' O interior at 24, 10). Preview with a scratch script that
composes the pieces before writing `kPlate*`, and hash the five plates' indices in the adapter so
the pieces are cut only from the pictures they were measured on.

The sequel's recordings cannot play yet, so `se-attract --demo=none` draws the pages on the CPU
over a dark ground instead of over a played frame.
