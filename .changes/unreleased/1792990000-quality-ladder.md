---
type: Added
title: The quality ladder and its automatic preset
---

`render/ladder.hpp` fits a picture to a frame budget: a game states its rows (each a ladder of stops, cheapest first, with a measured cost and a judged worth), and `fit_ladder` returns the picture of most worth that fits, solved exactly as a knapsack with a choice in each row. Rows marked to be given up first go first, in their order. `machine_scale` reads a machine as the table's reference made faster or slower from one timed frame. Fall Line's ladder (`games/fall_line/app/ladder.hpp`) has eight rows on its frame's options: resolution, shadows, snow, trails, spray, snowfall, clouds and bloom. It has four presets (low, medium, high, top) and `auto_picture`. The cloud march takes a share of its steps (`Clouds::steps`, `FrameOptions::clouds`), and with no share draws no layer. The new `ladder` lab checks the table and sweeps the fit; with `--price` it times every stop on the adapter and shoots the presets. A test that includes a game's presentation half (`<game>/app/`) now links it.
