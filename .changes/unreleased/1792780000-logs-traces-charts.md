---
type: Added
title: Log channels, series traces, charts and ge graph
breaking: true
---

Debugging a run is built in: subsystems and games log on named channels (`core::output::channel`, `log_on`, a `trace` level) that a log spec switches on (`configure("skier=trace,*=warn")`, `--log=` on any lab, which dumps `log.txt` with the step on every line); every measurement over time is one series or spans text file (`lab/series.hpp`); the chart kit (`lab/chart.hpp`, `lab/plot.hpp`) draws line, scatter, histogram, box, heatmap and flame charts as PNG and SVG with outliers and spikes ringed by one robust rule (`lab/stats.hpp`); and `ge graph <file>...` charts any trace, bench or profile, overlaying several files, with every outlier returned as a finding. New consumer skill: `debug`. Breaking: an output sink now receives the whole `LogLine` (level, channel, step, message) instead of a level and a message.
