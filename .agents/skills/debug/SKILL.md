---
name: debug
description: "Use to find out WHY the engine or a game behaves as it does, and for the debugging tools themselves: the log channels and the log spec (core/output), log dumps from labs (--log), per-step series traces and spans files (engine/lab/series.hpp), the statistics that call a value an outlier or a spike (stats.hpp), the chart kit (plot.hpp, chart.hpp: line, scatter, histogram, box, heatmap, flame timeline, PNG and SVG), ge graph, the animation views (anim.hpp: a figure's pose track, filmstrips, onion skins, joint angles, jolt, pops, snaps, stretch and clips), and the charts, trace and anim labs. Load it before adding a printf, a one-off dump or a chart of your own."
---

# Debugging: logs, traces and charts

An agent finds a problem by reading what a run did and LOOKING at it, never by guessing. The
engine gives every run three forms, each with one owner, and `docs/debugging.md` is the design:

| Form | Owner | How a reader gets it |
| --- | --- | --- |
| The log: lines by step on named channels | `engine/core/output.hpp` | `labs <id> --log=<spec>` writes `previews/<id>/log.txt` |
| A trace: one row a step (or a frame, a sample) | `engine/lab/series.hpp` | a lab attaches it (`trace.tsv`); `ge bench` and `ge profile` write it |
| A chart: a trace drawn, outliers ringed | `engine/lab/chart.hpp`, `ge graph` | a lab's shot (PNG) with its SVG; `ge graph <file>...` |

**Read this skill's lessons first**: `make lessons ARGS=debug`. Load `labs` beside it for a lab,
`agent-interface` for `ge graph`.

## The log

- A subsystem or a game logs on its OWN channel: `static const Channel ch = channel("course");`
  once, then `log_on(ch, Level::debug, ...)`. The plain helpers (`status`, `warn`, ...) are the
  `engine` channel.
- **A dropped line must cost nothing**: guard formatting with `enabled(ch, level)`. A step
  allocates nothing (`write-code`), and a log line is the one place that rule is kept by the
  guard. `games/fall_line/sim/game.cpp`'s `log_step` is the pattern.
- Levels: `debug` is an event worth a line (a gate, a landing, a refusal); `trace` is every step
  (the racers' state). Errors and warnings stay diagnostics: what, where, next (`agent-interface`).
- A game's `step` calls `set_step(state.steps)` first, so every line carries its step and lines
  up with a trace row, a replay and `ge state`.
- `configure(spec)` is the only way verbosity is set (`--log=` on the labs runner reads it);
  `set_debug_enabled` is `*=debug` that leaves named channels alone.

## Traces and spans

- One format for everything over time: tab-separated, `# key: value` meta (`x`, `unit.<col>`,
  `title`), a header, numeric rows. Text columns are allowed (a spans file's `name`).
- Numbers are written shortest round-trip, so a file read back is the same doubles; a trace of a
  deterministic run is deterministic and may go in a lab's digest.
- Name columns in snake_case with the unit in `# unit.<col>`, so `ge graph` labels the axis.
- Agreed with the profiling tools: `ge bench`, `ge profile` and `ge mem` write SERIES (wide: one
  column per zone or counter) and SPANS (`name depth start_ns dur_ns`). Read and write them only
  through `series.hpp`.

## Charts

- Draw with `chart.hpp`, never by hand on the raster: every chart then shares the look (one
  surface, the categorical order, one y axis, a legend from two series) and the outlier rule.
- One chart, one unit: two measures of different scale are two charts, never two axes.
- Put the run's inputs in the subtitle and the shot's `inputs`, and say in `look_for` what a
  correct chart shows ("one swing a turn; rings only at landings").
- Attach the SVG beside the PNG (`lab.attach(id + ".svg", ...)`): the SVG's text is how an agent
  reads exact values off a chart.
- `chart_text(chart, n)` is the chart in words: put it in a lab note or a command's text.

## The outlier rule (stats.hpp)

- A SPIKE (line charts) is a value out of line with its rolling-median neighbours by a robust z
  over 3.5, the scale floored at 1% of the series' p1..p99 range so a smooth 120 Hz signal flags
  nothing, and at a millionth of its largest value so a flat series' rounding flags nothing. An OUTLIER (sets) is the same against the whole set's median and MAD.
- Findings come strongest first and at most 60 are ringed: a noisy signal rings its worst, and
  "thousands of spikes" means a noisy signal, not thousands of bugs.
- Tune the rule in `stats.cpp` only, and rerun `make lab LAB=charts`: its planted outliers must
  each be ringed exactly once.

## Animations (anim.hpp)

- A figure is judged on a POSE TRACK: its joints every frame in its OWN frame (x right, y up, z
  forward, the ground at `floor`), never the world's, so the track is the animation and not the
  figure travelling. The figure hands out its joints and its frame (the skier's:
  `figure_frame`, `figure_joints`, `figure_joint_positions` in `plugins/skier/app/figure.hpp`).
- Record EVERY step, not every tenth: a pop is one frame.
- The faults are `pop`, `snap`, `angle-snap`, `stretch` and `clip` (`docs/debugging.md` has the
  table). Tune the limits in `AnimLimits`, the rule in `anim.cpp`, and rerun `make lab LAB=anim`:
  its planted pop, stretch and clip must each be found.
- A figure's lab plants the same three faults in a COPY of its track and fails when one is
  missed; the real track's findings are reported, never failed on, and read before and after a
  change (`--tag=before`, `--compare=before`).
- Look at the filmstrips first (the frame), then the onion skin (the paths), then the charts (the
  numbers): a finding's frame is in its sentence and ringed in the strip.

## The labs

| Lab | Question |
| --- | --- |
| `charts` | Does every chart form draw, read, and ring exactly the outliers planted in its data? |
| `trace` | What did the player's skier do, step by step, down one seed, and where did it jump? |
| `anim` | Does the skier's figure move without pops, snaps, stretched bones or poles through the snow, and are planted faults found? |

`make lab LAB=trace ARGS="--seed=38 --log=course=debug,skier=debug"` is the first thing to run
for any question about a run: the charts say where, the log says what happened there.
