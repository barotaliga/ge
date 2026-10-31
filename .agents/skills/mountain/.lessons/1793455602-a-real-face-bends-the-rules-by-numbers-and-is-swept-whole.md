---
title: A real face bends the rule book through numbers beside the dealt rule, and every claim about it is a sweep over every face
date: 2026-10-22
scope: plugins/mountain/mapgen/massif.cpp, plugins/mountain/mapgen/real_lean.cpp
concepts: [real-face, rule-bend, resort-rules, real-hints, lean, off-real, real-maps, sweep]
---

- Where a real mountain cannot meet a dealt rule (a gentler valley floor, a crowded top, a station
  on a slope), the stage reads the real face's number from `resort_rules::massif::real::least`
  through a small struct or flag (`GradeFace`, `RampFit`, `ChainMost`, `StationGround::level`,
  `slack`), and the analysis holds the map to it by `Level::face`. Read every stage that applies a
  dealt rule before assuming a face needs no number of its own. A pointer added to the plan or the
  attempt breaks every aggregate initialiser under `-Wmissing-field-initializers`.
- Check that no dealt map moved (the mountain tests, the labs' smoke digests) before reading a face.
- One face's picture says nothing. Sweep `real-maps` over every face, split over processes (3 to
  10 s a face): a face that stops building is a regression. For the hints' lean, sweep twice,
  `--hints=1` and `--hints=0`, and compare `off real m` face by face, then over the faces whose crop
  holds the real area; a lean that helps some moves others away.
- Name a generated file's namespace apart from its module's functions (`hints_baked`, not
  `real_hints`, the decoder).
