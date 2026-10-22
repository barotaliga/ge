---
title: Measure how near a real face's runs lie with the hints off beside them, over every face
date: 2026-10-22
scope: plugins/mountain/mapgen/real_lean.cpp
concepts: [real-hints, lean, off-real, real-maps, sweep, crop]
---

Leaning the generator on a real ski area moves its runs only as far as the walk's centre term
lets it, and a face's crop often holds only a corner of the real area (on the fells the real
pistes can lie more than a kilometre off the generated ones whatever the lean does). One face's
picture says nothing about the effect: sweep `real-maps` over every face twice, `--hints=1` and
`--hints=0` (four processes, about six minutes each), and compare the `off real m` column face by
face, then again over the faces whose crop holds the real area (hints off under 200 m). A lean
that helps some faces moves others the other way. Bake the hints into the tree before building
anything that reads them, and name a generated file's namespace apart from any function of the
same module (`hints_baked`, not `real_hints`, which is the decoder).
