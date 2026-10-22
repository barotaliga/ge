---
title: A real face bends the rule book through numbers beside the dealt rule, not a second path
date: 2026-10-22
scope: plugins/mountain/mapgen/massif.cpp
concepts: [real-face, rule-bend, resort-rules, face-lift, attempts, sweep]
---

Every place a real mountain cannot meet a dealt massif's rule (a gentler valley floor, real
knolls a run is cut through, a crowded top, a station on a slope) takes the real face's number
from `resort_rules::massif::real::least` where the dealt rule is read, through a small struct or
flag (`GradeFace`, `RampFit`, `ChainMost`, `StationGround::level`, `slack`), and the analysis
holds the map to the same number by `Level::face`. Read every stage that applies a dealt rule
for whether a real face needs its own number before assuming it needs none. Adding a pointer to the plan or the attempt (`TerrainPlan::face`,
`ResortAttempt::face`) also breaks every aggregate initialiser of the struct under
`-Wmissing-field-initializers`. Check that no dealt map moved with the mountain tests and the
labs' smoke digests before reading any face. Then sweep every face with the `real-maps` lab
split over processes (one face takes 3 to 10 s): a face that stops building is a regression even
when the two faces a test builds still stand.
