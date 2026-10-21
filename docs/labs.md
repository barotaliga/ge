# Labs

A lab runs the real code on one question and writes back what it found: numbers, tables
and captioned pictures. Labs are how this engine is tuned and checked. An agent measures
and LOOKS instead of guessing, and a PR shows its evidence as a lab's output before and
after the change. Labs are built once, into the engine, so that every engine subsystem,
every plugin and every game plugs into one contract.

Two rules follow from it:

1. **Every subsystem owes labs.** A module is not done when its tests pass. It is
   done when an agent can run a lab that shows what it does. Each milestone row in
   [`goals.md`](goals.md) names the labs its exit needs (the **Labs** column).
2. **A change in a lab's subject runs that lab before and after.** Keep both outputs
   (`--tag=before`, then `--compare=before`) and put them in the PR.

The procedures (how to write a lab, how to read one, how to caption a picture) are the
`labs` skill's. This page is the design.

## The contract

A lab is a `LabSpec` and a `run` function (`engine/lab/lab.hpp`):

| Field | What |
| --- | --- |
| `id` | The name on the command line (`make lab LAB=level`): the subject's noun, kebab-case. |
| `kind` | `sim`, `scenario`, `picture`, `drive` or `bench` (below). |
| `owner` | `engine`, `plugin:<name>` or `game:<name>`. Whose subject it is. |
| `subject` | The files or the system it measures. |
| `skill` | The skill that says how to read its output. |
| `question` | The one question it answers, as a sentence. It heads every report. |
| `flags` | Its own flags with defaults and help. Anything else typed is an error. |
| `smoke` | The flags CI runs it with. Seconds, not minutes. |

