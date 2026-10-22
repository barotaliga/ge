---
type: Added
title: ge shot, the views on the play contract, ge new --game=racer and ge upgrade
---

A game lists the views `ge shot` can draw (`GameSpec::views`, `Run::shoot`): `ge shot <log> --step=N --view=V` replays a run to a step and writes the view, drawn on the CPU and framed with its caption, the seed, the options, the step and the digest, as a PNG; without a log it shoots the pilot's run of `--seed`. `ge::play` now links `ge::lab` for the raster, and `lab::framed` is public. `ge new <dir> --game=racer` starts a game repo as a racer: a car round a loop of gates the seed lays out, two laps, an autopilot, and a track view. `ge upgrade`, run with a newer release's ge, moves a game repo's pin onto that release, syncs its skills and runs `ge doctor`; it never moves a repo back.
