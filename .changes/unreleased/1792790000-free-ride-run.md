---
type: Added
title: Fall Line's free ride
---

Fall Line's simulation gains the free ride (`games/fall_line/sim/free.hpp`). A run now has a mode (`Mode::race` or `Mode::free`). The free ride has no gates and no finish line. The ski area's runs are lines, each with its own nearest-point index. Each step it finds the run under the skier's skis (an `onto` event), notes every run skied and counts the metres skied on the snow. Reaching the foot of a run that flows into no other is an `arrived` event. Its reset stands him a little further down the run he was last on.

The bot plans along a line of its own (`BotPlan::track`): the course's piste in a race, or on the free ride the chain of runs from a run's head down to a foot (`run_chain`, `plan_free`, `simulate_free`). A race's digests read nothing of the free ride and do not move. The new `free` lab rides the bot down every piste of each seed.
