---
title: Stage an effect in a ride scenario, not where a bot happens to do it
date: 2026-10-22
scope: games/fall_line/labs/gpu_spray_lab.cpp
concepts: [labs, scenarios, staging, spray, bot, hockey-stop, landing]
---

The spray lab first looked for its moments in the bot's run on seed 38: the bot never skids
(its widest slide was under 1 m/s) and never flew 0.4 s, so the skid and the landing had
nothing to show. The ride scenarios (`plugins/skier/scenarios.hpp`: `hockey-stop`, `kicker`,
`carve-hard`, `powder-turns`) stage each one exactly, on a map `create_frame` takes as it takes
a generated one. A lab about what one moment looks like stages it; a lab about a whole run
uses the bot.
