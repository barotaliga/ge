---
title: A stand-off at one distance leaves the men behind the line climbing in place
date: 2026-10-22
scope: games/fall_line/sim/army.cpp, games/fall_line/sim/army_troop.cpp, games/fall_line/labs/army_lab.cpp
concepts: [army, stand-off, hold, keep-apart, stuck, column, lab, quarry]
---

Soldiers who stop at exactly `kStandOffM` form a line. The men of the same column behind them
are kept apart from it, so they push at the line 1 to 2 m short of the stand-off forever,
"climbing" at full speed and going nowhere. The side-step out of a stuck window is too short to
walk round a line 40 m wide. The fix is a band: a man who sees him holds within `kStandOffM`,
or within `kHoldBandM` further once his watch says he is caught. The column leaves for the fight
at the band's far edge. Check the end state's modes, not the nearest man: `nearest` looked fine
while a quarter of the wave was still pushing.

In a lab on a real slope, a skier stood at rest slides away under gravity. Holding
`progress.stuck` at 0 only stops the run's reset; to keep the quarry still, stand him again
every step. Otherwise the army chases a man who is 1.4 km away by the end.
