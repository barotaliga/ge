---
title: A runner's per-step hook keeps the plugin off the lab kit
date: 2026-10-23
scope: plugins
concepts: [scenario, trace, per-step, callback, series, plugin-dependencies]
---

To trace a scenario every step, do not make the plugin's runner build a `ge::lab::Series`: the
plugin's library does not link `ge_lab`, and the tests that run the same rows do not want it. Give
the runner an optional `std::function<void(const XStep&)>` called after each step with references
to the state, the controls and the events, and let the lab fill the series and draw it through
`lab/series_charts.hpp`. When the hook is empty the run is the run it was, so the scenarios'
digests and goldens do not move. Check that the step resets its events struct (`ev = XEvents{}`)
before you mark each event, or a mark repeats every step after it.
