---
name: replay
description: "Use for engine/replay in gameengine2 and anything that watches a run again: a game's Run::clone (the keyframe), the keeper and its thinning policy, the replay player's seek in a step budget, the transport (play, pause, speeds, skip, frame step); later the replay file and its marks, the instant replay, the director, spectating by inputs and the video export (docs/replays.md, rows M23a-M23h)."
---

# Replays

`engine/replay/` (`ge_replay`, namespace `ge::replay`) watches a run again from any moment. The
design is `docs/replays.md`; the rows are M23a–M23h in `docs/goals.md`. The one idea: a run is
its setup and its inputs (the input log, `engine/play/inputs.hpp`), so a replay is that log
stepped again through the game's own `Run`, never a recording of positions or pixels.

| File | What |
| --- | --- |
| `replay/keep.hpp/.cpp` | The keyframes: `Run::clone`s taken every `every_s`, thinned by age band on fixed multiples; the nearest copy at or before a step |
| `replay/player.hpp/.cpp` | The player: the log opened from its setup and stepped off its keyframes to any step, in a budget of steps a frame |
| `replay/transport.hpp/.cpp` | The transport: play, pause (the end pauses), four speeds, skip, frame step, seek; pure state over the step count |

## Rules

- **A seek lands on the straight run.** A run sought to step `s` has the digest and the state of
  the run played straight to `s`. Any change to the keeper, the player or a game's `clone` runs
  `make lab LAB=replay` before and after: `mismatches` must stay 0
  (`tests/replay_player_test.cpp` on a toy game, `tests/fall_line_play_test.cpp` on Fall Line).
- **A game's clone copies everything a step reads or writes**, its streams' positions, its
  pilot's hands and its digest included, and shares only what no step writes (the map, the runs'
  lines). A part the clone misses shows up as a mismatch at the first seek that starts from a
  keyframe. The clone's trace is off (`Run::trace(nullptr)`).
- **A keyframe is never stepped.** The player clones the keyframe it starts from, so the same
  keyframe serves the next seek.
- **The kept set is a rule, not a history.** `keeps(k, step, newest)` decides it from the policy
  and the newest step alone; spacings are whole multiples of the band before them
  (`policy_problem` says which is not), so a copy dropped is never wanted again.
- **The transport reads no clock.** The app hands `tick` the frame's seconds; a test hands it
  numbers. A seek's stepping is paid out with `Player::advance(budget)`, never in one frame.
- **What a keyframe costs is measured, not guessed**: the `replay` lab's `clone-bytes` (the
  counting allocator), `clone-ms` and `worst-seek-steps`. A game whose clone grows past a few
  hundred kilobytes shares more of what its steps never write.
