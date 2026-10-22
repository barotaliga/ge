---
name: labs
description: "Use to write, run or read a lab in a game repo: a measurement, a captioned picture, a headless sim over seeds, a scenario, a bot or agent drive, a bench; the GE_LAB contract, the runner's flags, digests, and the before-and-after a PR owes."
---

# Labs

A lab answers ONE question about the game by running the real code and writing what came back
as numbers, tables and captioned pictures, so an agent measures and LOOKS instead of guessing.
The contract and the runner are the engine's (`lab/lab.hpp`, `lab/runner.hpp`, target
`ge::lab`); every `labs/*.cpp` here is compiled into one `labs` executable.

## Running

```sh
ge lab                                 # the list (--json: id, kind, owner, skill, question, flags)
ge lab run                             # one lab, its default flags
ge lab run -- --help                   # its own flags; they go after --
ge lab run -- --seeds=7                # one lab with its own flags
ge lab run --tag=before                # write to previews/run@before/
ge lab run --compare=before            # every metric beside the tagged run's, every picture diffed
ge lab diff run                        # the last run's pictures against previews/run@before/:
                                       # PSNR and SSIM per distance band, each under its floor a finding
ge lab diff run --run -- --seeds=7     # run it first, then diff
ge lab run --json                      # the same, its report.json inside the answer
```

A lab writes `previews/<id>/`: `report.json` and `metrics.tsv` for an agent, `index.md` (the
contact sheet), and one PNG per picture with its caption burned in. Read `index.md` first, then
open the pictures it names. `ge test` runs every lab at its smoke flags twice in one process:
a digest that differs between the two runs is a determinism break.

## Writing one

`ge add lab <name> --kind=sim|scenario|picture|bench` writes a working one of that kind to
start from; make it answer your question. By hand, it is one file, `labs/<id>_lab.cpp`, registering a `LabSpec` with `GE_LAB(spec)`:

- `id` (the command-line name), `kind` (`sim`, `scenario`, `picture`, `drive`, `bench`),
  `owner` (`game:<name>`), `subject` (the files it measures), `skill` (how to read it),
  `question` (one sentence).
- `flags`: each with its default as typed and a help line. An unknown flag is an error.
- `smoke`: the flags CI runs it with, seconds not minutes.
- `run(Lab&)`: `lab.metric(key, value, unit)`, `lab.table(...)`, `lab.note(...)`,
  `lab.shot(Shot, Picture)` and `lab.fail(what, next_action)`.

**Captions.** Every picture carries a `Shot`: an id, a title, a caption saying what it shows,
`look_for` saying what a reviewer checks, and the inputs in force. Write them for a reader who
sees only that picture.

**Digests.** Metrics, tables and pictures feed the lab's digest unless marked
`Det::measured` (wall-clock times, anything that differs run to run). The digest is what makes a
lab a determinism test for free.

**Failures** are prompts: `lab.fail("seed 7 finished in 0 steps", "run tests/sim_game_test.cpp;
skill: sim-system")`. The run carries on so every failure is named, and the runner exits 1.

The engine's own headers say the rest: `lab/lab.hpp` (the contract), `lab/raster.hpp` (drawing:
lines, discs, text), `lab/runner.hpp` (every flag).

## Before and after

A change in a lab's subject runs the lab before (`--tag=before`, on the old code) and after
(`--compare=before`), and the PR carries the comparison and the pictures that moved. A digest
that moved on purpose says why. A change to how something looks is proved with `ge lab diff
<id>`: a picture handed with a distance map (`lab.shot(shot, picture, Det::measured,
distance)`) is judged band by band, so a change far off may be coarser than one near the
player (`lab/diff.hpp` says how).
