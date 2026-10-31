---
name: labs
description: "Use when writing, changing, running or reading a LAB in gameengine2: a measurement, a captioned screenshot, a staged scenario, a bot or agent drive, a cost bench. Owns the lab contract (engine/lab: LabSpec, GE_LAB, the Lab context, the runner, the raster, the PNG writer, the report and its contact sheet), where labs live (engine/lab/labs, plugins/<name>/labs, games/<name>/labs), captions, digests and the smoke baseline, the before-and-after a PR owes, and which labs each milestone must land. Load it BEFORE writing a one-off script or a printf to look at something: a lab is the same work, kept."
---

# Labs

A lab answers ONE question about its subject by running the real code, and writes back
numbers, tables and captioned pictures. `docs/labs.md` is the design: the contract, the
kinds, the files a run writes, the digests, the plans for GPU pictures, scenarios and
drives, and the table of labs by milestone. This skill is the procedure.

**Read this skill's lessons first**: `make lessons ARGS=labs`. Load `write-code` beside it,
and the subject's own skill (the lab's `skill` field names it).

## Where

Every lab runs on the GPU-less Linux container, which is the primary environment
(`docs/labs.md` § Where labs run). Pictures come from the CPU raster or, from M6, a
software GPU adapter read back to a `Picture` (`gpu-capture` is the pattern). A GPU frame
is a `Det::measured` shot, so its pixels stay out of the digest. Never write a lab that
needs a window or a person: `frame-pace` runs under `xvfb-run -a` and passes, noting it,
without a display. A measurement only the Mac can make keeps a Linux smoke mode and follows the
needs-the-Mac rule in `AGENTS.md`.

## Run one

```sh
make labs                                        # what exists: id, kind, owner, skill, question
make lab LAB=<id> ARGS=--help                    # its flags and their defaults
make lab LAB=<id> ARGS="--seeds=38 --tag=before" # BEFORE your change: kept in previews/<id>@before/
# ...change the code...
make lab LAB=<id> ARGS="--seeds=38 --compare=before"   # AFTER: every metric beside the kept run,
                                                       # every picture diffed (previews/<id>/diff/)
make lab LAB=<id> ARGS="--seeds=38 --shots=vista"   # WHILE ITERATING one look: only the named
                                                       # shots drawn, the rest left as they were
build/debug/ge lab <id> --before=origin/main -- --seeds=38   # OR BOTH AT ONCE, after the change:
                                       # the before built at the ref in build/before/<sha>/
build/debug/ge lab pair <id> --before="ce_relief 0"   # ONE SETTING ON AND OFF, no flag of the
                                       # lab's own: its shots side by side (the lab reads lab.cvars())
build/debug/ge lab publish <id> --before=before   # THE PICTURES FOR THE REPORT AND THE PR: one
                                                  # self-contained page, before beside after, ready
                                                  # to publish (previews/<id>/publish.html)
```

**A change to how something LOOKS** is proved by the picture diff, not by eye alone: the
`--compare` table gives each picture's PSNR, SSIM and changed share per distance band, each
band held to a floor (40 dB near down to 28 far), and `previews/<id>/diff/<shot>.png` shows
before, after, the change as a heatmap with the bands' contours, and the bands in metres
(`docs/labs.md` § The picture diff). A change meant only for far scenery should leave the near
bands at 100 dB. A GPU lab that guards a look hands its distance map with each shot
(`lab.shot(shot, picture, Det::measured, distance)`, the distance off
`render::read_distance`).

