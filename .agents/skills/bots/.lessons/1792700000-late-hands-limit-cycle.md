---
title: A 0.2 s reaction delay turned the skier bot's pursuit into a limit cycle until it looked further ahead
date: 2026-10-22
scope: games/fall_line/sim/bot.cpp, games/fall_line/sim/hands.cpp, engine/ai/hands.hpp
concepts: [hands, delay, limit-cycle, pursuit, look-ahead, lead, prediction, crawl, wander]
---

Putting human hands (0.2 s delay, a spring, a slew) under Fall Line's unchanged bot made its
steer swing from one end of the stick to the other: a pursuit tuned for instant hands is a
feedback loop with too much gain once the loop has a delay in it. Predicting the skier's heading
or position 0.2 s ahead made it worse (the prediction fed the same oscillation). What fixed it
was looking further down the line by the lead: `look += 6 * lead_s * max(v, 8 m/s)`, which
lowers the loop's gain. The speed floor matters: at a crawl on a climb the look shrank to
nothing and the bot stalled oscillating, as did a tuck wander that never let the trigger rest
(the wander is now faded within 0.1 of an end and of rest). When hands are slowed, lower the
bot's gain (a longer look), do not try to out-predict the delay.
