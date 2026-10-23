---
type: Added
title: The scenario labs trace every step with --trace
---

The `ride`, `sled`, `heli` and `para` labs take `--trace`. With it, each scenario also writes its every step as `<id>.trace.tsv`: the state, the controls and the events, for `ge graph`. It also draws captioned charts of the run (the height, the speeds, the angles, the controls), with the events as marks and the flights shaded.

The scenario runners take an optional per-step callback, so a game's own scenario lab can trace its runs the same way. `ge::lab::chart_series` (`lab/series_charts.hpp`) attaches a series and draws its charts in one call.
