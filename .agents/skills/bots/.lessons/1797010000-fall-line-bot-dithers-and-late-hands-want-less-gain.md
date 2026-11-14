---
title: A skier bot's speed rule dithers where no result shows it, and late hands want a longer look, not a prediction
date: 2026-10-22
scope: games/fall_line/sim/bot.cpp, games/fall_line/sim/hands.cpp, plugins/skier/pilot.cpp, engine/ai/hands.hpp, engine/ai/human.cpp, engine/play/human.cpp
concepts: [dither, flips, bang-bang, hands, delay, limit-cycle, look-ahead, humanness, hold, lag, bands, pilot, refactor, digest]
---

- The `why` lab's first run (seed 38, a field of four) found the bot changing its speed decision 56
  times a minute and a rival 196: a bang-bang rule on two thresholds round the cap toggles brake and
  glide every step. Times, gates and digests looked fine; only `--flips` showed it.
- Human hands (0.2 s delay, a spring, a slew) under the unchanged pursuit made a limit cycle from
  one end of the stick to the other. Predicting the skier 0.2 s ahead fed the oscillation; a look
  further down the line lowered the loop's gain and fixed it (`look += 6 * lead_s * max(v, 8 m/s)`).
  Keep the speed floor: at a crawl the look shrank to nothing and the bot oscillated.
- Humanness traps (`ai/human.hpp`): a plain median of holds is all one-step nudges, so the hold is a
  time-weighted median of part-way holds; a trigger part-way is passing through, so the hold and
  saturation bands are the stick's alone; an axis whose wanted never moves has no lag (NaN, not 0).
  Run a new metric on hands that should pass and on a constant input before writing its band.
- Moving the bot into `plugins/skier/pilot.hpp` kept every digest: the game's API stayed a thin
  layer over it (`BotPlan` derives from `PilotPlan`), float operations kept their order, the
  game-only parts hand the pilot plain data. A moved bot owes an empty digest diff.
