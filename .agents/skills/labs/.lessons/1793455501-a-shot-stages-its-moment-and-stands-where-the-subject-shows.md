---
title: A shot stages its moment in a scenario, stands where the subject shows, and a run's moments are scenes
date: 2026-10-22
scope: games/fall_line/labs
concepts: [labs, scenarios, staging, moments, run, bot, trails, track, view, follower]
---

- A lab about what one moment looks like stages it in a ride scenario
  (`plugins/skier/scenarios.hpp`: `hockey-stop`, `kicker`, `carve-hard`, `powder-turns`), which
  `create_frame` takes like a generated map; the bot on seed 38 never skids or flies 0.4 s. A lab
  about a whole run uses the bot.
- A run's moments (`world --moments=3,15,30`) step the game's own `Driver` once and shoot the chase
  camera at each second in order, so the cost stays one run and the scene list stays digestible.
- A track is behind the skier: a view of one stands past the place and looks back, picked a second or
  more before the run's end. A field keeps its lanes from the start, so to find a racer down
  another's line search every pair after the start with a reach a lane wide.
