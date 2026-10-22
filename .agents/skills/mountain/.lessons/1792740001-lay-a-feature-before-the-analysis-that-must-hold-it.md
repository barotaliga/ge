---
title: Lay a feature that reshapes the ground before the analysis that must hold the map
date: 2026-10-22
scope: plugins/mountain/mapgen/generate.cpp
concepts: [ski-route, route-jump, attempt, analysis, felled-trees]
---

The ski routes could have been laid on the accepted ski area, after its analysis, as a pure read
of the finished mountain. But a wild route fells trunks and cuts jumps and gaps into the ground,
so a map laid that way is a map no analysis ever read. Laid on the attempt's base map, after
`resort_base` and before `analyze_resort`, the routes ride into every course set on that base
(the jumps through `ResortAttempt::route_kickers`) and the whole rule book holds the stamped
ground. Then sweep: a feature that may refuse attempts should be seen not to, by comparing the
attempts a sweep takes with and without it (v8 against v10 here).
