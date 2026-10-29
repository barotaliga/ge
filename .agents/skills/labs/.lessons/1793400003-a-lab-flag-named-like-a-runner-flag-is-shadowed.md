---
title: A lab's own flag named like a runner flag (`repeat`) is taken by the runner
date: 2026-10-29
scope: plugins/progs/labs/qc_lab.cpp, engine/lab/
concepts: [lab-flags, runner, repeat, smoke, digest]
---

The `qc` lab first had a `--repeat` flag for its timing rounds. `labs qc --smoke --repeat=2` gave
the runner's `--repeat=2` to the lab as well. The heading said "15 times" while the smoke flags
asked for 3. The runner owns `--out`, `--tag`, `--compare`, `--diff*`, `--repeat`, `--smoke`,
`--all`, `--shard`, `--check-digests`, `--write-digests`, `--quiet` and `--debug` (`labs --help`).
Name a lab's own flags something else, as `--timings` is here.
