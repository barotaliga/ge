---
title: Every pilot drives through Driver::step, so an agent's run, a tape and the bot's agree to the bit
date: 2026-10-21
scope: games/fall_line/sim/simulate.cpp, games/fall_line/sim/play.cpp, games/fall_line/labs/play_lab.cpp
concepts: [driver, replay, play-protocol, rivals, digest, stdio]
---

The bot (`simulate`), a tape (`replay_tape`) and an agent (`PlaySession`) all step the run
through one `Driver`: snap the player's controls onto the tape's grid, record them, ask the bot
for each rival's, `step`, fold the state into the digest. Keeping that one path is what makes
`tests/fall_line_play_test.cpp`'s strongest check possible: an agent that sends the bot's own
moves as JSON text (numbers written with `num::to_string`, which round-trips) rides the bot's
run to the bit. A second loop that snapped or ordered anything differently would pass every
replay test of its own and still drift from the bot's. Two traps on the way: a replay must be
handed the same number of rivals (only the player is taped), and anything that prints to stdout
before the `hello` line (the build, the runner's report) breaks the protocol, which is why
`make play` sends the build to stderr and runs the lab with `--quiet`.