**Choosing a constant's value** is a sweep, not a string of edits and reruns: `make ge
ARGS="lab sweep <id> --set=<kName>=a,b,c -- <lab flags>"` rewrites the one-line `constexpr`, rebuilds
the labs and runs the lab at each value, prints every metric side by side and puts the source
back (`docs/labs.md` § The sweep); `--flag=<flag>=a,b,c` sweeps one of the lab's own flags
the same way with no rebuild (the hours of a day, a step rate). Put the value you pick in the code
and run the before-and-after as usual.

**Seeing a scenario step by step**: add `--trace` to a scenario lab (`ride`, `sled`, `heli`,
`para`). Each scenario writes `<id>.trace.tsv` and charts of it with its events marked, so a
snap, a chatter or an event at the wrong moment is a ring or a grey line you can point at, and
`ge graph` lays a before's and an after's file on one chart. A new scenario lab gets it by
giving its runner an optional per-step callback and drawing through
`engine/lab/series_charts.hpp` (`para_lab.cpp` is the pattern).

**Anything that moves can jump.** A lab that follows an actor hands its track to
`gl::find_teleports` (`engine/lab/teleport.hpp`) and reports it with `gl::report_teleports`
(fail on it when the subject must never jump): a position set without its velocity, at a handoff
or a respawn, is a row with its step and time instead of a hand-written metric.

Then read `previews/<id>/index.md` (the contact sheet) and open the pictures it lists. Each
picture's band says what it shows and what to look for, so check exactly that. Time a
`bench` lab on `PRESET=release`, because a debug build times the sanitizers.

**What a PR owes:** for a change in a lab's subject, the lab's output before and after (the
`--compare` table, the diff table and the sheets of the pictures that moved) in the PR body, plus
`make labs-smoke` green. If the change moves a lab's digest on purpose, run `make ge ARGS="digest <id> --write"`
(that lab's file only; `make labs-digests` rewrites every lab's), commit
`tests/digests/`, and say in the PR which lab moved and why. A digest that moved and you cannot
say why: `make ge ARGS="digest <id> --why"` names the metrics, rows (by column), shots and files
that differ from `origin/main`'s run (each run's `digest.tsv` is the list it reads).

## Write one

1. **Name the question.** One sentence that a run of this lab answers. If you need two
   sentences, you need two labs (or one lab with a `--scenario` table).
2. **Find its place.** `<module>/labs/<name>_lab.cpp`, beside the subject: `engine/lab/labs/`
   for an engine subsystem (named `<dir>_<name>_lab.cpp`), `plugins/<name>/labs/` for a
   plugin's, and `games/<name>/labs/` for a game's. A lab that needs the game (the bot, the
   modes) belongs to the game. CMake picks the file up; there is nothing to register.
3. **Write the spec and `run`.** Copy the nearest lab (`plugins/mountain/labs/level_lab.cpp` for a picture
   over seeds, `analyze_lab.cpp` beside it for a sweep with a table, `mapgen_cost_lab.cpp` for a bench,
   `loop_run_clock_lab.cpp` for an engine module), then:
   - flags with honest defaults, and a `smoke` that runs in seconds on a debug build;
   - **read the subject's own functions and restate nothing.** The gate is
     `level.checkpoints`, the digest is `level_digest`. A number typed into a lab is wrong
     the day the model moves;
   - metric keys are stable kebab names (`seed-38.drop`) because `--compare` matches on
     them. Give each its unit. Mark wall-clock and anything else that varies run to run
     `Det::measured`;
   - put the inputs in force on every shot (`inputs`) and the question in the spec. A
     before-and-after pair can only be checked if it shows it is the same measurement;
   - `lab.fail(what, next action)` when the lab sees something wrong. Write it in the
     diagnostics shape: what failed, then the test to run or the skill to load.
4. **Caption every picture** (below).
5. **Run it**, look at its contact sheet, run `make labs-smoke`, then `make ge ARGS="digest <id> --write"`
   for the new row, and `make index` (a lab file carries a `ge:` block like any source).
6. Add it to its milestone's `Labs:` line in `docs/goals/<row>.md` if it is one a milestone owes.
   There is no table of labs to add it to: the `LabSpec` is the list, and `make index` writes
   `docs/lab-list.md` from every one.

## Captions: a picture's acceptance test, written down

| Field | Write |
| --- | --- |
| `title` | A short upper-case line naming the subject and the view: `SEED 38 FROM ABOVE`. |
| `caption` | What is in the frame and how to read it: which way is up, what each colour is, the numbers that matter ("3092 m of piste, 936 m of drop through 27 gates"). |
| `look_for` | What a correct picture shows, as checks a reviewer can tick: "gates in order down the fall line; no trees on the piste". Write it before you look. |
| `inputs` | seed, version, region, every flag that changes the picture. |

The font is upper case, 5x7 and ASCII only. A character it lacks draws blank, so write
"DEG" and "x", not a degree sign or a multiplication sign.

## Digests and determinism

Every lab's digest hashes its deterministic metrics, its table cells and its pictures'
pixels. `labs_smoke` (CTest, every CI row) runs each lab twice in one process and checks the
result against `tests/digests/`:

- **Different digests between the two runs**: the lab or the code under it keeps state
  between runs or draws from an unseeded source. Load `determinism`.
- **Same twice but not the baseline, on every row**: the output moved. If you meant it,
  run `make ge ARGS="digest <id> --write"`. If you did not, run `--tag` on main and `--compare` on your
  branch to see which metric moved.
- **Not the baseline on ONE row**: a compiler or platform difference reached the output.
  Treat it as a cross-platform determinism failure on that row (`determinism`).

So lab code obeys the simulation's rules: no `<cmath>` transcendental (`core/libm`;
`make libm-check` scans `engine/lab`), pixels rounded with `num::round`, and no randomness
outside `core::Rng`.

**What is measured, not hashed.** Pass `Det::measured` for anything that can differ between two
honest rows: a time (a table of times too: `Lab::table` hashes every cell by default), anything that
depends on a GPU-culled draw's order (overdraw without the prepass; only order-free results such as
a converged still or the depth pyramid are exact), and any number that passes through a named view,
a camera or the clouds (presentation code calls the platform's `std::atan2`, and MSVC rounds it its
own way, so only the Windows row moves). Hash what the simulation alone decides.

**A GPU lab's no-adapter branch owes the same digest.** The digest is one running hash over every
deterministic metric and every shot's ID IN ORDER, and `Lab::shot` hashes the id even when the pixels
are measured. Windows CI has no adapter, so its `if (!device)` branch must take a placeholder shot
under every id the device branch takes (`-before` and `-diff` shots included) and give the same
deterministic metrics in the same order; give `gpu-errors` and `pipelines-after-load` last on both
paths (`water_scene::placeholders`, gpu-capture's placeholder). A branch that only notes and returns
digests to the empty FNV value `cbf29ce484222325`. Hiding Vulkan locally does not test it (Dawn falls
back to Null): force the branch (`gpu.device.reset()` after `create_device`, or `if (true)`), compare
with `--check-digests=tests/labs_digests.txt`, and restore.

**Names the tools take.** `tools/check-libm.sh` matches an unqualified `log(`, `exp(`, `pow(`,
`sin(`... in code and in strings, so no function of ours is named after a transcendental (the log
channel's writer is `output::log_on`) and a chart label spells "log scale" in words. The runner owns
`--out`, `--tag`, `--compare`, `--diff*`, `--repeat`, `--smoke`, `--all`, `--shard`,
`--check-digests`, `--write-digests`, `--quiet` and `--debug` (`labs --help`); a lab's own flag of
one of those names is shadowed, so name it something else (`--timings`).

**Registration lives in the executable.** `GE_LAB(spec)` registers from a static constructor, and the
linker drops an unreferenced object from a static library, so a lab in a library vanishes with no
error. Lab files and the helpers they share live in `engine/lab/labs/`, `plugins/*/labs/` or
`games/*/labs/`, globbed into `labs`; re-run `cmake --preset` after adding one.

**Reading a run back** takes its shots from that run's `report.json`, never the directory listing:
`previews/<id>/` keeps an older run's files.

## Kinds still to build

`scenario` and `drive` (the bot, rivals, an agent over `labs play --stdio`) land with M5,
and GPU captures with M6–M7. `docs/labs.md` holds their designs. Build them to that design,
and change the doc in the same PR if the design has to move.
