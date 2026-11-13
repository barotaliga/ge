---
title: Measure a classic strip's rows before aligning to them; its first band sits a pixel high
date: 2026-11-13
scope: games/shooters_edge/menu/classic_view.cpp, games/shooters_edge/menu/carved_letters.cpp, games/shooters_edge/menu/games_page.cpp
concepts: [carved-letters, strips, baseline, band, descender, letter-spacing, measure]
---

Read the strips' pictures out of the pack with a scratch script and print, per 20 px band, each
ink run's columns and first and last rows. The carved font: capitals on rows 1 to 16, small capitals
on 3 to 15, the t's stem to 18, the G's tail to 22 and the Q's to 23 (past the band: a box cut at
the band's foot loses the tail). The first band of every strip is a pixel higher (small capitals on
2 to 14), so a menu drawn band by band sets band 0 a pixel lower. Between letters the strips leave
1 px (39 of 46 gaps, the rest 2); a word's space is 5 px before a round capital and 10 before a stem.
To check a menu, draw the measured lines (cap top, small top, foot) over a `--demo=none` shot at the
screen's scale; a row off by one shows at once.
