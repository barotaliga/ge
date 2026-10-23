---
title: The first trace of the bot showed its speed control dithering, which no metric had shown
date: 2026-10-22
scope: games/fall_line/sim/bot.cpp, games/fall_line/labs/why_lab.cpp
concepts: [dither, flips, bang-bang, brake, tuck, threshold, margin, humanness, rivals]
---

The `why` lab's first run (30 s of seed 38, a field of four) found the player's bot changing its
speed decision 56 times a minute and one rival 196, braking more than half its steps. The speed
rule is bang-bang on two thresholds round the plan's cap, so near the cap it toggles brake and
glide from one step to the next. The finish time, the gates and the sim's digests all looked
fine, so only the decision records showed it. Read `--flips` before tuning a bot by its results:
dithering is cheap to see there and invisible in a run report. Start M16b's humanness work here
(a hold on a choice, or hands that slew the brake).
