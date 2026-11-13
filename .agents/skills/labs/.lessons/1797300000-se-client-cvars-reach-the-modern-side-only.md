---
title: se-client's --cvars reach the modern side only; the classic picture takes --before
date: 2026-11-13
scope: games/shooters_edge/labs/gpu_se_client_lab.cpp
concepts: [se-client, cvars, before, classic-look, lab-arguments]
---

The se-client lab runs two consoles: `--before` (default `r_look classic`) feeds the first, whose
pictures are `before-N` (or `classic-N`), and `--cvars` feeds the second. A setting meant for the
classic picture (`allskins rust`, `enemycolor 4 12`) given in `--cvars` leaves `classic-N`
unchanged and looks like a bug in the feature. Pass `--before="r_look classic;<setting>"` and read
`before-N`. For a companion to look at, `--server="coop 1" --say="addbot;addbot"` seats two at the
coop spawns; on the pack's first map `--views="480,-60,110,270,10"` faces them.
