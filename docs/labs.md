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
| `bench` | What something costs. Its wall-clock metrics are `measured`, which means they are printed and compared but never put in the digest. A game's step needs no lab of its own: `ge bench`, `ge profile` and `ge mem` time the pilot's run through the play contract, zone by zone (`engine/play/bench.hpp`). | `mapgen-cost` |

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
make lab LAB=level ARGS=--compare=before      # every metric beside the kept run's, every picture diffed
make lab LAB=world ARGS=--diff=before         # run nothing: the last run's pictures against the kept run's
make labs-smoke                               # every lab at its smoke flags, twice, digests checked
make labs-digests                             # rewrite tests/digests/ (a reviewed change)
make ge ARGS="digest level --write"           # rewrite one lab's row only; no --write: just check it
build/debug/labs --json                       # the list as one JSON array (what `ge lab` reads)
make lab LAB=trace ARGS="--log=course=debug"  # any lab: the log spec's lines in previews/<id>/log.txt
```

`--log=<spec>` switches on log channels for the run (`docs/debugging.md`): every line kept goes to
`previews/<id>/log.txt` as `[step] level channel: message`, the debug and trace lines only
there, and a note in the report counts them by channel and level. The log is `measured`: it
never moves a digest.

In a game repo, `ge lab` is the way in: `ge lab` lists, `ge lab <id> --tag=before` and
`--compare=before` run one, and with `--json` the lab's whole `report.json` is the answer's
`result`. `ge lab diff <id>` diffs the pictures (below), every band under its floor a finding.
`ge lab publish <id>... [--before=<tag>]` puts the last run (of one lab, or of several one after
another) on one self-contained page for a report or a pull request (`engine/cli/publish.hpp`). Each picture loses its caption band, which the page
sets as text, and is shrunk to `--width` (720 px) and inlined as a 256-colour GIF
(`engine/lab/gif.hpp`), with the metrics and tables after it. A kept run stands beside it, picture
for picture and metric for metric. The labs' PNGs are stored uncompressed, so a page of a dozen of
them inline would pass a published page's 16 MB, but as GIFs they come to tens of kilobytes each.

### The sweep: one constant or one flag at several values

`ge lab sweep <id> --set=<name>=<a>,<b>,... [--in=<file>] [--metrics=<word,...>] -- <lab flags>`
runs a lab with one constant of the code at each value, every metric side by side
(`engine/cli/sweep.hpp`). The constant is a `constexpr` defined on one line (a spec's or a
tuning table's, `inline constexpr double kFlight = 0.6;`), named bare (`kFlight`) or qualified
(`launch::kFlight`, which keeps only files that open those namespaces); `--in` names its file
when the name is in several. For each value the initializer is rewritten in place, only the
`labs` target is rebuilt (the files that include it recompile) and the lab runs with
`--tag=sweep-<name>-<value>`, so every run's pictures stay for `ge lab diff`. The file is put
back after the last run and the labs rebuilt on what it holds; a sweep killed half way leaves
its text under `build/<preset>/ge-sweep/backup.txt`, and the next sweep puts it back first.
Three values of a paramotor constant over one scenario take about 20 s in debug.

```sh
ge lab sweep para --set=launch::kFlight=0.4,0.6,0.9 --metrics=launch -- --scenario=launch
```

`--flag=<flag>=<a>,<b>,...` sweeps one of the lab's own flags instead of a constant: the labs are
built once and the lab runs with `--<flag>=<value>` at each value, tagged
`sweep-<flag>-<value>`, the same table side by side. Nothing is rewritten, so it is the way to
take a picture lab over the hours of a day or a sim over its step rate. `--set` and `--flag`
together are refused: one thing at a time.

```sh
ge lab sweep para --flag=hz=60,120 --metrics=launch -- --scenario=launch
```

### The picture diff: did the look move, and how near the player?

`--compare=<tag>` diffs every picture of the run against the same picture of the kept run, and
`--diff=<tag>` does it for the last run without running anything (`engine/lab/diff.hpp`). Each
shot is cropped out of its caption band and measured over the whole picture and, when the lab
handed a **distance map** with the shot (`Lab::shot`'s last argument; a GPU lab reads it off the
frame's depth with `render::read_distance`), over every **distance band**: 0-25 m, 25-100 m,
100-400 m, 400 m+ and the sky by default (`--diff-bands=25,100,400`). For each it gives the
PSNR, the SSIM of the luma, the mean and largest channel change and the share of pixels changed
by more than 8 levels, and over the whole picture a near-weighted mean (each pixel weighted by
1 / (1 + distance / 25 m)).

Each band is held to a **floor**, its PSNR in dB: 40, 36, 32 and 28 farther out, 28 for the sky
(`--diff-psnr=40,36,32,28,28`), so a change on the far ridge may be coarser than one at the
skis before it fails. A band under 64 pixels is reported and not judged; a picture with no
distance map is held to the nearest floor. `--diff` exits 1 when a band is under its floor;
`--compare` only prints it. Under `previews/<id>/diff/` it writes `diff.json` and a sheet a shot:

| Panel | Shows |
| --- | --- |
| BEFORE, AFTER | the two pictures |
| CHANGE | the after dimmed, every pixel changed by more than 8 levels coloured blue to yellow to red by how much, the bands' edges as contour lines |
| DISTANCE BANDS | each band in its own colour over the picture, its edges drawn and its name (in metres) at its middle |

with every band's numbers and verdict in the caption band under them. Two runs of the same code
on the same machine give the same bits (PSNR 100 everywhere), so any number under 100 is the
change. The `world` lab hands a distance map with every view; a GPU lab whose pictures guard a
look should do the same.

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
| `<shot>.json` | The shot's words beside the picture, with the picture's own size (the band left out). |
| `<shot>.distance` | How far away each pixel is, when the lab handed a distance map: `GEDIST1\n`, the width and height as little-endian u32, then the metres as f32 (infinity for the sky). Never in the digest. |
| `diff/` | With `--compare` or `--diff`: a sheet a shot and `diff.json` (above). |
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
- **`--check-digests=tests/digests/`** compares each lab's smoke-run digest with
  the committed baseline. CTest's `labs_smoke_1`..`_8` (one slice of the labs each,
  `--shard=I/N`) run it on every CI row, so a lab whose
  output differs between compilers fails on the row that differs.
- A lab that has to move (its drawing changed, or the code it runs changed on purpose)
  rewrites its file of the baseline with `ge digest <id> --write` (`make ge ARGS=...` here; the
  runner's `--write-digests` on one lab or a shard leaves the other labs' files), and the PR says why.
  A game repo keeps the same baseline once `ge digest --write` writes it, and its
  `labs_smoke` case checks it from then on.
- The baseline is one file a lab (`tests/digests/<lab>.txt`, the digest's 16 hex digits):
  two branches that add or move different labs touch different files and merge cleanly, in the
  merge queue too, which ignores custom merge drivers. Two branches that move the same lab
  conflict on its one file; keep the digest `ge digest <id> --write` writes on the merged tree.
  A runner path ending in `.txt` is still read and written as the older single file of rows.
- After a change, `ge digest --diff` runs every lab at its smoke flags and prints only the labs
  whose digest is off the baseline (or not in it), writing nothing: the list of rows the PR has
  to explain or fix, in one command (about 14 minutes for every lab in the debug build; name a
  lab to run only it).

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

**Step by step, `--trace`.** Every scenario lab (`ride`, `sled`, `heli`, `para`) takes `--trace`:
each scenario it runs also writes its every step as a series file, `<id>.trace.tsv` (the state,
the controls and the events: `ge graph` reads it, and two runs' files chart on one graph), and
draws it as captioned charts (`<id>.<chart>.png` and `.svg`: the height, the speeds, the angles,
the controls), its events marked as grey lines and its flights shaded, spikes ringed. The
runner hands each step to the lab through an optional callback (`run_para_scenario(sc, hz,
each)`), so the plugin needs nothing of the lab kit; the lab writes and draws through
`engine/lab/series_charts.hpp`, the same helper the `trace` lab draws a seed's run with. It is
the scenario's before and after at the scale of a step: `--trace --tag=before`, change the code,
`--trace --compare=before`, then `ge graph` both files.

Under `--trace` each scenario also reports its **teleports** (`engine/lab/teleport.hpp`): every
step where the actor moved farther than its speed carries it over the step (with a slack of 1.5
and 5 cm), as `<id>.teleports` and a note a jump naming its step, its time and the two distances.
A lab that follows anything that moves gets the same check from `find_teleports` and
`report_teleports`, which can fail the lab when its subject must never jump.

```sh
make lab LAB=para ARGS="--scenario=launch --trace"   # previews/para/launch.trace.tsv and its charts
```

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
- **An agent at the controls**: every game is played the same way, through the engine's
  play contract (`engine/play/game.hpp`). A game names its axes (the whole input: Fall Line's
  `steer`, `tuck`, `brake`, `jump`, `reset`) and options (`rivals`, `version`) and opens a
  `Run` (step, over, digest, observe, state, report, pilot). The engine's session
  (`engine/play/session.hpp`) snaps every input onto its axis's grid, records it on the INPUT
  LOG (`engine/play/inputs.hpp`: plain text, one line per run of held steps) and steps the run,
  so a run played by an agent, by the game's pilot or from a log is written down the same way
  and replays to the same digest. The game's `play` executable (`build/<preset>/play`) carries
  it, and `ge` drives it:

  | Command | What |
  | --- | --- |
  | `ge play --seed=38 -- --rivals=3` | the line protocol on the terminal (below) |
  | `ge sim --seeds=38,7` | the game's pilot over each seed: report, digest, `runs/<game>-<seed>.inputs` |
  | `ge sim --human` | the same, with how human each run looks (`engine/play/human.hpp`): each axis's saturation, jumps, reversals, hold and lag, each decision's changes of mind, a warning for every one outside a person's band (skill: bots) |
  | `ge replay <log>` | the log played back; a digest it misses is a determinism break |
  | `ge state <log> --step=N` | everything that decides the next step, after N steps |
  | `ge diverge <a> <b>` | the first input and the first step two runs part, and the fields that differ |
  | `ge why <log> --step=N [--around=K] [--flips] [--timeline]` | why a bot did it: the log replayed with the decision trace on (`engine/play/why.hpp`), agent N's decisions round the step (what it saw, the options it weighed with their scores and terms, the choice and its margin); on a log a person or an agent played, what the pilot would have done instead and the first step it would have played otherwise; `--flips` every change of mind; `--timeline` a series file for `ge graph` (skill: bots) |
  | `ge shot <log> --step=N --view=V` | a still of the run at step N: the game's view drawn on the CPU raster, framed with its caption, the seed, the options, the step and the digest (`runs/<game>-<seed>-<view>-<step>.png`); without a log, the pilot's run of `--seed`. Fall Line's views: `near` (the piste 160 m round the player, the field on it) and `map` (the whole map) |
| `ge shot --gpu [--view=V] [--mode=M] [--size=WxH]` | the same still drawn by the game's GPU renderer (a play executable built with the GPU registers one: `RendererSpec`, `GE_RENDERER`), in one of its modes. Fall Line's views: `chase` (the default), `high`, and the named views `start`, `piste`, `lookback`, `vista`, `aerial`; its modes: `lit` (the game's picture), `wireframe` (the clipmap's tiles banded by distance, the trees and figures orange, hidden lines hidden), `depth` (near white, 4 km black on a log scale) and `overlay` (the lit picture with the piste's line, the gates where the ground shows them and each racer's velocity, number and speed) |
| `ge record --gpu [--view=V] [--mode=M]` | the run recorded through the GPU renderer: one renderer for the whole recording, the first frame a cut and every frame after drawn on from the last, so the tracks, the spray and TAA's history carry; the files are named by the view and the mode (`runs/<game>-<seed>-chase-overlay.mp4`) |

  The protocol is stated once in `engine/play/session.hpp`. The session opens with `hello`
  (the game, the seed and options, the step rate, the axes, the ops); then:

  | Agent sends | Engine answers |
  | --- | --- |
  | `{"op":"observe"}` | the step and the clock, and the game's observation. Fall Line's: where he is, his heading, speed and edge, on the snow or in the air, down or not; the gate owed, its bearing off his heading (+ right) and its distance; where he stands on the piste (`piste_lateral`, `piste_width`, `piste_heading`, `piste_bend` over the next 30 m); finished and the finish time; his place in the field; his events since the last answer |
  | `{"op":"act","input":{"<axis>":v,...},"steps":n}` | the observation after n steps (1 to 1200) with that input held; an absent axis is 0, a `press` axis (`reset`) is taken on the first step |
  | `{"op":"pilot","steps":n}` | the game's own pilot plays n steps, then the observation: an agent hands over and takes back |
  | `{"op":"why"}` | what the pilot would do now and why, without stepping: the input it would send and its decision records (`engine/ai/why.hpp`), so an agent can ask before it hands over |
  | `{"op":"state"}` | the game's whole state now, every field to the bit |
  | `{"op":"inputs"}` | the input log so far, as its text: the run as a replay |
  | `{"op":"end"}` | the run's report, steps and digest; the session ends |

  A line the session cannot use answers `{"op":"error","error":...,"next":...}`: what was wrong
  and what to send instead. An agent that sends the bot's moves rides the bot's run to the bit,
  the pilot's run IS the bot's, and an agent's log played back is its run again
  (`tests/fall_line_play_test.cpp`). The `play` lab runs the same session: `--stdio` for the
  protocol, `--script=<file>` to play a file's lines, and with neither the bot plays through
  the protocol's text and its log is replayed to the digest. Two logs one input apart
  `ge diverge` at exactly that step, naming the fields that differ (the skier's yaw rate first).
  The protocol carries only what a player could know, so an agent's playtest is a player's run.

## The labs

A lab is listed in one place: its `LabSpec`, beside its subject (id, kind, owner, subject, skill
and the question it answers). Nothing else keeps a list, so pull requests adding different labs
never touch the same file. `make index` (and `make agents-check`) write the whole list to
`docs/lab-list.md`, generated in every checkout and never committed, as `docs/index.md` is: each
lab with its kind, owner, subject and skill, the roadmap rows whose labs name it, and
the file that registers it (`tools/check-labs.mjs`, which also holds every spec readable off its
source). The built runner says the same: `make labs`, or `labs --json`. A lab a row promises
before it is built is named in that row's labs only.
