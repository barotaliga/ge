---
title: A bot that pursues the centreline misses every gate set off the line
date: 2026-10-21
scope: games/fall_line/sim/bot.cpp
concepts: [bot, slalom-gate, gate-offset, weave, plan-bot, missed-gate]
---

On a ski area's course (generator v8) the slalom gates stand up to a dozen metres off the line,
13–22 m wide. The bot pursued its lane about the centreline, so it missed nearly every gate,
asked for a reset each time and finished one seed of nine. `plan_bot` now lays a weave through
the gates' offsets on a smoothstep (passing each gate parallel to the piste) and plans its
speed on the weave's own bends; the lane is held about the weave inside the gate's panels. A
map whose gates all stand on the line leaves the weave empty, so the single piste's runs and
their golden digests do not move. Run `make sim ARGS="--version=8 --rivals=3"` after any change
to the gates or the bot.
