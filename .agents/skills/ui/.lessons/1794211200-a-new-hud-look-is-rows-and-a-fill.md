---
title: A new HUD look is its own rows in the table plus a fill, and the se-hud lab is its layout check
date: 2026-11-09
scope: games/shooters_edge/hud/layout.cpp, games/shooters_edge/labs/se_hud_lab.cpp
concepts: [hud, looks, hud-style, digit-cells, strip, parent, attach, layout-problems, hud-sbar, panels, hud-swap, mirror, lines, hidden-parent]
---

A look is a bit in each row's `looks` mask: share a row when the element sits at the same place
(the crosshair, the feed, the frags), else add rows with the look's prefix (`arena_`, `tac_`,
`min_`) so every place stays the player's own `hud_<id>_` cvars. Hang a look's pieces from one root
per anchor: two roots on one anchor stack, a child never does. An option that hides a panel but keeps
its pieces (`hud_sbar 0`) keeps the panel's row on with its colour's alpha at 0, since the pieces
hang from its box. Prefer an option over a look when it changes one thing across looks (the
panels, the side, the guns' grouping): a look per combination multiplies rows. A child of a hidden
parent hangs from the nearest shown ancestor, so a strip that may be switched off (the stock) can
still carry the strip over it. A tall multi-line strip under the crosshair crowds a landscape
phone: put it in a corner. The lab checks every option over every look (`kOptions`). A `digits` element right-aligns in `digit_cells` cells: set it to 0 when the figure
sits beside a portrait or under the crosshair, or a short figure leaves empty cells. A strip
stretches each picture into `cell_w` x `cell_h`, so mixed-width pictures (the guns) take 0 for the
picture's own size. `make lab LAB=se-hud` names every box off the screen or overlapping at five
sizes: run it after every move, then shoot the looks on a real map (`labs se-client --game=<pack
folder> --map=e1m2 --hud=classic --hudcvars="hud_style <look>"`).
