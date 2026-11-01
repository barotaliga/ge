---
title: A plan that commits a bot past its other rules needs its own way out, measured in time spent
date: 2026-11-01
scope: games/shooters_edge/bots/fighter.cpp, games/shooters_edge/bots/mind.cpp, games/shooters_edge/labs/se_team_lab.cpp
concepts: [dive, commit, give-up, stall, air, swim-field, rng, prng, dive-wet-s, seeds, control, teleporter, stacked]
---

The dive was committed once in the water (no other goal, no air rule) so a swim was not turned
round half-way, and nothing ended a dive that could not finish: on e1m2 the rocket launcher's
dive fails at its ledge, and with no drowning to show for it the bots simply bobbed there, up to
821 s of a ten-minute match. A metric of TIME spent in the committed state (`dive s` in
`se-team`'s legs table) found it; counts of dives begun did not. The way out is a rule on the
plan's own clock (in the water longer than the swim's planned time plus 3 s, or short of air with
the swim field's way back shorter than the dive's way on), then a cool-down on that item so the
next think does not pick the same dive again.

Measure a bot change over SEEDS against a CONTROL, never one match against the old tree: any
change sends a match down another path, and one seed showed frags falling from 64 to 26 for a
reason the change did not cause. Build the control once with the fix switched off and the
counters on (`if (false && ...)`, the `labs` binary copied aside, the source restored), then run
both binaries in parallel with `--seed=N --tag=<v>-sN`. Over ten seeds: 2990 s wet on dives
against 96 s, 789 frags against 910.

That same seed's collapse was players jammed inside each other at a teleporter's exit (the
lab's last notes list several players at ONE point: `sort | uniq -c` over them), and the controls
had it too (2 matches in 10 each): the mod's teleporter moves a player onto an occupied exit
without killing the one standing there. Look for identical end positions before blaming a change.

Every draw comes from `core::Rng` (`make_rng`, `next`, `pick_index`), a bot's own included; no
check catches a hand-written generator in game code, and one spread from one file to a second in
a day, so a review has to. `make precheck` and `ge digest --affected` both passed while
`se-team`'s smoke digest had moved: after touching a lab's tables, metrics or the bots it runs,
run `ge digest <lab>` by name (one or a few at a time) before the push.
