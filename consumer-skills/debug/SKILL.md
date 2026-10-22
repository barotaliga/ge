---
name: debug
description: "Use to find out WHY a game behaves as it does, in a game repo: switch on the engine's log channels for a run and read the dump, log on a channel of the game's own, write a per-step series trace, and chart traces, benches and profiles with ge graph (PNG to look at, SVG to read, every outlier a finding)."
---

# Debugging: logs, traces and charts

Look before you change anything. A run says what it did in three forms, and each has one tool:

| Form | What | How |
| --- | --- | --- |
| **The log** | Lines by step on named channels: events, decisions, refusals | `--log=<spec>` on any lab; `log.txt` in its previews |
| **A trace** | One row a step: the numbers you are chasing, as a series file | a lab writes it (`lab/series.hpp`) |
| **A chart** | A trace, a bench or a profile drawn, outliers ringed | `ge graph <file>...` |

## The log

Every line goes through the engine's one channel (`core/output.hpp`). Log on a channel of your
own, at the level the line deserves, and check before you format a line that is usually off:

```cpp
#include "core/output.hpp"
namespace out = ge::core::output;

static const out::Channel kWind = out::channel("wind");    // once, keep the handle
if (out::enabled(kWind, out::Level::debug)) {
  out::log_on(kWind, out::Level::debug, "gust " + num::to_fixed(g, 2) + " m/s at gate 3");
}
```

Levels, loudest kept first: `error`, `warn`, `info`/`status`, `debug` (an event worth a line),
`trace` (every step). Every channel keeps up to `info` unless told otherwise. Call
`out::set_step(state.steps)` at the top of your step so every line carries its step.

Switch them on for a run with a LOG SPEC: `name=verbosity` pairs, `*` for the rest:

```sh
ge lab run -- --log=wind=trace,*=warn     # previews/run/log.txt: "[1234] debug wind: gust ..."
grep "debug wind" previews/run/log.txt | head
```

Debug and trace lines go to `log.txt` only, so a firehose never floods the terminal. The step in
each line is the one `ge state --step=N` and `ge diverge` speak of.

## Traces

A SERIES file is tab-separated text: `# key: value` lines (`# x: t_s`, `# unit.speed: km/h`),
a header, one row a step. Build one with `lab::make_series` and `lab::add_row`, and attach it from
a lab with `lab.attach("trace.tsv", bytes)`. A SPANS file (`name depth start_ns dur_ns`) is a
timeline: what `ge profile` writes.

## Charts

```sh
ge graph previews/run/trace.tsv                       # one chart a column, spikes ringed
ge graph before/trace.tsv after/trace.tsv --columns=speed   # overlaid: before and after
ge graph trace.tsv --kind=hist|box|heat|scatter       # distributions, percentiles, a grid
ge graph zones.tsv                                    # spans: a timeline and zone boxes
```

Each chart is a PNG (look at it) and an SVG beside it (its labels, values and rings are text).
Every ringed value comes back as a `note` finding, strongest first, with where to look. In a lab,
draw the same charts with `lab/chart.hpp` (`line_chart`, `histogram_chart`, `box_chart`,
`heatmap_chart`, `span_chart`) and `lab.shot(..., lab::to_picture(chart.plot))`.

## Reading what comes back

- **A ring on a line** is a SPIKE: one step out of line with its neighbours. Read the log at
  that step (`grep "^\[<step>\]" log.txt`) and `ge state --step=<step>`.
- **A ring on a histogram or a box** is an OUTLIER of the whole set: a frame, a step or a run
  unlike the rest. Find its row in the file.
- **Many rings on one line** is a noisy signal, not many bugs: the worst are ringed first.
- **Two files, two lines**: the gap between before and after is the change's effect.

## Animations

Judge a moving figure on a POSE TRACK (`lab/anim.hpp`), never by eye on the game alone. Record
its joints every step in its own frame (x right, y up, z forward, the ground at `floor`), with its
pieces' boxes as a wireframe, then:

```cpp
lab::PoseTrack track;                     // track.rig: joints, parents, sides, end joints
track.add_frame(joints, wire);            // every step, from your figure's pose
const lab::Motion m = lab::measure_motion(track);
for (const lab::AnimFinding& f : lab::anim_findings(track, m)) lab.note(f.note);
lab.shot({...}, lab::filmstrip(track, {lab::AnimView::side, lab::spread_frames(track, 16)}));
```

- **Filmstrips** (side, behind, above) show the frames, findings ringed red; **onion skins** show
  the hands' and feet's paths; `angle_chart` and `jolt_heatmap` are charts like any other.
- The faults: `pop` (a joint jumps in one frame), `snap` (its acceleration jerks), `angle-snap`
  (a joint turns too fast), `stretch` (a bone changes length), `clip` (a joint under the ground).
- Plant a pop, a stretch and a clip in a COPY of the track and fail the lab if one is missed, so
  the measures stay honest; report the real track's findings and compare them before and after.