`run(Lab&)` reads its flags (`arg`, `arg_num`, `arg_nums`, `arg_bool`) and reports through
five sinks: `metric` (a keyed number with a unit), `table`, `note`, `shot` (a picture and
its words) and `fail` (what failed and the next action, in the
[diagnostics](architecture.md#diagnostics-agent-actionable-output) shape).

### The kinds

| Kind | What it does | Today |
| --- | --- | --- |
| `sim` | Headless runs of the real code over seeds or a sweep. Numbers and tables. | `analyze`, `run-clock` |
| `scenario` | One staged situation, such as a skier placed at a moment on a synthetic slope with a scripted input, measured exactly. | `ride` |
| `picture` | A captioned picture of a subject. It is drawn on the CPU raster now, and from M7 it is a GPU frame read back. | `level`, `noise` |
| `drive` | A pilot plays the game, either the bot or an agent over stdio. The run is the report, and the control tape is the replay. | `sim`, `play` |
| `bench` | What something costs. Its wall-clock metrics are `measured`, which means they are printed and compared but never put in the digest. | `mapgen-cost` |

### Where labs live, and how they register

A lab is a file in a `labs/` directory beside its subject:

```
engine/lab/            the framework: contract, runner, report, raster, PNG
engine/lab/labs/       the engine's own labs (<dir>_<name>_lab.cpp: core_noise_lab.cpp)
plugins/<name>/labs/   a plugin's labs, owner "plugin:<name>" (plugins/mountain/labs/)
games/<name>/labs/     a game's labs, owner "game:<name>"
```

Each lab file ends with `GE_LAB(kSpec);`, which registers the lab from a static
initializer. CMake globs every `.cpp` in those `labs/` directories into ONE executable,
`labs`. Nothing is registered by hand, and a plugin or a game adds a lab by adding a file.
Labs are compiled into the executable and never into a static library, because the linker
drops unreferenced objects from a library and the registration with them. A helper that
several of one module's labs share sits in the same directory (`plugins/mountain/labs/level_map.hpp`).

A lab may include anything (it is tooling), and nothing includes a lab.

## Where labs run

**The primary development environment is a Linux container with no GPU**, and every lab
must run and be verifiable there: the CPU raster today, and from M6 GPU pictures through a
software adapter (a CPU Vulkan driver under Dawn), so a screenshot lab works on
CI and in an agent's container alike. A lab that renders reads its frame back to a
`Picture`, and the agent opens the PNG and the contact sheet. No lab may need a window, a
display server or a person: the one lab that measures a window (`frame-pace`) runs under a
virtual display (`xvfb-run -a`) in the container, and without a display it notes that and
passes, with the same digest.

Some things can only be measured on the secondary machine, an Apple-silicon Mac: real GPU
timings, Metal-specific behaviour and the feel of the frame pacing on a real display. A lab
that needs it says so in its `question`, keeps a smoke mode that still runs on Linux (at the
least its CPU half), and its hardware runs follow `AGENTS.md`'s rule for work that needs the
Mac. That rule applies only when Linux cannot answer the question, never because the Mac
would answer it faster.

## Running labs

```sh
make labs                                     # the list: id, kind, owner, skill, question
make lab LAB=level ARGS="--seeds=38,7"        # one lab; ARGS are its flags and the runner's
make lab LAB=level ARGS=--help                # its flags, with defaults
make lab LAB=level ARGS=--tag=before          # keep this run as previews/level@before/
make lab LAB=level ARGS=--compare=before      # every metric beside the kept run's
make labs-smoke                               # every lab at its smoke flags, twice, digests checked
make labs-digests                             # rewrite tests/labs_digests.txt (a reviewed change)
```

The binary is `build/<preset>/labs` (`PRESET=release` for anything timed). An unknown flag
exits 2, a failed lab exits 1, and a clean run exits 0, because a measurement that ignores
a mistyped flag reports a confident wrong number.

## What a lab writes

The terminal gets the report: the question, the inputs in force, the metrics, the tables, the
notes, where the pictures went, any failures and the digest. Under `previews/<id>/` (or
`previews/<id>@<tag>/`, gitignored) the next agent and the PR get files:

| File | For |
| --- | --- |
| `report.json` | Everything above, machine-readable. |
| `metrics.tsv` | `key value unit det`, one metric a line. `--compare` reads it back. |
| `index.md` | **The contact sheet**: every picture with its title, caption, what to look for and its inputs. An agent reads this first, then opens the pictures it needs. |
| `<shot>.png` | The picture with its **caption band** burned in under it. |
| `<shot>.json` | The shot's words beside the picture. |
| any attached file | What `lab.attach(name, bytes)` wrote, listed at the foot of the contact sheet: a rendered sound's `<shot>.wav`, for a reader with speakers to hear exactly what was metered. Its bytes are in the digest unless it is `measured`. |

### Sounds

A lab shows a rendered sound with `ge::lab::sound_shot(lab, shot, stereo)` (`engine/lab/sound.hpp`):
it meters the buffer (`engine/audio/meter.hpp`), writes `<id>.loudest-db`, `.peak-db`,
`.centroid-hz`, `.onset-s` and `.length-s`, draws one picture (the waveform zoomed to its peak,
the 10 ms level envelope from -80 to 0 dBFS, the ten octave bands) and attaches the WAV. A session
that cannot listen reads the sound by those numbers and that picture. The synth renders the same
bytes on every platform, so a sound's metrics and its WAV are in the digest like a raster's pixels.

### Captions

A picture without words is a picture nobody can review. So every `shot` carries a `Shot`:

- `title`: one line ("SEED 38 FROM ABOVE");
- `caption`: what the picture shows, including the legend (which colour is what, which way
  is up);
- `look_for`: what a reviewer checks it for ("gates alternate red and blue down the fall
  line"). This is the picture's acceptance test, written down;
- `inputs`: the seed, the version and the flags it was made under.

The words are burned into a band under the image, so a picture pasted alone into a PR still
says what it is. They are also in the sidecar and the contact sheet. The words are written
for a reader who sees only that picture.

## Digests: every lab is a determinism test

A lab's **digest** is an FNV-1a 64 over what it wrote, in order: each deterministic metric's
key and bits, every table cell, and every picture's id and pixels (the caption band
included). `measured` metrics are left out, and so are the pixels of a `measured` picture
(`lab.shot(shot, picture, Det::measured)`): a GPU frame is rasterised by each adapter in
its own way, so its id is hashed and its pixels are not, and its sidecar and contact sheet
say so. A lab that holds a GPU frame to an image does it itself, on the pinned adapter, and
fails when it is off. Three checks use it:

- **`--repeat=N`** runs the lab N times in one process and fails when the digests differ.
  That catches state kept between runs and unseeded draws.
- **`--check-digests=tests/labs_digests.txt`** compares each lab's smoke-run digest with
  the committed baseline. CTest's `labs_smoke` runs it on every CI row, so a lab whose
  output differs between compilers fails on the row that differs.
- A lab that has to move (its drawing changed, or the code it runs changed on purpose)
  rewrites the baseline with `make labs-digests`, and the PR says why.

That is why lab code follows the simulation's rules even though it is tooling: no `<cmath>`
transcendental (`make libm-check` scans `engine/lab`; use `core/libm`), integer pixels from
`num::round`, and nothing random but `core::Rng`.

## Pictures before and after the renderer

**Today** a picture is drawn on the CPU raster (`engine/lab/raster.hpp`: boxes, lines,
discs and a 5x7 font) and written by the PNG
writer (`engine/lab/png.hpp`). That is enough for maps from above, plots, profiles and
contact sheets, and it runs anywhere, CI included.

**From M6/M7** the renderer adds a capture path, and the API above stays the same:

```cpp
// render (M7): draw a named view of a GameState offscreen and read it back.
ge::lab::Picture capture(const FrameView& view, const CaptureSpec& spec);   // planned
lab.shot({"chase-12", "CHASE AT 12 S", "...", "...", inputs}, capture(view, spec));
```

- `capture` renders into an offscreen texture on a headless device (the GPU smoke test M6
  sets up), reads it back to RGBA8 and returns a `Picture`. It gets the same caption band,
  sidecar and contact sheet.
- **Named views** (`spawn`, `powder`, `lookback`, `track`, `jump`, `landing`, `vista`,
  `chase-<s>`, `orbit`) are a table in the game's labs. A view
  is a camera rig and a moment, so the same view is the same picture on every run.
- **Side by side with a reference.** A picture lab can put its frame beside a reference
  picture of the same view (`--reference=DIR`, built with M7), with both captions. The pair
  is M7's and M8's exit evidence.
- GPU pictures are not bit-identical across drivers, so a GPU shot is hashed only when
  the lab says it is stable (a software adapter on CI). Otherwise its pixels are left out
  of the digest, while its metrics still count.

## Scenario labs

A scenario stages ONE exact situation instead of skiing into it: a hand-built map (a
synthetic slope with a kicker, rollers and a lone tree, or a flat drag strip of packed snow
or powder: `plugins/skier/synthetic.hpp`), a moment to stand the skier at (`stand_skier`:
position, heading, speed, height over the snow), a scripted input as a function of run time and
state, a length in seconds, and the numbers that say how it went (`lab.metric`).

A scenario lab is a table of these (one `LabSpec`, with `--scenario=<id>` choosing a row), so
`make lab LAB=ride ARGS=--scenario=kicker` stages the kicker taken at a known speed. The same
rows (`plugins/skier/scenarios.cpp`) back the physics tests
(`tests/skier_scenarios_test.cpp`), so a scenario is written once and is both a test and a lab.

## Drive labs: the bot, the rivals and agents at the controls (M5)

A **pilot** is anything that turns what a skier can see into a `SkierInput`, one step at a
time. The engine has one loop for every pilot:

```
state = create_game(opts)
for each step:  input = pilot(state)  →  tape.record(input)  →  step(state, input)
report: the RunReport (finish, time, gates, crashes, digest) + the control tape
```

- **The bot** (`games/fall_line/sim/bot.hpp`) is a pilot. The `sim` drive lab (`make sim`)
  rides it over seeds and prints the run report table (finish, time, gates, crashes, digest)
  with its line drawn on each map; `simulate.hpp` is the one loop every pilot drives, and
  `tests/fall_line_sim_test.cpp` holds its digests golden.
- **Rivals** are bots with profiles (`Rider`: a pace off a stream of the map's seed, a lane off
  the grid slot), on the same grid, through the same `step`; `make sim ARGS=--rivals=3` prints
  the field table (`games/fall_line/sim/field.hpp`).
- **Every drive is recorded on the control tape** (`plugins/racing/tape.hpp`, the format a
  ghost and a replay use). A drive's seed, options and tape reproduce it exactly, so any
  run (a bot's, an agent's, a player's) becomes a repro or a scenario.
- **An agent at the controls**: `labs play --quiet --stdio` runs a game under a line protocol,
  with one JSON object a line in each direction, so an agent can playtest without a screen
  (`--seed`, `--rivals`; the runner's `--quiet` keeps stdout to the protocol). The session
  opens with a `hello` line (the seed, the field, the gates, the piste's length, the step rate,
  the ops). The protocol is stated once in `games/fall_line/sim/play.hpp`:

  | Agent sends | Engine answers |
  | --- | --- |
  | `{"op":"observe"}` | the clock and step; where he is, his heading, speed and edge, on the snow or in the air, down or not; the gate owed, its bearing off his heading (+ right) and its distance; where he stands on the piste (`piste_lateral`, `piste_width`, `piste_heading`, `piste_bend` over the next 30 m); finished and the finish time; his place in the field; his events since the last answer |
  | `{"op":"act","input":{"steer":s,"tuck":t,"brake":b,"jump":j,"reset":r},"steps":n}` | the observation after n steps (1 to 1200) with that input held; every input key is optional, and `reset` is taken on the first step |
  | `{"op":"shot","view":"chase"}` (M7) | the path of a captioned picture of that view now |
  | `{"op":"tape"}` | the tape so far, one base64 stream per axis: the run as a replay |
  | `{"op":"end"}` | the drive's report and digest; the session ends |

  A line the session cannot use answers `{"op":"error","error":...,"next":...}`: what was wrong
  and what to send instead. Numbers are fixed-point text to three decimals. The session drives
  the same step as the bot (`Driver`), so an agent's tape played back with the same rivals is
  its run again, and an agent that sends the bot's moves rides the bot's run to the bit
  (`tests/fall_line_play_test.cpp`). Without `--stdio`, the `play` lab has the bot play through
  the protocol's text and checks exactly that; `--script=<file>` plays a file's lines.
  Two agents (or an agent and the bot) racing one map is two pilots on one grid. The
  protocol carries only what a player could know, so an agent's playtest is a player's
  run.

## The labs

| Lab | Kind | Owner | Subject | Lands |
| --- | --- | --- | --- | --- |
| `noise` | picture | engine | `engine/core/noise.hpp` | M2 (now) |
| `run-clock` | sim | engine | `engine/loop/run_clock.hpp` | M2 (now) |
| `level` | picture | plugin:mountain | `plugins/mountain/mapgen` (`--version=1,8`: the single piste, a ski area's race map) | M4a, M4b4a (now) |
| `analyze` | sim | plugin:mountain | `plugins/mountain/analysis` (a race map's course and area at `--version=8`) | M4a, M4b4a (now) |
| `mapgen-cost` | bench | plugin:mountain | `plugins/mountain/mapgen` | M4a (now) |
| `slalom` | picture | plugin:mountain | `plugins/mountain/mapgen/slalom.hpp` (a slalom set over a map, both runs, straightened and from above) | M4b4b (now) |
| `speed-events` | picture | plugin:mountain | `plugins/mountain/mapgen/speed_events.hpp` (the downhill, super-G and giant slalom set over a map, straightened and from above) | M4b4c (now) |
| `speed-ski` | picture | plugin:mountain | `plugins/mountain/mapgen/speed_ski.hpp` (a speed-skiing track cut down a map's face, its profile from the side and the map from above) | M4b4d (now) |
| `ski-cross` | picture | plugin:mountain | `plugins/mountain/mapgen/ski_cross.hpp` (a ski-cross course built over a map: close up, its profile with the mean fall taken out, the map from above) | M4b4e (now) |
| `resort` | picture | plugin:mountain | `plugins/mountain/mapgen/resort_build.hpp` (the ski area, its courses) | M4b3d (now) |
| `junctions` | picture | plugin:mountain | `plugins/mountain/mapgen/network_stamp.cpp` (where runs meet) | M4b3d (now) |
| `routes`, `skijump-hills` | picture | plugin:mountain | the routes, the jump hills | M4c |
| `real-faces` | picture | plugin:mountain | the real faces | M4d |
| `sim` | drive | game:fall_line | `games/fall_line/sim` (the step, the course, the bot, the field) | M5b–M5c (now) |
| `ride` | scenario | plugin:skier | `plugins/skier` (`step_skier`, the snow, the limits) | M5a (now) |
| `play` | drive | game:fall_line | `games/fall_line/sim/play` (an agent over stdio) | M5c (now) |
| `gpu-capture` | picture | engine | `engine/gpu` (the headless capture) | M6 (now) |
| `frame-pace` | bench | engine | the platform's frame loop | M6 (now) |
| `world` | picture | game:fall_line | Fall Line's frame from named views of a seed (`games/fall_line/labs/gpu_world_lab.cpp`) | M7a (now) |
| `shadows` | picture | game:fall_line | the horizon maps over a day from above, and the bake counter of a sun sweep (`games/fall_line/labs/gpu_shadows_lab.cpp`) | M7b (now) |
| `profile` | bench | game:fall_line | a frame's CPU and GPU cost | M7d |
| `forest`, `snow`, `trails`, `spray`, `skis`, `skier` | picture | game:fall_line | the world drawn | M8 |
| `synth` | sim | engine | `engine/audio` (every voice the synth can play, metered, pictured and written as WAV; the render's cost) | M9a (now) |
| `audition` | sim | game:fall_line | Fall Line's bank, the skier's beds at seven moments and the bot's whole run, metered, pictured and written as WAV | M9b (now) |
| `screenshots` | picture | game:fall_line | the HUD and the cards at 1280x720, 390x844, 844x390 | M10 |
