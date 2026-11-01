---
title: A plan that commits a bot past its other rules needs its own way out, measured in time spent
date: 2026-11-01
scope: games/shooters_edge/bots/water.cpp, games/shooters_edge/bots/mind.cpp, games/shooters_edge/labs/se_team_lab.cpp
concepts: [dive, commit, give-up, stall, air, swim-field, rng, prng, dive-wet-s, seeds, control, teleporter, stacked]
---

A dive was committed once in the water (no other goal, no air rule) and nothing ended one that
could not finish: on e1m2 the launcher's dive fails at its ledge, and bots bobbed there for up to
821 s of a ten-minute match, with no drowning to show for it. TIME spent in the committed state
(`dive s` in `se-team`'s legs table) found it; counts of dives begun did not. The way out is a
rule on the plan's own clock (`water.hpp`), then a cool-down on that item so the next think does
not pick the same dive again.

Measure a bot change over SEEDS against a CONTROL, never one match against the old tree: any
change sends a match down another path, and one seed's frags fell from 64 to 26 for a reason the
change did not cause. Build the control once with the fix switched off and the counters on
(`if (false && ...)`, the `labs` binary copied aside, the source restored), then run both
binaries in parallel with `--seed=N --tag=<v>-sN`. Ten seeds: 2990 s wet on dives against 96 s.

That seed's collapse was players jammed inside each other at a teleporter's exit, and the
controls had it too (2 matches in 10 each): the mod's teleporter puts a player on an occupied
exit. Look for several players ending at ONE point (`sort | uniq -c` over the lab's last notes)
before blaming a change.

Every draw comes from `core::Rng`, a bot's own included; no check catches a hand-written
generator in game code, and one spread to a second file within a day. `make precheck` and
`ge digest --affected` both passed while `se-team`'s smoke digest had moved: after touching a
lab's tables or the bots it runs, run `ge digest <lab>` by name before the push.
